// xv6: DataReel 6.4 side of the 6.4 <-> 7.4 gxStream cross test (11/16/2026, test only); same modes as xv7
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gxstream.h"
class Raw6 : public gxStream {  // sends raw bytes on the client's TLS connection
public:
  int raw(const void *p, int n) { return stream_send(gxsocket, p, n); }
};
int main(int argc, char **argv)
{
  if(argc < 4) return 2;
  const char *mode = argv[1], *pki = argv[3]; int port = atoi(argv[2]);
  const char *how = argc > 4 ? argv[4] : "";
  char f1[512], f2[512], f3[512];
  sprintf(f3, "%s/ca.pem", pki);
  if(!strcmp(mode, "server")) {
    gxStream srv; sprintf(f1, "%s/copy.pem", pki); sprintf(f2, "%s/copy.key", pki);
    srv.SetServerCert(f1, f2); srv.SetClientCAFile(f3); srv.AllowClientCN("objrepl-primary");
    if(srv.StreamServer(port) != 0) { printf("server6: listen failed: %s\n", srv.SocketExceptionMessage()); return 1; }
    if(srv.Accept() < 0) { printf("server6: accept failed: %s\n", srv.TLSError()); return 1; }
    gxBlockHeader h;
    if(srv.ReadClientHeader(h) != 0) { printf("server6: header refused (%s) old=%d\n", srv.SocketExceptionMessage(), srv.PeerOldHeader()); return srv.PeerOldHeader() ? 3 : 1; }
    unsigned char b[4096];
    if(srv.ReadRemoteBlock(b, sizeof(b), h) != 0) { printf("server6: block read failed\n"); return 1; }
    int n = (int)h.block_length; unsigned char r[4096]; for(int i = 0; i < n; i++) r[i] = b[n - 1 - i];
    if(srv.WriteRemoteBlock(r, n) != 0) { printf("server6: reply failed\n"); return 1; }
    printf("server6: got %d bytes (status 0x%lx), replied\n", n, (unsigned long)h.block_status);
    srv.Close();
    return 0;
  }
  Raw6 st; sprintf(f1, "%s/primary.pem", pki); sprintf(f2, "%s/primary.key", pki);
  st.SetClientCert(f1, f2); st.SetServerCAFile(f3);
  char host[] = "127.0.0.1";
  if(st.StreamClient(port, host) != 0) { printf("client6: connect failed: %s %s\n", st.SocketExceptionMessage(), st.TLSError()); return 1; }
  unsigned char req[300]; for(int i = 0; i < 300; i++) req[i] = (unsigned char)(i * 7 + 1);
  if(!strcmp(how, "old24")) {
    unsigned char w[24]; memset(w, 0, 24); unsigned long cw = 0xfefe, len = 300, stt = 'N' | ('R' << 8);
    memcpy(w, &cw, 4); memcpy(w + 4, &len, 4); memcpy(w + 8, &stt, 4);
    st.raw(w, 24); st.raw(req, 300);
    gxBlockHeader rq0; rq0.block_length = 0;
    printf("client6: old24 sent\n"); return 4;
  }
  gxBlockHeader rq; rq.block_length = 300; gxBlockHeader rh;
  unsigned char *rep = (unsigned char *)st.RequestBlock(req, rq, rh);
  if(!rep) { printf("client6: request failed (%s) old=%d\n", st.SocketExceptionMessage(), st.PeerOldHeader()); return st.PeerOldHeader() ? 3 : 1; }
  for(int i = 0; i < 300; i++) if(rh.block_length != 300 || rep[i] != req[299 - i]) { printf("client6: bad reply\n"); return 1; }
  printf("client6: reply OK (%lu bytes)\n", (unsigned long)rh.block_length);
  delete[] (char *)rep; st.Close();
  return 0;
}
