// Makes phase C .vix files with the master (phase A-C) library, for the phase D compatibility test
#include <cstdio>
#include <cmath>
#include <vector>
#include "gxvindex.h"
using namespace datareel;
int main(int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : ".";
  struct T { const char *name; gxvElementType t; gxvMetric m; int n; unsigned M; } ts[] = {
    { "phase_c_f32_l2.vix", gxvFLOAT32, gxvL2, 300, 8 }, { "phase_c_f16_cos.vix", gxvFLOAT16, gxvCOSINE, 300, 8 },
    { "phase_c_i8_ip.vix", gxvINT8, gxvIP, 300, 8 }, { "phase_c_f32_graph.vix", gxvFLOAT32, gxvL2, 5000, 4 } };
  char ep[1024]; snprintf(ep, sizeof(ep), "%s/phase_c_expected.txt", dir);
  FILE *out = fopen(ep, "w");
  for(auto &x : ts) {
    char p[1024]; snprintf(p, sizeof(p), "%s/%s", dir, x.name);
    remove(p);
    gxVectorIndex v;
    if(v.Create(p, 8, x.t, x.m, x.M, "phase-c-test", 100) != gxv_NO_ERROR) { printf("create %s\n", p); return 1; }
    std::vector<float> vec(8);
    for(int i = 0; i < x.n; i++) {
      for(int d = 0; d < 8; d++) vec[d] = (float)sin(i * 0.37 + d * 1.3) + 0.01f * d;
      unsigned tags[4] = { (unsigned)(i % 7), (unsigned)(100 + i), 20261101u + (unsigned)(i % 30), 0 };
      if(v.Insert(1000 + i, vec.data(), tags) != gxv_NO_ERROR) { printf("insert\n"); return 1; }
    }
    for(int i = 0; i < x.n; i += 11) v.Delete(1000 + i);
    if(x.n > 4096) v.Repair();
    for(int qi = 0; qi < 6; qi++) {
      for(int d = 0; d < 8; d++) vec[d] = (float)cos(qi * 0.91 + d * 0.7);
      std::vector<gxvResult> r;
      v.Search(vec.data(), 10, r, 64);
      fprintf(out, "%s q%d", x.name, qi);
      for(auto &h : r) fprintf(out, " %lld:%.9g", h.id, h.distance);
      fprintf(out, "\n");
    }
    fprintf(out, "%s count %llu deleted %llu\n", x.name, (unsigned long long)v.Count(), (unsigned long long)v.Deleted());
    v.Close();
    printf("%s\n", p);
  }
  fclose(out);
  return 0;
}
