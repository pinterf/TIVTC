/*
**   Helper methods for TIVTC and TDeint
**
**
**   Copyright (C) 2004-2007 Kevin Stone, additional work (C) 2020 pinterf
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __TCOMMONASM_H__
#define __TCOMMONASM_H__

#include "internal.h"
#include <xmmintrin.h>
#include <emmintrin.h>
#include <algorithm>

template<int bits_per_pixel>
AVS_FORCEINLINE int cubicInt(int p1, int p2, int p3, int p4)
{
  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  const int temp = (19 * (p2 + p3) - 3 * (p1 + p4) + 16) >> 5;
  return std::min(std::max(temp, 0), max_pixel_value);
}

void absDiff_SSE2(const uint8_t* srcp1, const uint8_t* srcp2,
  uint8_t* dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width,
  int height, int mthresh1, int mthresh2);

void absDiff_c(const uint8_t* srcp1, const uint8_t* srcp2,
  uint8_t* dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width,
  int height, int mthresh1, int mthresh2);

void absDiff_uint16_c(const uint8_t* srcp1, const uint8_t* srcp2,
  uint8_t* dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width,
  int height, int mthresh);

template<typename pixel_t, bool YUY2_LumaOnly>
void check_combing_c(const pixel_t* srcp, uint8_t* dstp, int width, int height, int src_pitch, int dst_pitch, int cthresh);


template<typename pixel_t, bool YUY2_LumaOnly, typename safeint_t>
void check_combing_c_Metric1(const pixel_t* srcp, uint8_t* dstp, int width, int height, int src_pitch, int dst_pitch, safeint_t cthreshsq);

void check_combing_SSE2(const uint8_t *srcp, uint8_t *dstp,
  int width, int height, int src_pitch, int dst_pitch, int cthresh);

void check_combing_YUY2LumaOnly_SSE2(const uint8_t *srcp, uint8_t *dstp,
  int width, int height, int src_pitch, int dst_pitch, int cthresh);

#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif 
void check_combing_uint16_SSE4(const uint16_t* srcp, uint8_t* dstp, int width, int height, int src_pitch, int dst_pitch, int cthresh);

void check_combing_SSE2_Metric1(const uint8_t *srcp, uint8_t *dstp,
  int width, int height, int src_pitch, int dst_pitch, int cthreshsq);
  
void check_combing_SSE2_Luma_Metric1(const uint8_t *srcp, uint8_t *dstp,
  int width, int height, int src_pitch, int dst_pitch, int cthreshsq);

template<typename pixel_t>
void buildABSDiffMask_SSE2(const uint8_t *prvp, const uint8_t *nxtp,
  uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);

template<typename pixel_t, bool YUY2_LumaOnly>
void buildABSDiffMask_c(const uint8_t* prvp, const uint8_t* nxtp,
  uint8_t* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);

template<typename pixel_t>
void do_buildABSDiffMask(const uint8_t* prvp, const uint8_t* nxtp, uint8_t* tbuffer,
  int prv_pitch, int nxt_pitch, int tpitch, int width, int height, bool YUY2_LumaOnly, int cpuFlags);

template<typename pixel_t, int bits_per_pixel>
void AnalyzeDiffMask_Planar(uint8_t* dstp, int dst_pitch, uint8_t* tbuffer, int tpitch, int Width, int Height);
void AnalyzeDiffMask_YUY2(uint8_t* dstp, int dst_pitch, uint8_t* tbuffer, int tpitch, int Width, int Height, bool mChroma);


void buildABSDiffMask2_uint8_SSE2(const uint8_t *prvp, const uint8_t *nxtp,
  uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);

void buildABSDiffMask2_uint16_SSE2(const uint8_t* prvp, const uint8_t* nxtp,
  uint8_t* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height, int bits_per_pixel);

template<typename pixel_t, bool YUY2_LumaOnly>
void buildABSDiffMask2_c(const uint8_t* prvp, const uint8_t* nxtp,
  uint8_t* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height, int bits_per_pixel);

template<typename pixel_t>
void do_buildABSDiffMask2(const uint8_t* prvp, const uint8_t* nxtp, uint8_t* dstp,
  int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height, bool YUY2_LumaOnly, int cpuFlags, int bits_per_pixel);


template<int blockSizeY>
void compute_sum_8xN_sse2(const uint8_t *srcp, int pitch, int &sum);

void compute_sum_16x8_sse2_luma(const uint8_t *srcp, int pitch, int &sum);

// fixme: put non-asm utility functions into different file
void copyFrame(PVideoFrame& dst, PVideoFrame& src, const VideoInfo& vi, IScriptEnvironment* env);

template<typename pixel_t>
void blend_5050_SSE2(uint8_t* dstp, const uint8_t* srcp1, const uint8_t* srcp2, int width, int height, int dst_pitch, int src1_pitch, int src2_pitch);
template<typename pixel_t>
void blend_5050_c(uint8_t* dstp, const uint8_t* srcp1, const uint8_t* srcp2, int width, int height, int dst_pitch, int src1_pitch, int src2_pitch);

template<int planarType>
void do_FillCombedPlanarUpdateCmaskByUV(uint8_t* cmkp, uint8_t* cmkpU, uint8_t* cmkpV, int Width, int Height, ptrdiff_t cmk_pitch, ptrdiff_t cmk_pitchUV);

#endif // __TCOMMONASM_H__
