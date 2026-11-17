DataReel 6.4 example: searching a DataReel 7.x object server's vectors
======================================================================

A DataReel 6.x program searches the vectors a DataReel 7.x database
object server serves, with the gxv7ObjectClient class
(dr7features/include/dr7features.h): op 7 VECTOR_SEARCH and op 8
VECTOR_INFO over mutual TLS.

The server is the DataReel 7.x database object server, whose config
(gxObjectDbConfig) gives a database a vector index or set: path, access
read, vectors = a .vixset manifest or one .vix. 11/17/2026: DataReel
7.4 removed gxrepl (whose "serve" command was the server
before); the 6.4 build makes vector_server (dr7features/inner), the
7.x object server for one database with its vectors:

  vector_server PORT CERT KEY CA CLIENT_CN DB_NAME DB_FILE VECTORS

The client connects with its certificate, checks the server's
certificate against the CA and the server name, and opens one channel
to the database in read mode:

  #include "dr7features.h"
  gxv7ObjectClient cli;
  cli.Connect("db.example.com", 2073, "db.example.com",
	      "client.pem", "client.key", "ca.pem", "news");
  gxv7VectorInfo info;
  cli.VectorInfo(info);                 // dim, type, metric, files, nodes, spec
  gxv7Result r[10]; unsigned n;
  cli.VectorSearch(query, info.dim, 10, r, n);   // closest first
  // r[i].id, r[i].distance, r[i].tags, cli.FileName(r[i].file)

VectorSearch() also takes the search width (ef), a tag filter and a
day range (tag 2 between day_lo and day_hi; the server skips the files
outside it). The server refuses k over 1000, a query of another
dimension and a query that is not finite; the channel goes on
answering.

POSIX only: DataReel 7.4 builds its multiplex session (the object
client) on POSIX systems only, so on Windows gxv7ObjectClient::Connect()
returns -1 and the CMake build does not build this program.

Build: the 6.x CMake build with DATAREEL7_FEATURES_DIR (it builds this
program as vector_client and vector_server). Test (Linux; the 7.x
object server is POSIX only):

  ./run_vector_client.sh build/dr7inner/tools build/vector_client [WORK_FOLDER]

It makes throwaway certificates, a 3-day vector set (the program's
"make" mode), starts vector_server with the news database, and checks:
- VECTOR_INFO describes the set;
- 6 searches, a day range search and a tag filter search: the server's
  answers equal a local gxv7VectorSet search of the same files (ids,
  distances, order and file names);
- wrong dimension and k over 1000 refused, the channel still answers;
- a client certificate not in allow_cn, a self-signed certificate with
  the allowed name, a server certificate for another name and an
  unknown database are refused.

The client uses the DataReel 7.x client classes (gxStream, the
multiplex session gxdMuxSession) inside the dr7features library, so
its messages are the 7.x ones by construction. The DataReel 6.x
gxStream (with the 6.3.1 mutual TLS) is a different, older protocol:
use it for 6.x servers, and gxv7ObjectClient for 7.x object servers.
