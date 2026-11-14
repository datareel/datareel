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
#include <string.h>
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

// ----------------------------------------------------------- //
// Element types (phase C): IEEE binary16 and int8 with a scale
// ----------------------------------------------------------- //

unsigned short gxv_f16_from_float(float f)
{
  unsigned x;
  memcpy(&x, &f, 4);
  unsigned sign = (x >> 16) & 0x8000u;
  unsigned ax = x & 0x7FFFFFFFu;
  if(ax >= 0x7F800000u) { // Infinity or NaN (a quiet NaN stays a NaN)
    return (unsigned short)(sign | 0x7C00u | (ax > 0x7F800000u ? 0x200u : 0u));
  }
  if(ax >= 0x477FF000u) return (unsigned short)(sign | 0x7C00u); // 65520 and up: infinity
  if(ax < 0x38800000u) {
    // Below 2^-14, the smallest normal half: a subnormal or zero.
    // Values under 2^-25 round to zero (2^-25 itself ties to even: 0).
    if(ax < 0x33000000u) return (unsigned short)sign;
    unsigned e = ax >> 23;
    unsigned m = (ax & 0x7FFFFFu) | 0x800000u;
    unsigned shift = 126 - e; // value = m * 2^(e-150); unit 2^-24
    unsigned h = m >> shift;
    unsigned rem = m & ((1u << shift) - 1);
    unsigned half = 1u << (shift - 1);
    if(rem > half || (rem == half && (h & 1u))) h++; // May become the smallest normal
    return (unsigned short)(sign | h);
  }
  // Normal: rebias the exponent (127 -> 15) and round the 13 bits dropped
  // to nearest, ties to even; a carry moves into the exponent correctly
  unsigned h = (ax - 0x38000000u) >> 13;
  unsigned rem = ax & 0x1FFFu;
  if(rem > 0x1000u || (rem == 0x1000u && (h & 1u))) h++;
  return (unsigned short)(sign | h);
}

float gxv_float_from_f16(unsigned short h)
{
  unsigned sign = ((unsigned)h & 0x8000u) << 16;
  unsigned e = ((unsigned)h >> 10) & 0x1Fu;
  unsigned m = (unsigned)h & 0x3FFu;
  unsigned x;
  if(e == 0) {
    if(m == 0) {
      x = sign;
    }
    else { // Subnormal: normalize it
      e = 113;
      while(!(m & 0x400u)) {
	m <<= 1;
	e--;
      }
      m &= 0x3FFu;
      x = sign | (e << 23) | (m << 13);
    }
  }
  else if(e == 31) {
    x = sign | 0x7F800000u | (m << 13);
  }
  else {
    x = sign | ((e + 112) << 23) | (m << 13);
  }
  float f;
  memcpy(&f, &x, 4);
  return f;
}

float gxv_i8_encode(const float *v, signed char *q, unsigned dim)
{
  float mx = 0.0f;
  for(unsigned i = 0; i < dim; i++) {
    float a = fabsf(v[i]);
    if(a > mx) mx = a;
  }
  float s = mx / 127.0f;
  for(unsigned i = 0; i < dim; i++) {
    long r = 0;
    if(s > 0.0f) {
      r = lrintf(v[i] / s);
      if(r > 127) r = 127;
      if(r < -127) r = -127;
    }
    q[i] = (signed char)r;
  }
  return s;
}

// The table: every one of the 65536 bit patterns decoded once by
// gxv_float_from_f16(), so table lookups are exact by construction. A
// function-local static, so it is built once, safely, on first use.
namespace {
struct gxvF16Table {
  gxvF16Table() {
    for(unsigned i = 0; i < 65536; i++) v[i] = gxv_float_from_f16((unsigned short)i);
  }
  float v[65536];
};
}

static inline const float *gxv_f16_tab()
{
  static const gxvF16Table t;
  return t.v;
}

// One half from any address (memcpy: no alignment or aliasing
// assumptions; compilers turn it into a plain 2-byte load)
static inline unsigned gxv_h(const unsigned short *h, unsigned i)
{
  unsigned short v;
  memcpy(&v, (const unsigned char *)h + 2 * (size_t)i, 2);
  return v;
}

void gxv_f16_decode(const unsigned short *h, float *out, unsigned dim)
{
  const float *t = gxv_f16_tab();
  for(unsigned i = 0; i < dim; i++) out[i] = t[gxv_h(h, i)];
}

// Each block of 8 is decoded into b[] and then summed exactly as
// gxv_l2sq() / gxv_dot() sum a block, so the result is the same bits.
float gxv_l2sq_f16(const float *a, const unsigned short *h, unsigned dim)
{
  const float *t = gxv_f16_tab();
  float s[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  float b[8];
  unsigned i = 0, n8 = dim & ~7u;
#ifdef GXV_SSE2
  __m128 lo = _mm_setzero_ps(), hi = _mm_setzero_ps();
  for(; i < n8; i += 8) {
    for(unsigned j = 0; j < 8; j++) b[j] = t[gxv_h(h, i + j)];
    __m128 d0 = _mm_sub_ps(_mm_loadu_ps(a + i), _mm_loadu_ps(b));
    __m128 d1 = _mm_sub_ps(_mm_loadu_ps(a + i + 4), _mm_loadu_ps(b + 4));
    lo = _mm_add_ps(lo, _mm_mul_ps(d0, d0));
    hi = _mm_add_ps(hi, _mm_mul_ps(d1, d1));
  }
  gxv_store8(lo, hi, s);
#else
  for(; i < n8; i += 8) {
    for(unsigned j = 0; j < 8; j++) {
      b[j] = t[gxv_h(h, i + j)];
      float d = a[i + j] - b[j];
      s[j] += d * d;
    }
  }
#endif
  float r = gxv_combine(s);
  for(; i < dim; i++) {
    float d = a[i] - t[gxv_h(h, i)];
    r += d * d;
  }
  return r;
}

float gxv_dot_f16(const float *a, const unsigned short *h, unsigned dim)
{
  const float *t = gxv_f16_tab();
  float s[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  float b[8];
  unsigned i = 0, n8 = dim & ~7u;
#ifdef GXV_SSE2
  __m128 lo = _mm_setzero_ps(), hi = _mm_setzero_ps();
  for(; i < n8; i += 8) {
    for(unsigned j = 0; j < 8; j++) b[j] = t[gxv_h(h, i + j)];
    lo = _mm_add_ps(lo, _mm_mul_ps(_mm_loadu_ps(a + i), _mm_loadu_ps(b)));
    hi = _mm_add_ps(hi, _mm_mul_ps(_mm_loadu_ps(a + i + 4), _mm_loadu_ps(b + 4)));
  }
  gxv_store8(lo, hi, s);
#else
  for(; i < n8; i += 8) {
    for(unsigned j = 0; j < 8; j++) {
      b[j] = t[gxv_h(h, i + j)];
      s[j] += a[i + j] * b[j];
    }
  }
#endif
  float r = gxv_combine(s);
  for(; i < dim; i++) r += a[i] * t[gxv_h(h, i)];
  return r;
}

void gxv_i8_decode(const signed char *q, float scale, float *out, unsigned dim)
{
  for(unsigned i = 0; i < dim; i++) out[i] = (float)q[i] * scale;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
