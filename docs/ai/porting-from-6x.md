# Porting DataReel 6.x code to DataReel 7

DataReel 7 keeps the database and TLS classes and replaces the general
purpose classes with the C++ standard library. Files written by 6.x
open unchanged; most porting work is replacing removed classes.

## Checklist

1. Build with CMake and C++17 (`find_package(DataReel 7)`,
   `DataReel::datareel`). The old nmake, Borland and MinGW makefiles,
   `dll/` and `winslib/` are gone.
2. Keep `DATAREEL_64BIT_DATABASE` and `DATAREEL_NATIVE_TYPES` the same
   as your 6.x build so existing files keep their layout. In the 6.x
   Linux makefile `LARGEFILES=1` meant the 64-bit engine and
   `TYPES_DEFMACS` with `__USE_NATIVE_INT_TYPES__` meant native types.
3. Replace the removed classes (table below).
4. Change network code to TLS (below).
5. Remove includes of headers that no longer exist (`gxstring.h`,
   `ustring.h`, `membuf.h`, `gxlist.h`, `gxbstree.h`, `dfileb.h`,
   `futils.h`, `gxthread.h`, `gxmutex.h`, `gxcond.h`, `gxsema.h`,
   `gxdatagm.h`, `gxsutils.h`, `stdafx.h`, `gxdlincs.h`). `systime.h`
   and `thrpool.h` still exist with new contents.

## Removed classes and replacements

| 6.x | DataReel 7 |
|-----|------------|
| `UString`, `gxString` | `std::string` plus `strutil.h` functions |
| `gxStringw` | `std::wstring` |
| `MemoryBuffer`, `MemoryBlock` | `std::vector<unsigned char>` |
| `gxList`, `gxListB`, `gxBStree` | `std::list`, `std::vector`, `std::set`, `std::map` |
| `DiskFileB`, `futils` | `std::fstream`, `std::filesystem` |
| `SysTime` | `systime.h` functions (`date_time_string()`, `smtp_time()`, `http_time()`, `iso8601_utc()`), `std::chrono` |
| `gxThread` and helpers | `std::thread` |
| `gxMutex`, `gxCondition`, `gxSemaphore` | `std::mutex`, `std::condition_variable`, `std::counting_semaphore` (C++20) |
| `thrPool` | `datareel::ThreadPool` (`thrpool.h`) |
| `gxsHTTPSClient` | `gxsHTTPClient` (HTTPS only) |
| `gxDatagram` | `gxSocket::OpenDatagram()`, `SendTo()`, `RecvFrom()` |
| `gxIP32`, `gxMAC48` | `gxsAddress`, `std::string` |
| `FString` | `char[N]` or `std::array` |
| `gxsHTML`, `gxsXML`, `gxsRSS` | a dedicated parser library |
| `HyperText`, `PostScriptDrv`, report writers | `std::ostream` |
| `Terminal` | ncurses |
| `gxDeviceCache` | none |

## Strings

`strutil.h` has std::string versions of the gxString members:

```cpp
#include "strutil.h"
using namespace datareel;

std::string s = "  Name: Alice  ";
trim_in_place(s);                          // s.TrimLeadingSpaces(); s.TrimTrailingSpaces();
delete_before_including(s, ": ");          // s.DeleteBeforeIncluding(": ");
std::size_t pos = ifind(s, "ali");         // s.IFind("ali"); npos (not -1) if absent
s = to_upper(s);                           // s.ToUpper();
int n = to_int("42");                      // gxString("42").Atoi()
std::vector<std::string> parts = split("a,b,c", ',');
replace_all(s, "Alice", "Bob");            // s.ReplaceString("Alice", "Bob")
std::string line;
str_append(line, "id=", 42, ' ', 3.5);     // line << "id=" << 42 << ' ' << 3.5;
```

`Find()` results are `std::string::npos` on failure instead of `-1`;
check every `if(pos == -1)` test.

## Memory buffers

```cpp
std::vector<unsigned char> buf;            // MemoryBuffer buf;
buf.assign(p, p + n);                      // buf.Load(p, n);
send(buf.data(), (int)buf.size());         // buf.m_buf(), buf.length()
```

## Threads

```cpp
#include "thrpool.h"
datareel::ThreadPool pool(8);              // thrPool + gxThread subclass
auto result = pool.Submit([](int x) { return x * 2; }, 21);
int v = result.get();                      // 42; exceptions propagate here
pool.Wait();                               // Wait for queued tasks
```

## Configuration and log files

- `gxConfig::Load()` and the write functions return `bool` (true on
  success). `GetStrValue()` returns `const char *`, or null when the
  name is not set.
- `LogFile` no longer derives from `DiskFileB`: `Open(name)` appends,
  `WriteLine(text)` writes a time-stamped line and is thread safe,
  `operator<<` writes text.
- `CDate::c_str()` is now `str()` and returns `std::string`.

## Network code

Plain text protocols were removed. Typical changes:

| 6.x | DataReel 7 |
|-----|------------|
| `gxSocket` server with `remote_socket`, `RemoteRecv`, `RemoteSend` | `gxsTLSServer::Accept()` gives a `gxsTLSConnection` per client |
| `gxSocket` client `Send`/`Recv` | `gxsTLSConnection::Connect()`, `Send()`, `ReadLine()` |
| `gxsHTTPClient` with `http://` | `gxsHTTPClient` with `https://` only |
| `gxsSMTPClient` on port 25 | `gxsSMTPClient::Connect(host, 587, gxsMAIL_STARTTLS)` or port 465 |
| `gxsPOP3Client` on port 110 plain | `Connect(host)` (995) or `Connect(host, 110, gxsMAIL_STARTTLS)` |
| `gxsFTPClient` plain FTP | FTPS: `Connect(host)` (AUTH TLS on 21) |
| `gxStream` over plain `gxSocket` | `gxStream` over TLS: servers `Accept(stream.Connection())` |
| `gxSSL` with SSLv3/TLS 1.0 methods | `gxSSL_TLS` (1.2 and 1.3), `gxSSL_TLSv1_2`, `gxSSL_TLSv1_3` |

Servers now need a certificate and private key. Clients verify the
server certificate; give them your CA with `SetCAFile()`.

## Namespace

All names are in `namespace datareel`. The headers add
`using namespace datareel;` so unqualified 6.x code compiles. Define
`DATAREEL_NO_GLOBAL_NAMES` before including DataReel headers (or in
your build) to keep the global namespace clean and write
`datareel::gxDatabase`.

## Behavior changes in classes that stayed

- `gxDatabase::FindNextObject()`, `FindNextBlock()`, `FindPrevBlock()`
  and `FindPrevObject()` take an object (or block) address and return
  0 at the end. `FindPrevBlock()` no longer skips deleted blocks.
- `gxRDBMS::Search()` and `SortOrderList()` fill a
  `std::set<gxrdDatabaseRecord>` instead of a `gxBStree`.
- `gxSSL::MakeRSAPrivateKey()` makes 3072-bit keys by default;
  `MakeDHParms()`, `LoadDHParms()`, `MakeRSAEphKey()` were removed.
