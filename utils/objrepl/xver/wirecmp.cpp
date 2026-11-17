// prints the canonical wire bytes of a few block headers, and checks unpack(pack(h)) == h
#include <stdio.h>
#include <string.h>
#include "gxheader.h"
#ifdef DR7
using namespace datareel;
#endif
int main()
{
  long long fp[] = {0, 1, 4096, 0x7fffffffLL, 0x123456789aLL, -1, -4096};
  unsigned long len[] = {0, 8, 65536, 0xfffffffful, 1ul << 40};
  int bad = 0;
  for(int a = 0; a < 7; a++) for(int b = 0; b < 5; b++) {
    gxBlockHeader h; h.block_length = (gxUINT32)len[b]; h.block_status = (gxUINT32)('N' | (b << 8)); h.block_nd_fptr = (FAU)(FAU_t)fp[a];
    unsigned char w[32]; gxWirePackBlockHeader(h, w);
    for(int i = 0; i < 32; i++) printf("%02x", w[i]); printf("\n");
    if(memcmp(w, &h, 32) != 0) fprintf(stderr, "differs from the raw struct (a=%d b=%d)\n", a, b);
    gxBlockHeader g; gxWireUnpackBlockHeader(w, g);
    if(g.block_check_word != h.block_check_word || g.block_length != h.block_length || g.block_status != h.block_status || (FAU_t)g.block_nd_fptr != (FAU_t)h.block_nd_fptr) bad++;
  }
  fprintf(stderr, "roundtrip mismatches %d\n", bad);
  return bad;
}
