DataReel 6.4 example: replicating a DataReel 6.x program's records
===================================================================

11/17/2026. DataReel 6.x programs replicate their records with the
DataReel 7.x object replication (gxObjRepl), through the 6.x class
gxv7ObjRepl (dr7features/include/dr7features.h). DataReel 6.4 has no
replication code of its own: the class calls the 7.x gxObjRepl, built
from a DataReel 7.4 source tree by the 6.x CMake build.

Two nodes, each a folder of object databases. The primary commits
objects (bytes under a 64-bit id both nodes share) and sends them to
its copy as gxStream blocks over TLS with certificates on both sides.
Only objects travel, never file pages: each node keeps its own file
addresses, free space and indexes. The primary and the copy can each
be these 6.x programs, a DataReel 7.4 program or the 7.4 objrepl tool,
on Linux or Windows.

Before 11/17/2026 this example was a 6.x gxDatabase folder copied by
the 7.x gxrepl (page shipping). DataReel 7.4 removed gxrepl;
replication is gxObjRepl, and the 6.x programs here use it directly.

  repl6_writer.cpp       the primary: "primary ROOT CONF RECORDS" writes
                         station readings (a plain 6.x struct with a
                         CRC, repl6_records.h), commits them 25 at a
                         time and drains them to the copy
  repl6_check.cpp        the copy: "copy ROOT CONF [--check]" serves
                         until TERM/INT, Ctrl+C or ROOT/objrepl.stop and
                         with --check reads every live record back and
                         checks its CRC. Both programs also have
                         "digest ROOT", "status ROOT" and "engine"
  run_repl6_cross.sh     the test: 6.4 primary -> 7.4 copy, 7.4 primary
                         -> 6.4 copy, 6.4 -> 6.4, a copy that starts late,
                         a second run on the same folder, a refused
                         certificate, a 7.4 takeover of a 6.4 copy folder
  run_repl6_xver.sh      the 7.4 tree's 6.4<->7.4 wire tests
                         (utils/objrepl/xver: wirecmp, xmatrix, hook64)
                         built against this 6.4 library (Linux)

Build (DataReel 7.4 or later, which has gxObjRepl and the
canonical gxStream block header; the build stops with a message for
an older tree):

  cmake -S datareel-6.4 -B build -DCMAKE_BUILD_TYPE=Release \
        -DDATAREEL7_FEATURES_DIR=/path/to/datareel-7.4 \
        -DDATAREEL_64BIT_DATABASE=ON
  cmake --build build -j2
  # build/repl6_writer, build/repl6_check, and the 7.4 objrepl tool
  # in build/dr7inner/tools

Tests:

  ./run_repl6_cross.sh build/dr7inner/tools build [WORK]
  ./run_repl6_xver.sh /path/to/datareel-7.4 build [WORK]

A 32-bit engine build (DATAREEL_64BIT_DATABASE=OFF) runs them with
OBJREPL_ALLOW_32BIT=1, as the 7.4 objrepl tests: the run makes its own
folders, so no engine reads the other's files. Never point a 32-bit
engine node and a 64-bit engine node at the same folder.

Configuration: an objrepl.conf file, one "key value" line each (the
7.4 utils/objrepl/objrepl_certs.sh makes a CA, the two certificates
and sample files):

  # the primary                     # the copy
  name primary1                     name copy1
  peer-name copy1                   peer-name primary1
  copy copyhost:24600               listen 0.0.0.0:24600
  ca ca.pem                         ca ca.pem
  cert primary.pem                  cert copy.pem
  key primary.key                   key copy.key
                                    allow-cn objrepl-primary

In a program:

  gxv7ObjRepl p;
  p.LoadConfig("primary.conf");
  if(p.Open("primary", gxv7ObjRepl::PRIMARY) != 0) ... p.Error()
  p.OpenDatabase("readings");
  p.Begin();
  unsigned long long id = p.NewId();
  p.Put("readings", id, &rec, sizeof(rec));
  p.Commit();      // durable on the primary, in its outbox
  p.Pump();        // 0: the copy has every commit (or Drain(seconds))

Replication is asynchronous: if the primary is lost, the commits the
copy does not have are lost with it (the status shows them as lag_seq).
Takeover() makes the copy the primary at the next epoch, and the old
primary is fenced off until Rejoin(). See the DataReel 7.4 gxObjRepl
reference and administrator's guide for failover, monitoring (Check(),
the status file and the alarm file), indexes and links.
