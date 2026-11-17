# Text, CSV, single-instance and file utilities (DataReel 7.4)

Small helpers added in 7.4. All are in the `datareel` namespace; the
fragments below show the calls, the complete programs are named after
each section (they are built with the examples and checked by
`tests/run_tests.sh`).

## Terminal control sequences out of text (strutil.h)

`strip_ansi(text, options)` returns the text without ECMA-48 (VT100,
VT220, xterm) control sequences: CSI (`ESC [ ...`, colours, cursor
moves), OSC (titles), DCS/PM/APC strings, two-byte escapes and C0
controls. `AnsiStripper` does the same for text that arrives in pieces
(a serial port, a socket): a sequence split across two `Feed()` calls
is still removed.

```cpp
#include "strutil.h"
std::string clean = datareel::strip_ansi(raw);           // one string

datareel::AnsiStripOptions o;
o.cr_to_newline = true;        // \r alone becomes \n, \r\n becomes \n
o.cursor_to_newline = true;    // cursor moves (CSI H, f, B, E) become \n
datareel::AnsiStripper st(o);
std::string out;
st.Feed(piece1, out); st.Feed(piece2, out);
st.Finish(out);                // the end: an unfinished sequence is dropped
```

Options: `keep_tab` and `keep_newline` (default true), `cr_to_newline`,
`cursor_to_newline`, `c1_8bit` (bytes 0x80-0x9F are C1 controls; off by
default so UTF-8 passes through), `keep_del`. `Removed()` counts the
sequences removed. A C0 control inside a CSI is acted on and the CSI
continues to its final byte, as in ECMA-48.

## CSV files (gxcsv.h)

`gxCSVReader` reads RFC 4180 CSV from a file or a string: quoted
fields with the delimiter, quotes (`""`) and line ends in them, CR LF
or LF rows, a UTF-8 byte order mark skipped. `gxCSVWriter` writes rows
that read back the same.

```cpp
#include "gxcsv.h"
datareel::gxCSVReader r;                     // gxCSVOptions: delimiter, quote, strict,
if(!r.Open("in.csv")) { /* r.ErrorMessage() */ }   // trim_unquoted, skip_empty_rows, crlf, quote_all
r.ReadHeader();                              // the first row: Header(), Column("name")
int qty = r.Column("qty");                   // -1 if there is no such column
std::vector<std::string> row;
while(r.ReadRow(row)) { /* row[qty] */ }
if(r.Error()) { /* strict mode: the line in r.Line() */ }

datareel::gxCSVWriter w;
w.Open("out.csv");                           // or SetOutput(&string)
w.WriteRow({"id", "note"});
w.WriteRow({"7", "two\nlines, quoted"});
w.Close();
```

`gxCSVReader::ParseText(text, rows)` parses a whole string at once;
`gxCSVWriter::FormatRow(fields)` formats one row. Program:
`examples/general/csv_merge.cpp` (joins two files on a key column).

## One instance at a time (gxproclock.h)

`gxProcessLock::Acquire(file)` takes an exclusive lock on a lock file
(flock on POSIX, LockFileEx on Windows) and writes the process id into
it. A second `Acquire()` of the same file, in this process or another,
fails at once and `HolderPid()` names the holder. The system drops the
lock when the holder ends, however it ends, so a stale file never
blocks a restart. It is not inherited by child processes.

```cpp
#include "gxproclock.h"
datareel::gxProcessLock lock;
if(!lock.Acquire("/var/run/myapp.lock")) {
  fprintf(stderr, "%s\n", lock.Error().c_str());   // "... is locked by process N"
  return 2;
}
// ... run; the destructor (or Release()) frees it
```

`gxProcessLock::IsLocked(file, &pid)` checks without taking the lock.
Program: `examples/general/service_example.cpp`, the frame of a
long-running program (a heartbeat, a stop file, SIGTERM/Ctrl+C, POSIX
`--daemon`, and on Windows `--install`/`--service`/`--uninstall` as a
demand-start LocalService service).

## File utilities (utils/)

Back in 7.4, built with the library:

- `hexdump [switches] infile [outfile]`: a file in hex, decimal or as a
  C array; `-r` rebuilds the binary from a hex dump; `-c` and `-C`
  print CRC-32 and CRC-16 checksums.
- `ffind [switches] file`: find (`-F`), replace (`-R"old" -W"new"`) and
  insert strings in text files; the exit code is the match count.
- `txfilter [switches] infile`: keep the printable (or alphabetic,
  alphanumeric, numeric) characters of a text file; UNIX or DOS line
  ends.

Each prints its switches when run with none. See `docs/utils/` for the
full lists.
