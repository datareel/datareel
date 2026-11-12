// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxvkern.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
// Date Last Modified: 11/12/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
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

Distance kernels for the vector index (gxvindex.h). Plain C-style
functions over raw float buffers, so the DataReel 6.x version shares
this code word for word (no std:: types here).

Every kernel sums in a fixed order: 8 running partial sums (element i
goes to sum i % 8), combined as ((s0+s4)+(s2+s6))+((s1+s5)+(s3+s7)),
then the tail. A SIMD version must keep this order (one 8-lane
register is exactly this), so results are bit-identical with SIMD on
and off and on every node.

Changes:
==============================================================
11/12/2026: New in DataReel 7.4 (vector extensions, phase A).
11/12/2026: Phase C: float16 and int8 element conversions.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_VECTOR_KERNELS_HPP__
#define __GX_VECTOR_KERNELS_HPP__

#include "gxdlcode.h"

namespace datareel {

// Squared Euclidean distance
GXDLCODE_API float gxv_l2sq(const float *a, const float *b, unsigned dim);

// Dot product
GXDLCODE_API float gxv_dot(const float *a, const float *b, unsigned dim);

// Scale v to unit length in place. Returns the length before scaling;
// a zero vector is left unchanged and 0 is returned.
GXDLCODE_API float gxv_normalize(float *v, unsigned dim);

// 1 if every element is a finite number
GXDLCODE_API int gxv_finite(const float *v, unsigned dim);

// Element types (phase C). IEEE binary16: round to nearest, ties to
// even, subnormals kept; values above 65504 in size become infinity
// (gxVectorIndex refuses them first with gxv_OUT_OF_RANGE).
GXDLCODE_API unsigned short gxv_f16_from_float(float f);
GXDLCODE_API float gxv_float_from_f16(unsigned short h);

// int8, symmetric per vector: s = max|v| / 127 (0 for a zero vector),
// q[i] = clamp(lrintf(v[i] / s), -127, 127). Returns s. Decoding gives
// q[i] * s, within s / 2 of v[i].
GXDLCODE_API float gxv_i8_encode(const float *v, signed char *q, unsigned dim);
GXDLCODE_API void gxv_i8_decode(const signed char *q, float scale, float *out,
				unsigned dim);

} // namespace datareel

#endif // __GX_VECTOR_KERNELS_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
