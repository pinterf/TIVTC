/*
**                    TIVTC for AviSynth 2.6 interface
**
**   TIVTC includes a field matching filter (TFM) and a decimation
**   filter (TDecimate) which can be used together to achieve an
**   IVTC or for other uses. TIVTC currently supports 8 bit planar YUV and
**   YUY2 colorspaces.
**
**   Copyright (C) 2004-2008 Kevin Stone, additional work (C) 2020 pinterf
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

#ifndef __TDECIMATEASM_H__
#define __TDECIMATEASM_H__

#include <xmmintrin.h>
#include <emmintrin.h>
#include "internal.h"

void HorizontalBlurSSE2_YUY2_R_luma(const uint8_t* srcp, uint8_t* dstp, int src_pitch, int dst_pitch, int width, int height);
void HorizontalBlurSSE2_YUY2_R(const uint8_t* srcp, uint8_t* dstp, int src_pitch, int dst_pitch, int width, int height);
void VerticalBlurSSE2_R(const uint8_t* srcp, uint8_t* dstp, int src_pitch, int dst_pitch, int width, int height);
void HorizontalBlurSSE2_Planar_R(const uint8_t* srcp, uint8_t* dstp, int src_pitch, int dst_pitch, int width, int height);

// used for YUY2
void calcLumaDiffYUY2SSD_SSE2_16(const uint8_t* prvp, const uint8_t* nxtp,
  int width, int height, int prv_pitch, int nxt_pitch, uint64_t& ssd);
void calcLumaDiffYUY2SAD_SSE2_16(const uint8_t* prvp, const uint8_t* nxtp,
  int width, int height, int prv_pitch, int nxt_pitch, uint64_t& sad);
void calcSSD_SSE2_32x16_YUY2_lumaonly(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& ssd);
void calcSSD_SSE2_32x16(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& ssd);
void calcSSD_SSE2_8x8_YUY2_lumaonly(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& ssd);
void calcSAD_SSE2_32x16(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& sad);
void calcSAD_SSE2_32x16_YUY2_lumaonly(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& sad);
void calcSAD_SSE2_8x8_YUY2_lumaonly(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& sad); // PF new


// generic
template<int blkSizeY>
void calcSSD_SSE2_16xN(const uint8_t *ptr1, const uint8_t *ptr2, int pitch1, int pitch2, int &ssd);
template<int blkSizeY>
void calcSSD_SSE2_8xN(const uint8_t *ptr1, const uint8_t *ptr2, int pitch1, int pitch2, int &ssd);
template<int blkSizeY>
void calcSSD_SSE2_4xN(const uint8_t *ptr1, const uint8_t *ptr2, int pitch1, int pitch2, int &ssd);

template<int blkSizeY>
void calcSAD_SSE2_8xN(const uint8_t *ptr1, const uint8_t *ptr2, int pitch1, int pitch2, int &sad); 
template<int blkSizeY>
void calcSAD_SSE2_4xN(const uint8_t *ptr1, const uint8_t *ptr2, int pitch1, int pitch2, int &sad);
template<int blkSizeY>
void calcSAD_SSE2_16xN(const uint8_t *ptr1, const uint8_t *ptr2, int pitch1, int pitch2, int &sad);


//-- helpers
void calcDiffSAD_32x32_SSE2(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t *diff, bool chroma, const VideoInfo& vi);

void calcDiffSSD_32x32_SSE2(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t *diff, bool chroma, const VideoInfo& vi);

void calcDiffSSD_Generic_SSE2(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t *diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, const VideoInfo& vi);

void calcDiffSAD_Generic_SSE2(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t *diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, const VideoInfo& vi);

template<typename pixel_t, bool SAD, int inc>
void calcDiff_SADorSSD_Generic_c(const pixel_t* prvp, const pixel_t* curp,
  int prv_pitch, int cur_pitch, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, int nt, const VideoInfo& vi);

void CalcMetricsExtracted(IScriptEnvironment* env, PVideoFrame& prevt, PVideoFrame& currt, CalcMetricData& d);

template<typename pixel_t>
void HorizontalBlur_Planar_c(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height, bool allow_leftminus1);
void HorizontalBlur_YUY2_lumaonly_c(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height, bool allow_leftminus1);
void HorizontalBlur_YUY2_c(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height, bool allow_leftminus1);

void HorizontalBlur_Planar_SSE2(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height);
void HorizontalBlur_YUY2_lumaonly_SSE2(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height);
void HorizontalBlur_YUY2_SSE2(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height);

void HorizontalBlur(PVideoFrame& src, PVideoFrame& dst, bool bchroma,
  VideoInfo& vi_t, int opti);

template<typename pixel_t>
void VerticalBlur_c(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height);
void VerticalBlur_YUY2_c(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height, int inc);

void VerticalBlur_SSE2(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height);

void VerticalBlur(PVideoFrame& src, PVideoFrame& dst, bool bchroma,
  VideoInfo& vi_t, int opti);


// handles 50% special case as well
void dispatch_blend(uint8_t* dstp, const uint8_t* srcp1, const uint8_t* srcp2, int width, int height,
  int dst_pitch, int src1_pitch, int src2_pitch, int weight_i, int bits_per_pixel, int cpuFlags);

#endif // __TDECIMATEASM_H__
