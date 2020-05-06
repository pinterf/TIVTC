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
#ifndef __TFMASM_H__
#define __TFMASM_H__

#include <windows.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include "internal.h"

  void checkSceneChangeYUY2_2_SSE2(const unsigned char *prvp, const unsigned char *srcp,
    const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch,
    int nxt_pitch, unsigned long &diffp, unsigned long &diffn);
  void checkSceneChangeYUY2_1_SSE2(const unsigned char *prvp, const unsigned char *srcp,
    int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp);
  void checkSceneChangeYV12_1_SSE2(const unsigned char *prvp, const unsigned char *srcp,
    int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp);
  void checkSceneChangeYV12_2_SSE2(const unsigned char *prvp, const unsigned char *srcp,
    const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch,
    int nxt_pitch, unsigned long &diffp, unsigned long &diffn);

#if 0
  check in TDeint -> common
  void check_combing_SSE2(const unsigned char *srcp, unsigned char *dstp,
    int width, int height, int src_pitch, int src_pitch2, int dst_pitch, __m128i threshb,
    __m128i thresh6w);
  
  void check_combing_SSE2_Luma(const unsigned char *srcp, unsigned char *dstp,
    int width, int height, int src_pitch, int src_pitch2, int dst_pitch, __m128i threshb,
    __m128i thresh6w);
  
  void check_combing_SSE2_M1(const unsigned char *srcp, unsigned char *dstp,
    int width, int height, int src_pitch, int dst_pitch, __m128i thresh);
  
  void check_combing_SSE2_Luma_M1(const unsigned char *srcp, unsigned char *dstp,
    int width, int height, int src_pitch, int dst_pitch, __m128i thresh);
  
  void buildABSDiffMask_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
    unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);
  
  void buildABSDiffMask2_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
    unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);
#endif

#if 0
  // buggy + template + from common with TDeint place
  void compute_sum_8x8_sse2(const unsigned char *srcp, int pitch, int &sum);
#endif

#if 0
  // buggy + template + from common with TDeint place
  template<bool aligned>
  void compute_sum_16x8_sse2_luma(const unsigned char *srcp, int pitch, int &sum);
#endif

#endif // TFMASM_H__
