xver - 11/16/2026: DataReel 6.4 <-> 7.4 gxStream cross tests (test only, Linux)

  wirecmp.cpp  the block header packed by 6.4 and by 7.4: the same 32 bytes (and the 64-bit Linux struct's own bytes),
               unpacked as packed. Build once against each tree's headers (-DDR7 for 7.4), run both, cmp the output.
  xv6.cpp      6.4 side, xv7.cpp 7.4 side: "server PORT PKI" / "client PORT PKI [old24|raw32]" over mutual TLS: a
               300-byte request, the reply reversed; old24 sends a header with 4-byte words (a 64-bit Windows peer
               before the canonical form: refused, PeerOldHeader()), raw32 the old 64-bit Linux bytes (accepted)
  xmatrix.sh   the 10-row matrix (both versions as server and client, raw32, old24); needs xv6, xv7 and pki/ here
  hook64.cpp   6.4 gxStream speaking gxObjRepl 7.4's handshake: "primary HOST PORT PKI" against a 7.4 objrepl copy,
               "copy PORT PKI" for a 7.4 objrepl primary (hello, reply, F_EPOCH, ack)

Build (from this folder; D6 = the -D flags of the 6.4 library build, F7 = CXX_DEFINES of a 7.4 CMake build):
  g++ -std=gnu++98 $D6 -I <6.4>/include xv6.cpp -o xv6 <6.4>/lib/libdatareel.a -lssl -lcrypto -lz -lpthread
  g++ $F7 -I <7.4>/include xv7.cpp -o xv7 <7.4 build>/libdatareel.a -lssl -lcrypto -lz -lpthread
  (hook64 like xv6; wirecmp: once with the 6.4 flags, once with -DDR7 and the 7.4 flags, no library)
  utils/objrepl/objrepl_certs.sh pki --copy-host 127.0.0.1 --days 30 --apply ; ./xmatrix.sh
Both builds use the 64-bit database engine.
