// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: https_server.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 11/01/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Small HTTPS server. Serves files from a directory, one thread per
connection from a datareel::ThreadPool. Requests for paths that
leave the directory are refused.

Usage: https_server port cert.pem key.pem document_root

11/01/2026: Request paths are mapped by map_request_path(), which
refuses decoded NUL and control characters, backslashes, segments
starting with '.' (.., .git, .htpasswd), and on Windows device names
(CON, NUL, COM1, ...) and ':' in a segment. The mapped file is
resolved with std::filesystem::weakly_canonical() and must stay under
the canonical document root, so a symbolic link cannot leave it.
Request lines are escaped before they are logged. Define
HTTPS_SERVER_NO_MAIN to include this file in a test.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include "gxshttp.h"
#include "gxstls.h"
#include "gxsurl.h"
#include "thrpool.h"

using namespace datareel;
namespace fs = std::filesystem;

static std::string content_type(const fs::path &p)
{
  std::string ext = p.extension().string();
  if(ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
  if(ext == ".txt") return "text/plain; charset=utf-8";
  if(ext == ".css") return "text/css";
  if(ext == ".js") return "text/javascript";
  if(ext == ".json") return "application/json";
  if(ext == ".png") return "image/png";
  if(ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
  return "application/octet-stream";
}

#ifdef _WIN32
static const bool windows_paths = true;
#else
static const bool windows_paths = false;
#endif

// 11/01/2026: Escape control characters and bytes over 0x7E before
// writing request data to the log, so a request cannot forge or
// garble log lines
static std::string log_escape(const std::string &s)
{
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  for(unsigned char c : s) {
    if(c < 0x20 || c >= 0x7F || c == '\\') {
      out += "\\x";
      out += hex[c >> 4];
      out += hex[c & 0x0F];
    }
    else {
      out += char(c);
    }
  }
  return out;
}

// 11/01/2026: Windows device names, which open a device in any folder
// and with any extension (nul.txt)
static bool windows_device_name(const std::string &segment)
{
  std::string base = segment.substr(0, segment.find('.'));
  while(!base.empty() && base.back() == ' ') base.pop_back();
  for(char &c : base) {
    if(c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  }
  if(base == "CON" || base == "PRN" || base == "AUX" || base == "NUL") {
    return true;
  }
  if(base.size() == 4 && (base.compare(0, 3, "COM") == 0 ||
			  base.compare(0, 3, "LPT") == 0) &&
     base[3] >= '1' && base[3] <= '9') return true;
  return false;
}

// 11/01/2026: Map a request target ("/path?query") to a file under
// root, which must be a canonical path. Returns 0 and sets file, or
// the HTTP status to send: 400 for a bad target, 403 for a path that
// is refused or leaves the root.
static int map_request_path(const fs::path &root, const std::string &target,
			    fs::path &file, bool windows_rules = windows_paths)
{
  file.clear();
  if(target.empty() || target[0] != '/') return 400;
  std::string raw = target.substr(0, target.find_first_of("?#"));
  std::string path;
  // NUL and control characters, raw or %-encoded, and bad % escapes
  if(!gxsURL::DecodeStrict(raw, path)) return 400;
  if(path.find('\\') != std::string::npos) return 403;
  if(path.empty() || path.back() == '/') path += "index.html";

  fs::path rel;
  std::size_t start = 0;
  while(start <= path.size()) {
    std::size_t slash = path.find('/', start);
    if(slash == std::string::npos) slash = path.size();
    std::string seg = path.substr(start, slash - start);
    start = slash + 1;
    if(seg.empty()) continue;
    // ".", ".." and dotfiles (.git, .htpasswd, .env)
    if(seg[0] == '.') return 403;
    if(windows_rules) {
      // Drive letters and alternate data streams (file::$DATA),
      // names Windows trims (file.txt. or "file.txt ") and devices
      if(seg.find(':') != std::string::npos) return 403;
      if(seg.back() == '.' || seg.back() == ' ') return 403;
      if(windows_device_name(seg)) return 403;
    }
    try {
      rel /= fs::u8path(seg); // Throws on Windows for bad UTF-8
    }
    catch(...) {
      return 400;
    }
  }
  if(rel.empty()) return 403;
  // Resolve symbolic links and require the result to stay under root
  std::error_code ec;
  fs::path canon = fs::weakly_canonical(root / rel, ec);
  if(ec) return 403;
  fs::path inside = canon.lexically_relative(root);
  if(inside.empty() || *inside.begin() == "..") return 403;
  file = canon;
  return 0;
}

#ifndef HTTPS_SERVER_NO_MAIN
static void serve(std::shared_ptr<gxsTLSConnection> conn, fs::path root)
{
  gxsHTTPMessage req, resp;
  if(gxsHTTPReadRequest(*conn, req, 1024*1024) != 0) {
    conn->Close();
    return;
  }
  resp.SetHeader("Server", "DataReel/7");
  resp.SetHeader("Connection", "close");

  // 11/01/2026: map_request_path() checks the decoded path and
  // resolves it under the canonical root
  fs::path file;
  int map_status = map_request_path(root, req.target, file);

  if(req.method != "GET" && req.method != "HEAD") {
    resp.status = 405;
    resp.SetHeader("Allow", "GET, HEAD");
  }
  else if(map_status != 0) {
    resp.status = map_status; // Bad path or outside the document root
  }
  else {
    std::ifstream in(file, std::ios::in | std::ios::binary);
    if(!in || fs::is_directory(file)) {
      resp.status = 404;
    }
    else {
      std::ostringstream ss;
      ss << in.rdbuf();
      resp.status = 200;
      resp.body = ss.str();
      resp.SetHeader("Content-Type", content_type(file));
    }
  }
  if(resp.status != 200) {
    resp.body = std::to_string(resp.status) + " " + 
      gxsHTTPStatusCodeMessage(resp.status) + "\n";
    resp.SetHeader("Content-Type", "text/plain; charset=utf-8");
  }
  if(req.method == "HEAD") {
    resp.SetHeader("Content-Length", std::to_string(resp.body.size()));
    resp.body.clear();
  }
  // 11/01/2026: Escaped, the request line comes from the client
  std::cout << log_escape(req.method) << " " << log_escape(req.target) 
	    << " " << resp.status << std::endl;
  gxsHTTPWriteResponse(*conn, resp);
  conn->Close();
}

int main(int argc, char **argv)
{
  if(argc < 5) {
    std::cerr << "Usage: " << argv[0] 
	      << " port cert.pem key.pem document_root\n";
    return 1;
  }
  // 11/01/2026: Canonical, so symbolic links in the root itself do
  // not make every file look like it is outside it
  std::error_code ec;
  fs::path root = fs::weakly_canonical(fs::absolute(argv[4]), ec);
  if(ec || !fs::is_directory(root)) {
    std::cerr << "Not a directory: " << argv[4] << "\n";
    return 1;
  }
  gxsTLSServer server;
  if(server.Listen(std::atoi(argv[1]), argv[2], argv[3]) != 0) {
    std::cerr << server.Error() << "\n";
    return 1;
  }
  std::cout << "Serving " << root.string() << " on port " << server.Port() 
	    << std::endl;
  ThreadPool pool(8);
  for(;;) {
    auto conn = std::make_shared<gxsTLSConnection>();
    conn->SetTimeout(30);
    if(server.Accept(*conn) != 0) continue; // Bad handshakes are ignored
    pool.Submit(serve, conn, root);
  }
  return 0;
}
#endif // HTTPS_SERVER_NO_MAIN
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
