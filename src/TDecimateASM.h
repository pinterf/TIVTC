/*
**                    TIVTC v1.0.5 for Avisynth 2.5.x
**
**   TIVTC includes a field matching filter (TFM) and a decimation
**   filter (TDecimate) which can be used together to achieve an
**   IVTC or for other uses. TIVTC currently supports YV12 and
**   YUY2 colorspaces.
**
**   Copyright (C) 2004-2008 Kevin Stone
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

#ifndef _M_X64
  void blend_MMX_8(unsigned char* dstp, const unsigned char* srcp,
    const unsigned char* nxtp, int width, int height, int dst_pitch,
    int src_pitch, int nxt_pitch, double w1, double w2);
#endif
  void blend_SSE2_16(unsigned char* dstp, const unsigned char* srcp,
    const unsigned char* nxtp, int width, int height, int dst_pitch,
    int src_pitch, int nxt_pitch, double w1, double w2);
#ifndef _M_X64
  void calcLumaDiffYUY2SSD_MMX_8(const unsigned char *prvp, const unsigned char *nxtp,
    int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &ssd);
  void calcLumaDiffYUY2SAD_ISSE_8(const unsigned char *prvp, const unsigned char *nxtp,
    int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
  void calcLumaDiffYUY2SAD_MMX_8(const unsigned char *prvp, const unsigned char *nxtp,
    int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
#endif
  void calcLumaDiffYUY2SSD_SSE2_16(const unsigned char *prvp, const unsigned char *nxtp,
    int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &ssd);
#ifndef _M_X64
  void calcLumaDiffYUY2SSD_MMX_16(const unsigned char *prvp, const unsigned char *nxtp,
    int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &ssd);
#endif

  void calcLumaDiffYUY2SAD_SSE2_16(const unsigned char *prvp, const unsigned char *nxtp,
    int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
#ifndef _M_X64
  void calcLumaDiffYUY2SAD_ISSE_16(const unsigned char *prvp, const unsigned char *nxtp,
    int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
  void calcLumaDiffYUY2SAD_MMX_16(const unsigned char *prvp, const unsigned char *nxtp,
    int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
#endif

  template<bool aligned>
  void calcSSD_SSE2_16x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);

  template<bool aligned>
  void calcSSD_SSE2_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);

  template<bool aligned>
  void calcSSD_SSE2_32x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);

  void calcSSD_SSE2_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);
  void calcSSD_SSE2_8x8(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);
  void calcSSD_SSE2_4x4(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);

#ifndef _M_X64
  void calcSSD_MMX_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);
  void calcSSD_MMX_32x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);
  void calcSSD_MMX_4x4(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);
  void calcSSD_MMX_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);
  void calcSSD_MMX_8x8(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);
  void calcSSD_MMX_16x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &ssd);
#endif

  void calcSAD_SSE2_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad); // PF new
  void calcSAD_SSE2_8x8(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad); // PF new
  void calcSAD_SSE2_4x4(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad); // PF new
  void calcSAD_SSE2_16x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_SSE2_16x16_unaligned(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  
  template<bool aligned>
  void calcSAD_SSE2_32x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);

  template<bool aligned>
  void calcSAD_SSE2_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);

#ifndef _M_X64
  void calcSAD_iSSE_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_iSSE_32x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_iSSE_4x4(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_iSSE_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_iSSE_8x8(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_iSSE_16x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
#endif

#ifndef _M_X64
  void calcSAD_MMX_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_MMX_32x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_MMX_4x4(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_MMX_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_MMX_8x8(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
  void calcSAD_MMX_16x16(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int &sad);
#endif

#ifndef _M_X64
  void blend_iSSE_5050(unsigned char* dstp, const unsigned char* srcp,
    const unsigned char* nxtp, int width, int height, int dst_pitch,
    int src_pitch, int nxt_pitch);
#endif
  void blend_SSE2_5050(unsigned char* dstp, const unsigned char* srcp,
    const unsigned char* nxtp, int width, int height, int dst_pitch,
    int src_pitch, int nxt_pitch);
#ifndef _M_X64
  void VerticalBlurMMX_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
    int dst_pitch, int width, int height);
#endif
  void VerticalBlurSSE2_R(const unsigned char *srcp, unsigned char *dstp,
    int src_pitch, int dst_pitch, int width, int height);
  void HorizontalBlurSSE2_YUY2_R_luma(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
    int dst_pitch, int width, int height);
#ifndef _M_X64
  void HorizontalBlurMMX_YUY2_R_luma(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
    int dst_pitch, int width, int height);
#endif
  void HorizontalBlurSSE2_YV12_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
    int dst_pitch, int width, int height);
#ifndef _M_X64
  void HorizontalBlurMMX_YV12_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
    int dst_pitch, int width, int height);
#endif

#ifndef _M_X64
  void HorizontalBlurMMX_YUY2_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
    int dst_pitch, int width, int height);
#endif

  void HorizontalBlurSSE2_YUY2_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
    int dst_pitch, int width, int height);

//-- helpers
  template<bool use_sse2>
  void calcDiffSAD_32x32_iSSEorSSE2(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, unsigned __int64 *diff, bool chroma);

#ifndef _M_X64
  void calcDiffSAD_32x32_MMX(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, unsigned __int64 *diff, bool chroma);
#endif

  void calcDiffSSD_32x32_MMXorSSE2(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, bool use_sse2, unsigned __int64 *diff, bool chroma);

  template<bool use_sse2>
  void calcDiffSSD_Generic_MMXorSSE2(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, unsigned __int64 *diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS);

  template<bool use_sse2>
  void calcDiffSAD_Generic_MMXorSSE2(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, unsigned __int64 *diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS);

#ifndef _M_X64
  void calcDiffSAD_Generic_iSSE(const unsigned char *ptr1, const unsigned char *ptr2,
    int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, unsigned __int64 *diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS);
#endif


#endif // __TDECIMATEASM_H__