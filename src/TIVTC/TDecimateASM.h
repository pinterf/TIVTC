/*
**                    TIVTC v1.0.14 for Avisynth 2.6 interface
**
**   TIVTC includes a field matching filter (TFM) and a decimation
**   filter (TDecimate) which can be used together to achieve an
**   IVTC or for other uses. TIVTC currently supports YV12 and
**   YUY2 colorspaces.
**
**   Copyright (C) 2004-2008 Kevin Stone, additional work (C) 2017 pinterf
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

#include <windows.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include "internal.h"

// used for YUY2
void calcLumaDiffYUY2SSD_SSE2_16(const unsigned char* prvp, const unsigned char* nxtp,
  int width, int height, int prv_pitch, int nxt_pitch, uint64_t& ssd);
void calcLumaDiffYUY2SAD_SSE2_16(const unsigned char* prvp, const unsigned char* nxtp,
  int width, int height, int prv_pitch, int nxt_pitch, uint64_t& sad);
void calcSSD_SSE2_32x16_YUY2_lumaonly(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int& ssd);
void calcSSD_SSE2_32x16(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int& ssd);
void calcSSD_SSE2_8x8_YUY2_lumaonly(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int& ssd);
void calcSAD_SSE2_32x16(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int& sad);
void calcSAD_SSE2_32x16_YUY2_lumaonly(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int& sad);
void calcSAD_SSE2_8x8_YUY2_lumaonly(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int& sad); // PF new
void HorizontalBlurSSE2_YUY2_R_luma(const unsigned char* srcp, unsigned char* dstp, int src_pitch,
  int dst_pitch, int width, int height);
void HorizontalBlurSSE2_YUY2_R(const unsigned char* srcp, unsigned char* dstp, int src_pitch,
  int dst_pitch, int width, int height);

// generic
template<int blkSizeY>
void calcSSD_SSE2_16xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &ssd);
template<int blkSizeY>
void calcSSD_SSE2_8xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &ssd);
template<int blkSizeY>
void calcSSD_SSE2_4xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &ssd);

template<int blkSizeY>
void calcSAD_SSE2_8xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &sad); 
template<int blkSizeY>
void calcSAD_SSE2_4xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &sad);
template<int blkSizeY>
void calcSAD_SSE2_16xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &sad);

void blend_SSE2_16(unsigned char* dstp, const unsigned char* srcp,
  const unsigned char* nxtp, int width, int height, int dst_pitch,
  int src_pitch, int nxt_pitch, double w1, double w2);
void blend_SSE2_5050(unsigned char* dstp, const unsigned char* srcp,
  const unsigned char* nxtp, int width, int height, int dst_pitch,
  int src_pitch, int nxt_pitch);

void VerticalBlurSSE2_R(const unsigned char *srcp, unsigned char *dstp,
  int src_pitch, int dst_pitch, int width, int height);
void HorizontalBlurSSE2_Planar_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height);


//-- helpers
void calcDiffSAD_32x32_SSE2(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t *diff, bool chroma);

void calcDiffSSD_32x32_SSE2(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t *diff, bool chroma);

void calcDiffSSD_Generic_SSE2(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t *diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS);

void calcDiffSAD_Generic_SSE2(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t *diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS);

void CalcMetricsExtracted(IScriptEnvironment* env, PVideoFrame& prevt, int np, PVideoFrame& currt, CalcMetricData& d);

void VerticalBlurSSE2(const unsigned char* srcp, unsigned char* dstp, int src_pitch,
  int dst_pitch, int width, int height);

void HorizontalBlur_SSE2_Planar(const unsigned char* srcp, unsigned char* dstp, int src_pitch,
  int dst_pitch, int width, int height);

void HorizontalBlur_SSE2_YUY2_lumaonly(const unsigned char* srcp, unsigned char* dstp, int src_pitch,
  int dst_pitch, int width, int height);

void VerticalBlur(PVideoFrame& src, PVideoFrame& dst, int np, bool bchroma,
  IScriptEnvironment* env, VideoInfo& vi_t, int opti);

void HorizontalBlur(PVideoFrame& src, PVideoFrame& dst, int np, bool bchroma,
  IScriptEnvironment* env, VideoInfo& vi_t, int opti);

void HorizontalBlur_SSE2_YUY2(const unsigned char* srcp, unsigned char* dstp, int src_pitch,
  int dst_pitch, int width, int height);

#endif // __TDECIMATEASM_H__
