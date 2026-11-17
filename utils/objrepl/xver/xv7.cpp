// xv7: DataReel 7.4 side of the 6.4 <-> 7.4 gxStream cross test (11/16/2026, test only)
//   xv7 server PORT PKI       one client: read a block, reply with it reversed; exit 0, 3 if refused as an old header
//   xv7 client PORT PKI [old24|raw32]   send a 300-byte request, check the reply; old24 sends the header with
//                             4-byte words (a 64-bit Windows peer before the fix), raw32 the struct bytes (old Linux)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "gxstream.h"
using namespace datareel;
int main(int argc, char **argv)
{
  if(argc < 4) return 2;
  std::string mode = argv[1], pki = argv[3]; int port = atoi(argv[2]);
  std::string how = argc > 4 ? argv[4] : "";
  if(mode == "server") {
    gxStreamServer srv; srv.SetClientCAFile(pki + "/ca.pem"); srv.AllowClientCN("objrepl-primary");
    if(srv.Listen(port, pki + "/copy.pem", pki + "/copy.key", std::string(), "127.0.0.1") != 0) { printf("listen failed\n"); return 1; }
    gxStream st;
    if(srv.Accept(st) != 0) { printf("accept failed\n"); return 1; }
    gxBlockHeader h;
    if(st.ReadHeader(h) != 0) { printf("server7: header refused (%s) old=%d\n", st.SocketExceptionMessage(), st.PeerOldHeader()); return st.PeerOldHeader() ? 3 : 1; }
    std::vector<unsigned char> b;
    if(st.ReadBlock(b, h) != 0) { printf("server7: block read failed\n"); return 1; }
    std::vector<unsigned char> r(b.rbegin(), b.rend());
    if(st.WriteBlock(r.data(), (int)r.size()) != 0) { printf("server7: reply failed\n"); return 1; }
    printf("server7: got %u bytes (status 0x%lx), replied\n", (unsigned)b.size(), (unsigned long)h.block_status);
    return 0;
  }
  gxStream st; st.SetClientCert(pki + "/primary.pem", pki + "/primary.key"); st.Connection().SetCAFile(pki + "/ca.pem");
  if(st.Connect("127.0.0.1", port) != 0) { printf("client7: connect failed: %s\n", st.Error().c_str()); return 1; }
  unsigned char req[300]; for(int i = 0; i < 300; i++) req[i] = (unsigned char)(i * 7 + 1);
  if(how == "old24" || how == "raw32") {
    unsigned char w[32]; memset(w, 0, 32); unsigned n;
    unsigned long cw = 0xfefe, len = 300, stt = 'N' | ('R' << 8);
    if(how == "old24") { memcpy(w, &cw, 4); memcpy(w + 4, &len, 4); memcpy(w + 8, &stt, 4); n = 24; }
    else { for(int i = 0; i < 4; i++) { w[i] = (unsigned char)(cw >> 8 * i); w[8 + i] = (unsigned char)(len >> 8 * i); w[16 + i] = (unsigned char)(stt >> 8 * i); } n = 32; }
    std::string raw((char *)w, n); raw.append((char *)req, 300);
    if(st.Connection().Send(raw.data(), (int)raw.size()) != (int)raw.size()) { printf("client7: raw send failed\n"); return 1; }
    gxBlockHeader rh; std::vector<unsigned char> rep;
    if(st.ReadHeader(rh) != 0 || st.ReadBlock(rep, rh) != 0) { printf("client7: no reply (as expected for old24)\n"); return 4; }
    for(int i = 0; i < 300; i++) if(rep.size() != 300 || rep[i] != req[299 - i]) { printf("client7: bad reply\n"); return 1; }
    printf("client7: %s accepted, reply OK\n", how.c_str()); return 0;
  }
  gxBlockHeader rq; rq.block_length = 300; gxBlockHeader rh; std::vector<unsigned char> rep;
  if(st.RequestBlock(req, rq, rh, rep) != 0) { printf("client7: request failed (%s) old=%d\n", st.SocketExceptionMessage(), st.PeerOldHeader()); return st.PeerOldHeader() ? 3 : 1; }
  for(int i = 0; i < 300; i++) if(rep.size() != 300 || rep[i] != req[299 - i]) { printf("client7: bad reply\n"); return 1; }
  printf("client7: reply OK (%u bytes)\n", (unsigned)rep.size());
  return 0;
}
