/*
**                TDeinterlace v1.2 for Avisynth 2.6 interface
**
**   TDeinterlace is a bi-directionally motion adaptive deinterlacer.
**   It also uses a couple modified forms of ela interpolation which
**   help to reduce "jaggy" edges in places where interpolation must
**   be used. TDeinterlace currently supports YV12 and YUY2 colorspaces.
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

#ifndef __TDEINTASM_H__
#define __TDEINTASM_H__

#include <windows.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include "internal.h"

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
  
  template<int blockSizeY>
  void compute_sum_8xN_sse2(const unsigned char *srcp, int pitch, int &sum);

  void compute_sum_16x8_sse2_luma(const unsigned char *srcp, int pitch, int &sum);

#endif // __TDEINTASM_H__
