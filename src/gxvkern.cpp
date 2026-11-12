// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxvkern.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
// Date Last Modified: 11/12/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Distance kernels, see gxvkern.h. The SSE2 path (every x86-64 CPU) and
the plain loop give bit-identical results; vix_test checks the order.
Do not build this file with -ffast-math. Floating point contraction
(fused multiply-add) is turned off below and in CMakeLists.txt.
*/
// ----------------------------------------------------------- //
// No multiply-add contraction in this file whatever the build flags: GCC
// fuses a*b + c into an FMA on x86-64-v3 (RHEL 10's baseline) and that
// changes the last bit of a distance. CMakeLists.txt also builds this
// file with -ffp-contract=off.
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize ("fp-contract=off")
#endif
#include "gxvkern.h"
#include <math.h>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define GXV_SSE2 1
#endif

namespace datareel {

static inline float gxv_combine(const float s[8])
{
  return ((s[0] + s[4]) + (s[2] + s[6])) + ((s[1] + s[5]) + (s[3] + s[7]));
}

// The 8 partial sums live in two 4-lane registers: lanes 0-3 hold
// s0..s3 and lanes 4-7 hold s4..s7, each lane adding its own elements
// in order, so the result is the same as the scalar loop. Separate
// multiply and add (no fused multiply-add) on every path.
#ifdef GXV_SSE2
static inline void gxv_store8(__m128 lo, __m128 hi, float s[8])
{
  _mm_storeu_ps(s, lo);
  _mm_storeu_ps(s + 4, hi);
}
#endif

float gxv_l2sq(const float *a, const float *b, unsigned dim)
{
  float s[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  unsigned i = 0, n8 = dim & ~7u;
#ifdef GXV_SSE2
  __m128 lo = _mm_setzero_ps(), hi = _mm_setzero_ps();
  for(; i < n8; i += 8) {
    __m128 d0 = _mm_sub_ps(_mm_loadu_ps(a + i), _mm_loadu_ps(b + i));
    __m128 d1 = _mm_sub_ps(_mm_loadu_ps(a + i + 4), _mm_loadu_ps(b + i + 4));
    lo = _mm_add_ps(lo, _mm_mul_ps(d0, d0));
    hi = _mm_add_ps(hi, _mm_mul_ps(d1, d1));
  }
  gxv_store8(lo, hi, s);
#else
  for(; i < n8; i += 8) {
    for(unsigned j = 0; j < 8; j++) {
      float d = a[i + j] - b[i + j];
      s[j] += d * d;
    }
  }
#endif
  float r = gxv_combine(s);
  for(; i < dim; i++) {
    float d = a[i] - b[i];
    r += d * d;
  }
  return r;
}

float gxv_dot(const float *a, const float *b, unsigned dim)
{
  float s[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  unsigned i = 0, n8 = dim & ~7u;
#ifdef GXV_SSE2
  __m128 lo = _mm_setzero_ps(), hi = _mm_setzero_ps();
  for(; i < n8; i += 8) {
    lo = _mm_add_ps(lo, _mm_mul_ps(_mm_loadu_ps(a + i), _mm_loadu_ps(b + i)));
    hi = _mm_add_ps(hi, _mm_mul_ps(_mm_loadu_ps(a + i + 4), _mm_loadu_ps(b + i + 4)));
  }
  gxv_store8(lo, hi, s);
#else
  for(; i < n8; i += 8) {
    for(unsigned j = 0; j < 8; j++) s[j] += a[i + j] * b[i + j];
  }
#endif
  float r = gxv_combine(s);
  for(; i < dim; i++) r += a[i] * b[i];
  return r;
}

float gxv_normalize(float *v, unsigned dim)
{
  float len = sqrtf(gxv_dot(v, v, dim));
  if(len > 0.0f) {
    float inv = 1.0f / len;
    for(unsigned i = 0; i < dim; i++) v[i] *= inv;
  }
  return len;
}

int gxv_finite(const float *v, unsigned dim)
{
  for(unsigned i = 0; i < dim; i++) {
    if(!isfinite(v[i])) return 0;
  }
  return 1;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
