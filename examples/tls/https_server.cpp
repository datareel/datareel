// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: https_server.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  
USA

Small HTTPS server. Serves files from a directory, one thread per
connection from a datareel::ThreadPool. Requests for paths that
leave the directory are refused.

Usage: https_server port cert.pem key.pem document_root
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

static void serve(std::shared_ptr<gxsTLSConnection> conn, fs::path root)
{
  gxsHTTPMessage req, resp;
  if(gxsHTTPReadRequest(*conn, req, 1024*1024) != 0) {
    conn->Close();
    return;
  }
  resp.SetHeader("Server", "DataReel/7");
  resp.SetHeader("Connection", "close");

  std::string path = req.target.substr(0, req.target.find('?'));
  path = gxsURL::Decode(path);
  if(path.empty() || path.back() == '/') path += "index.html";
  fs::path file = (root / fs::path(path).relative_path()).lexically_normal();
  std::string rel = file.lexically_relative(root).string();

  if(req.method != "GET" && req.method != "HEAD") {
    resp.status = 405;
    resp.SetHeader("Allow", "GET, HEAD");
  }
  else if(rel.empty() || rel.compare(0, 2, "..") == 0) {
    resp.status = 403; // Outside the document root
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
  std::cout << req.method << " " << req.target << " " << resp.status << std::endl;
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
  fs::path root = fs::absolute(argv[4]).lexically_normal();
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
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
