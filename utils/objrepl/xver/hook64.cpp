// hook64: DataReel 6.4 gxStream speaking gxObjRepl 7.4's handshake (11/16/2026, test only)
//   hook64 primary HOST PORT PKI   as a primary to a 7.4 objrepl copy: hello, reply, F_EPOCH, ack
//   hook64 copy PORT PKI           as a copy for a 7.4 objrepl primary: hello, reply (accept), F_EPOCH, ack, then
//                                  answers one more frame (or a close) and exits
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "gxstream.h"
typedef std::vector<unsigned char> Bytes;
static void u8(Bytes &b, unsigned v) { b.push_back((unsigned char)v); }
static void un(Bytes &b, unsigned long long v, int n) { for(int i = 0; i < n; i++) b.push_back((unsigned char)(v >> (8 * i))); }
static void str(Bytes &b, const std::string &s) { un(b, s.size(), 2); b.insert(b.end(), s.begin(), s.end()); }
struct Rd { const unsigned char *p; size_t n, o; Rd(const Bytes &b) : p(b.empty() ? 0 : &b[0]), n(b.size()), o(0) {}
  unsigned long long get(int k) { unsigned long long v = 0; if(o + k > n) { o = n + 1; return 0; } for(int i = 0; i < k; i++) v |= (unsigned long long)p[o + i] << (8 * i); o += k; return v; }
  std::string s() { int k = (int)get(2); if(o + k > n) { o = n + 1; return ""; } std::string r((const char *)p + o, k); o += k; return r; }
  bool ok() const { return o <= n; } };
static const unsigned long MAGIC = 0x3550524fUL;   // "ORP5"
// a frame from the client side (ReadClientHeader... are server side): use the low level calls on the right socket
class Hook : public gxStream {
public:
  bool send(gxsSocket_t s, const Bytes &b) { gxBlockHeader gx; SetBlockStatus(gx, gxSendBlock); gx.block_length = (gxUINT32)b.size(); return WriteBlock(s, b.empty() ? "" : (const char *)&b[0], gx) == 0; }
  bool recv(gxsSocket_t s, Bytes &b) {
    gxBlockHeader gx; if(ReadHeader(s, gx, 30, 0) != 0) { printf("  ReadHeader: %s old=%d\n", SocketExceptionMessage(), PeerOldHeader()); return false; }
    b.resize((size_t)gx.block_length); if(b.empty()) return true;
    return ReadBlock(s, &b[0], (int)b.size(), gx, 30, 0) == 0; }
  gxsSocket_t client() { return gxsocket; }
  gxsSocket_t remote() { return remote_socket; }
};
int main(int argc, char **argv)
{
  if(argc < 4) return 2;
  std::string mode = argv[1]; char f1[512], f2[512], f3[512];
  Hook st;
  if(mode == "primary") {
    const char *pki = argv[4]; sprintf(f1, "%s/primary.pem", pki); sprintf(f2, "%s/primary.key", pki); sprintf(f3, "%s/ca.pem", pki);
    st.SetClientCert(f1, f2); st.SetServerCAFile(f3);
    if(st.StreamClient(atoi(argv[3]), argv[2]) != 0) { printf("hook64: connect failed: %s %s\n", st.SocketExceptionMessage(), st.TLSError()); return 1; }
    Bytes h; un(h, MAGIC, 4); un(h, 1, 8); str(h, ""); str(h, "hook64"); u8(h, 1); un(h, 0, 8); un(h, 0, 8);
    Bytes rep; if(!st.send(st.client(), h) || !st.recv(st.client(), rep)) { printf("hook64: no reply to the hello\n"); return 1; }
    Rd r(rep); unsigned ok = (unsigned)r.get(1); unsigned long long applied = r.get(8), cep = r.get(8); unsigned snap = (unsigned)r.get(1);
    std::string cname = r.s(); unsigned crole = (unsigned)r.get(1); r.get(8); std::string why = r.s();
    printf("hook64: reply ok=%u applied=%llu epoch=%llu snapshot=%u copy=%s role=%u why=[%s]\n", ok, applied, cep, snap, cname.c_str(), crole, why.c_str());
    if(!r.ok() || !ok || crole != 2) return 1;
    Bytes e; u8(e, 5); un(e, 1, 8); Bytes ack;
    if(!st.send(st.client(), e) || !st.recv(st.client(), ack) || ack.size() < 8) { printf("hook64: no ack to F_EPOCH\n"); return 1; }
    Rd a(ack); printf("hook64: F_EPOCH acked at seq %llu\n", a.get(8));
    st.Close(); return 0;
  }
  const char *pki = argv[3]; sprintf(f1, "%s/copy.pem", pki); sprintf(f2, "%s/copy.key", pki); sprintf(f3, "%s/ca.pem", pki);
  st.SetServerCert(f1, f2); st.SetClientCAFile(f3); st.AllowClientCN("objrepl-primary");
  if(st.StreamServer(atoi(argv[2])) != 0) { printf("hook64: listen failed\n"); return 1; }
  if(st.Accept() < 0) { printf("hook64: accept failed: %s\n", st.TLSError()); return 1; }
  Bytes h; if(!st.recv(st.remote(), h)) { printf("hook64: no hello\n"); return 1; }
  Rd r(h); unsigned long magic = (unsigned long)r.get(4); unsigned long long pep = r.get(8); std::string links = r.s(), pname = r.s();
  unsigned prole = (unsigned)r.get(1); unsigned long long plast = r.get(8);
  printf("hook64: hello magic=%08lx epoch=%llu links=[%s] primary=%s role=%u last=%llu\n", magic, pep, links.c_str(), pname.c_str(), prole, plast);
  if(!r.ok() || magic != MAGIC) return 1;
  Bytes rep; u8(rep, 1); un(rep, 0, 8); un(rep, 0, 8); u8(rep, 0); str(rep, "hook64"); u8(rep, 2); un(rep, 0, 8); str(rep, "");
  if(!st.send(st.remote(), rep)) return 1;
  Bytes e; if(!st.recv(st.remote(), e) || e.empty() || e[0] != 5) { printf("hook64: no F_EPOCH (%u bytes, kind %d)\n", (unsigned)e.size(), e.empty() ? -1 : e[0]); return 1; }
  Rd re(e); re.get(1); printf("hook64: F_EPOCH epoch=%llu\n", re.get(8));
  Bytes ack; un(ack, 0, 8); if(!st.send(st.remote(), ack)) return 1;
  Bytes nxt; if(st.recv(st.remote(), nxt)) printf("hook64: next frame kind %d, %u bytes\n", nxt.empty() ? -1 : nxt[0], (unsigned)nxt.size());
  else printf("hook64: the primary closed after the handshake\n");
  st.Close(); return 0;
}
