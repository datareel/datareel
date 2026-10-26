// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: strutil.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
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

String helpers for std::string that the C++17 standard library
does not provide: trimming, ASCII case conversion, case-insensitive
compare and search, replace-all, splitting, printf-style formatting
and non-throwing number conversion.

Changes:
==============================================================
10/26/2026: DataReel 7 replaces the UString/gxString classes and
the null-terminated string functions with std::string and these
header-only helpers in the datareel namespace.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_STRUTIL_HPP__
#define __GX_STRUTIL_HPP__

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace datareel {

inline bool is_space(char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
    c == '\f' || c == '\v';
}

inline char to_upper(char c)
{
  return (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
}

inline char to_lower(char c)
{
  return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
}

inline std::string to_upper(std::string_view s)
{
  std::string r(s);
  for(auto &c : r) c = to_upper(c);
  return r;
}

inline std::string to_lower(std::string_view s)
{
  std::string r(s);
  for(auto &c : r) c = to_lower(c);
  return r;
}

inline std::string_view trim_left_view(std::string_view s)
{
  std::size_t i = 0;
  while(i < s.size() && is_space(s[i])) i++;
  return s.substr(i);
}

inline std::string_view trim_right_view(std::string_view s)
{
  std::size_t n = s.size();
  while(n > 0 && is_space(s[n-1])) n--;
  return s.substr(0, n);
}

inline std::string trim_left(std::string_view s)
{
  return std::string(trim_left_view(s));
}

inline std::string trim_right(std::string_view s)
{
  return std::string(trim_right_view(s));
}

inline std::string trim(std::string_view s)
{
  return std::string(trim_right_view(trim_left_view(s)));
}

// Case-insensitive (ASCII) equality
inline bool iequals(std::string_view a, std::string_view b)
{
  if(a.size() != b.size()) return false;
  for(std::size_t i = 0; i < a.size(); i++) {
    if(to_lower(a[i]) != to_lower(b[i])) return false;
  }
  return true;
}

// Case-insensitive (ASCII) find, returns std::string::npos if not found
inline std::size_t ifind(std::string_view s, std::string_view p,
			 std::size_t pos = 0)
{
  if(p.empty()) return pos <= s.size() ? pos : std::string::npos;
  if(p.size() > s.size()) return std::string::npos;
  for(std::size_t i = pos; i + p.size() <= s.size(); i++) {
    if(iequals(s.substr(i, p.size()), p)) return i;
  }
  return std::string::npos;
}

inline bool starts_with(std::string_view s, std::string_view p)
{
  return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

inline bool ends_with(std::string_view s, std::string_view p)
{
  return s.size() >= p.size() &&
    s.compare(s.size() - p.size(), p.size(), p) == 0;
}

inline bool istarts_with(std::string_view s, std::string_view p)
{
  return s.size() >= p.size() && iequals(s.substr(0, p.size()), p);
}

// Replace every occurrence of from with to, returns the count
inline std::size_t replace_all(std::string &s, std::string_view from,
			       std::string_view to)
{
  if(from.empty()) return 0;
  std::size_t count = 0, pos = 0;
  while((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
    count++;
  }
  return count;
}

// Remove every occurrence of c
inline void erase_char(std::string &s, char c)
{
  std::string r;
  r.reserve(s.size());
  for(char ch : s) if(ch != c) r += ch;
  s.swap(r);
}

// Split on sep. Empty fields are kept unless skip_empty is true.
inline std::vector<std::string> split(std::string_view s, char sep,
				      bool skip_empty = false)
{
  std::vector<std::string> out;
  std::size_t start = 0;
  for(;;) {
    std::size_t pos = s.find(sep, start);
    std::string_view field = s.substr(start, pos == std::string::npos ?
				      std::string::npos : pos - start);
    if(!skip_empty || !field.empty()) out.emplace_back(field);
    if(pos == std::string::npos) break;
    start = pos + 1;
  }
  return out;
}

// printf-style formatting into a std::string
inline std::string vformat(const char *fmt, va_list ap)
{
  va_list ap2;
  va_copy(ap2, ap);
  int n = std::vsnprintf(nullptr, 0, fmt, ap2);
  va_end(ap2);
  if(n <= 0) return std::string();
  std::string r(std::size_t(n) + 1, '\0');
  std::vsnprintf(&r[0], r.size(), fmt, ap);
  r.resize(std::size_t(n));
  return r;
}

#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
inline std::string format(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  std::string r = vformat(fmt, ap);
  va_end(ap);
  return r;
}

// Non-throwing conversions with atoi() semantics: leading spaces are
// skipped, conversion stops at the first invalid character, 0 on error
inline int to_int(const std::string &s) { return std::atoi(s.c_str()); }
inline long to_long(const std::string &s) { return std::atol(s.c_str()); }
inline long long to_llong(const std::string &s)
{
  return std::strtoll(s.c_str(), nullptr, 10);
}
inline unsigned long long to_ullong(const std::string &s)
{
  return std::strtoull(s.c_str(), nullptr, 10);
}
inline double to_double(const std::string &s) { return std::atof(s.c_str()); }

// Copy s into a fixed char buffer, always null terminated, truncating
// when needed. Returns the number of characters copied.
inline std::size_t copy_to(char *dest, std::size_t dest_size,
			   std::string_view s)
{
  if(dest_size == 0) return 0;
  std::size_t n = s.size() < dest_size - 1 ? s.size() : dest_size - 1;
  if(n) s.copy(dest, n);
  dest[n] = '\0';
  return n;
}

// Length of a fixed char buffer that may not be null terminated
inline std::size_t bounded_length(const char *s, std::size_t max)
{
  std::size_t n = 0;
  while(n < max && s[n]) n++;
  return n;
}

// In-place editing helpers for parsing. Each returns true if the
// pattern was found and the string changed.

// Delete everything before the first p, keeping p
inline bool delete_before(std::string &s, std::string_view p)
{
  std::size_t pos = s.find(p);
  if(pos == std::string::npos) return false;
  s.erase(0, pos);
  return true;
}

// Delete everything before the first p, including p
inline bool delete_before_including(std::string &s, std::string_view p)
{
  std::size_t pos = s.find(p);
  if(pos == std::string::npos) return false;
  s.erase(0, pos + p.size());
  return true;
}

// Delete everything before the last p, keeping p
inline bool delete_before_last(std::string &s, std::string_view p)
{
  std::size_t pos = s.rfind(p);
  if(pos == std::string::npos) return false;
  s.erase(0, pos);
  return true;
}

// Delete everything before the last p, including p
inline bool delete_before_last_including(std::string &s, std::string_view p)
{
  std::size_t pos = s.rfind(p);
  if(pos == std::string::npos) return false;
  s.erase(0, pos + p.size());
  return true;
}

// Delete everything after the first p, keeping p
inline bool delete_after(std::string &s, std::string_view p)
{
  std::size_t pos = s.find(p);
  if(pos == std::string::npos) return false;
  s.erase(pos + p.size());
  return true;
}

// Delete everything after the first p, including p
inline bool delete_after_including(std::string &s, std::string_view p)
{
  std::size_t pos = s.find(p);
  if(pos == std::string::npos) return false;
  s.erase(pos);
  return true;
}

// Delete everything after the last p, keeping p
inline bool delete_after_last(std::string &s, std::string_view p)
{
  std::size_t pos = s.rfind(p);
  if(pos == std::string::npos) return false;
  s.erase(pos + p.size());
  return true;
}

// Delete everything after the last p, including p
inline bool delete_after_last_including(std::string &s, std::string_view p)
{
  std::size_t pos = s.rfind(p);
  if(pos == std::string::npos) return false;
  s.erase(pos);
  return true;
}

inline void trim_in_place(std::string &s) { s = trim(s); }
inline void trim_left_in_place(std::string &s) { s = trim_left(s); }
inline void trim_right_in_place(std::string &s) { s = trim_right(s); }

// Convert hex escape sequences such as %41 to characters. A doubled
// escape character is replaced by one. Returns the number converted.
inline int convert_escape_seq(std::string &s, char esc = '%', 
			      int num_octets = 2)
{
  auto hexval = [](char c) -> int {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  std::string r;
  r.reserve(s.size());
  int count = 0;
  for(std::size_t i = 0; i < s.size(); i++) {
    if(s[i] != esc) { r += s[i]; continue; }
    if(i + 1 < s.size() && s[i+1] == esc) { r += esc; i++; continue; }
    int val = 0, n = 0;
    while(n < num_octets && i + 1 + n < s.size()) {
      int h = hexval(s[i + 1 + n]);
      if(h < 0) break;
      val = val * 16 + h;
      n++;
    }
    if(n == 0) { r += s[i]; continue; }
    r += char(val);
    i += std::size_t(n);
    count++;
  }
  s.swap(r);
  return count;
}

// Split a definition string into fields the way the 6.x parse()
// function did: leading spaces are skipped, empty fields between
// separators are kept and a trailing separator adds no field.
inline std::vector<std::string> parse_fields(std::string_view s, char sep)
{
  s = trim_left_view(s);
  std::vector<std::string> out;
  if(s.empty()) return out;
  out = split(s, sep);
  if(!out.empty() && out.back().empty()) out.pop_back();
  return out;
}

// Three-way compare returning -1, 0 or 1
inline int compare3(std::string_view a, std::string_view b)
{
  int r = a.compare(b);
  return (r < 0) ? -1 : (r > 0 ? 1 : 0);
}

// Natural order compare: runs of digits compare by numeric value, so
// "item9" sorts before "item10". Returns -1, 0 or 1.
inline int natural_compare(std::string_view a, std::string_view b, 
			   bool check_case = true)
{
  std::size_t i = 0, j = 0;
  auto digit = [](char c) { return c >= '0' && c <= '9'; };
  while(i < a.size() && j < b.size()) {
    if(digit(a[i]) && digit(b[j])) {
      std::size_t si = i, sj = j;
      while(si < a.size() && a[si] == '0') si++;
      while(sj < b.size() && b[sj] == '0') sj++;
      std::size_t ei = si, ej = sj;
      while(ei < a.size() && digit(a[ei])) ei++;
      while(ej < b.size() && digit(b[ej])) ej++;
      if(ei - si != ej - sj) return (ei - si < ej - sj) ? -1 : 1;
      int r = a.substr(si, ei - si).compare(b.substr(sj, ej - sj));
      if(r != 0) return r < 0 ? -1 : 1;
      i = ei; j = ej;
      continue;
    }
    char ca = check_case ? a[i] : to_upper(a[i]);
    char cb = check_case ? b[j] : to_upper(b[j]);
    if(ca != cb) return ((unsigned char)ca < (unsigned char)cb) ? -1 : 1;
    i++; j++;
  }
  if(i < a.size()) return 1;
  if(j < b.size()) return -1;
  return 0;
}

// Byte-wise three-way compare of two buffers
template<class Buf>
inline int buffer_compare(const Buf &a, const Buf &b)
{
  if(a < b) return -1;
  if(b < a) return 1;
  return 0;
}

// Byte pattern search, returns std::string::npos if not found
template<class Buf>
inline std::size_t buffer_find(const Buf &haystack, const Buf &needle)
{
  if(needle.empty()) return 0;
  if(needle.size() > haystack.size()) return std::string::npos;
  for(std::size_t i = 0; i + needle.size() <= haystack.size(); i++) {
    bool match = true;
    for(std::size_t k = 0; k < needle.size(); k++) {
      if(haystack[i+k] != needle[k]) { match = false; break; }
    }
    if(match) return i;
  }
  return std::string::npos;
}

// Append each argument to s: strings and characters as text, numbers
// in decimal. str_append(s, "id=", 42, ',') replaces the 6.x
// gxString << chains.
inline void str_append_one(std::string &s, const std::string &v) { s += v; }
inline void str_append_one(std::string &s, std::string_view v) { s += v; }
inline void str_append_one(std::string &s, const char *v) { if(v) s += v; }
inline void str_append_one(std::string &s, char *v) { if(v) s += v; }
inline void str_append_one(std::string &s, char v) { s += v; }
inline void str_append_one(std::string &s, unsigned char v) { s += char(v); }
inline void str_append_one(std::string &s, bool v) { s += v ? '1' : '0'; }
template<class T>
inline auto str_append_one(std::string &s, T v) 
  -> std::enable_if_t<std::is_arithmetic<T>::value> 
{ 
  s += std::to_string(v); 
}

// Class types that convert to an integer, such as the gxINT32 and
// gxUINT64 portable integer classes, are written as numbers
template<class T>
inline auto str_append_one(std::string &s, const T &v)
  -> std::enable_if_t<std::is_class<T>::value &&
		      !std::is_convertible<T, std::string_view>::value &&
		      std::is_convertible<T, long long>::value>
{
  s += std::to_string((long long)v);
}

template<class... Args>
inline std::string &str_append(std::string &s, const Args &...args)
{
  (str_append_one(s, args), ...);
  return s;
}

template<class... Args>
inline std::string str_cat(const Args &...args)
{
  std::string s;
  (str_append_one(s, args), ...);
  return s;
}

} // namespace datareel

#endif // __GX_STRUTIL_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
