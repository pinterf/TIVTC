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

#include "TDecimate.h"

#ifdef _M_X64
#define USE_INTR
#undef ALLOW_MMX
#else
#define USE_INTR
#define ALLOW_MMX
#undef ALLOW_MMX
#endif


void TDecimate::blurFrame(PVideoFrame &src, PVideoFrame &dst, int np, int iterations,
  bool bchroma, IScriptEnvironment *env, VideoInfo& vi_t, int opti)
{
  PVideoFrame tmp = env->NewVideoFrame(vi_t);
  HorizontalBlur(src, tmp, np, bchroma, env, vi_t, opti);
  VerticalBlur(tmp, dst, np, bchroma, env, vi_t, opti);
  for (int i = 1; i < iterations; ++i)
  {
    HorizontalBlur(dst, tmp, np, bchroma, env, vi_t, opti);
    VerticalBlur(tmp, dst, np, bchroma, env, vi_t, opti);
  }
}

void TDecimate::HorizontalBlur(PVideoFrame &src, PVideoFrame &dst, int np, bool bchroma,
  IScriptEnvironment *env, VideoInfo& vi_t, int opti)
{
  int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  if (vi_t.IsYV12() && !bchroma) np = 1;
  long cpu = env->GetCPUFlags();
  if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
  if (opti != 4)
  {
    if (opti == 0) cpu &= ~0x2C;
    else if (opti == 1) { cpu &= ~0x28; cpu |= 0x04; }
    else if (opti == 2) { cpu &= ~0x20; cpu |= 0x0C; }
    else if (opti == 3) cpu |= 0x2C;
  }
  for (int b = 0; b < np; ++b)
  {
    const unsigned char *srcp = src->GetReadPtr(plane[b]);
    int src_pitch = src->GetPitch(plane[b]);
    int width = src->GetRowSize(plane[b]);
    int widtha = (width >> 3) << 3;
    int height = src->GetHeight(plane[b]);
    unsigned char *dstp = dst->GetWritePtr(plane[b]);
    int dst_pitch = dst->GetPitch(plane[b]), x, y;
    if (vi_t.IsYV12())
    {
      if ((cpu&CPUF_SSE2) && width >= 8)
      {
        // always mod 8, sse2 unaligned!
        HorizontalBlurMMXorSSE2_YV12<true>(srcp, dstp, src_pitch, dst_pitch, widtha, height);
        if (widtha != width)
        {
          for (y = 0; y < height; ++y)
          {
            for (x = widtha; x < width - 1; ++x) dstp[x] = (srcp[x - 1] + (srcp[x] << 1) + srcp[x + 1] + 2) >> 2;
            if (x != width - 1) x = width - 1;
            dstp[x] = (srcp[x - 1] + srcp[x] + 1) >> 1;
            srcp += src_pitch;
            dstp += dst_pitch;
          }
        }
      }
#ifdef ALLOW_MMX
      else if ((cpu&CPUF_MMX) && width >= 8)
      {
        HorizontalBlurMMXorSSE2_YV12<false>(srcp, dstp, src_pitch, dst_pitch, widtha, height);
        if (widtha != width)
        {
          for (y = 0; y < height; ++y)
          {
            for (x = widtha; x < width - 1; ++x) dstp[x] = (srcp[x - 1] + (srcp[x] << 1) + srcp[x + 1] + 2) >> 2;
            if (x != width - 1) x = width - 1;
            dstp[x] = (srcp[x - 1] + srcp[x] + 1) >> 1;
            srcp += src_pitch;
            dstp += dst_pitch;
          }
        }
      }
#endif
      else
      {
        for (y = 0; y < height; ++y)
        {
          dstp[0] = (srcp[0] + srcp[1] + 1) >> 1;
          for (x = 1; x < width - 1; ++x) dstp[x] = (srcp[x - 1] + (srcp[x] << 1) + srcp[x + 1] + 2) >> 2;
          dstp[x] = (srcp[x - 1] + srcp[x] + 1) >> 1;
          srcp += src_pitch;
          dstp += dst_pitch;
        }
      }
    }
    else
    {
      if (bchroma)
      {
#ifndef ALLOW_MMX
        if ((cpu&CPUF_SSE2) && width >= 8)
        {
          HorizontalBlurMMXorSSE2_YUY2<true>(srcp, dstp, src_pitch, dst_pitch, widtha, height);
          if (width != widtha)
          {
            for (y = 0; y < height; ++y)
            {
              for (x = widtha; x < width - 4; ++x)
              {
                dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2;
                ++x;
                dstp[x] = (srcp[x - 4] + (srcp[x] << 1) + srcp[x + 4] + 2) >> 2;
              }
              if (x != width - 4) x = width - 4;
              dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2; ++x;
              dstp[x] = (srcp[x - 4] + srcp[x] + 1) >> 1; ++x;
              dstp[x] = (srcp[x - 2] + srcp[x] + 1) >> 1; ++x;
              dstp[x] = (srcp[x - 4] + srcp[x] + 1) >> 1;
              srcp += src_pitch;
              dstp += dst_pitch;
            }
          }
        }
#else
        if ((cpu&CPUF_MMX) && width >= 8)
        {
          HorizontalBlurMMXorSSE2_YUY2<false>(srcp, dstp, src_pitch, dst_pitch, widtha, height);
          if (width != widtha)
          {
            for (y = 0; y < height; ++y)
            {
              for (x = widtha; x < width - 4; ++x)
              {
                dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2;
                ++x;
                dstp[x] = (srcp[x - 4] + (srcp[x] << 1) + srcp[x + 4] + 2) >> 2;
              }
              if (x != width - 4) x = width - 4;
              dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2; ++x;
              dstp[x] = (srcp[x - 4] + srcp[x] + 1) >> 1; ++x;
              dstp[x] = (srcp[x - 2] + srcp[x] + 1) >> 1; ++x;
              dstp[x] = (srcp[x - 4] + srcp[x] + 1) >> 1;
              srcp += src_pitch;
              dstp += dst_pitch;
            }
          }
        }
#endif
        else
        {
          for (y = 0; y < height; ++y)
          {
            dstp[0] = (srcp[0] + srcp[2] + 1) >> 1;
            dstp[1] = (srcp[1] + srcp[5] + 1) >> 1;
            dstp[2] = (srcp[0] + (srcp[2] << 1) + srcp[4] + 2) >> 2;
            dstp[3] = (srcp[3] + srcp[7] + 1) >> 1;
            for (x = 4; x < width - 4; ++x)
            {
              dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2;
              ++x;
              dstp[x] = (srcp[x - 4] + (srcp[x] << 1) + srcp[x + 4] + 2) >> 2;
            }
            dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2; ++x;
            dstp[x] = (srcp[x - 4] + srcp[x] + 1) >> 1; ++x;
            dstp[x] = (srcp[x - 2] + srcp[x] + 1) >> 1; ++x;
            dstp[x] = (srcp[x - 4] + srcp[x] + 1) >> 1;
            srcp += src_pitch;
            dstp += dst_pitch;
          }
        }
      }
      else
      {
#ifndef ALLOW_MMX
        if ((cpu&CPUF_SSE2) && width >= 8)
        {
          HorizontalBlurMMXorSSE2_YUY2_luma<true>(srcp, dstp, src_pitch, dst_pitch, widtha, height);
          if (width != widtha)
          {
            for (y = 0; y < height; ++y)
            {
              for (x = widtha; x < width - 2; x += 2)
                dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2;
              if (x != width - 2) x = width - 2;
              dstp[x] = (srcp[x - 2] + srcp[x] + 1) >> 1;
              srcp += src_pitch;
              dstp += dst_pitch;
            }
          }
        }
#else
        if ((cpu&CPUF_MMX) && width >= 8)
        {
          HorizontalBlurMMXorSSE2_YUY2_luma<false>(srcp, dstp, src_pitch, dst_pitch, widtha, height);
          if (width != widtha)
          {
            for (y = 0; y < height; ++y)
            {
              for (x = widtha; x < width - 2; x += 2)
                dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2;
              if (x != width - 2) x = width - 2;
              dstp[x] = (srcp[x - 2] + srcp[x] + 1) >> 1;
              srcp += src_pitch;
              dstp += dst_pitch;
            }
          }
        }
#endif
        else
        {
          for (y = 0; y < height; ++y)
          {
            dstp[0] = (srcp[0] + srcp[2] + 1) >> 1;
            for (x = 2; x < width - 2; x += 2) dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2;
            dstp[x] = (srcp[x - 2] + srcp[x] + 1) >> 1;
            srcp += src_pitch;
            dstp += dst_pitch;
          }
        }
      }
    }
  }
}

void TDecimate::VerticalBlur(PVideoFrame &src, PVideoFrame &dst, int np, bool bchroma,
  IScriptEnvironment *env, VideoInfo& vi_t, int opti)
{
  int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  if (vi_t.IsYV12() && !bchroma) np = 1;
  long cpu = env->GetCPUFlags();
  if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
  if (opti != 4)
  {
    if (opti == 0) cpu &= ~0x2C;
    else if (opti == 1) { cpu &= ~0x28; cpu |= 0x04; }
    else if (opti == 2) { cpu &= ~0x20; cpu |= 0x0C; }
    else if (opti == 3) cpu |= 0x2C;
  }
  for (int b = 0; b < np; ++b)
  {
    const unsigned char *srcp = src->GetReadPtr(plane[b]);
    int src_pitch = src->GetPitch(plane[b]);
    int width = src->GetRowSize(plane[b]);
    int widtha = (width >> 3) << 3;
    int widtha2 = (width >> 4) << 4;
    int height = src->GetHeight(plane[b]);
    unsigned char *dstp = dst->GetWritePtr(plane[b]);
    int dst_pitch = dst->GetPitch(plane[b]);
    if ((cpu&CPUF_MMX) && width >= 8)
    {
      if ((cpu&CPUF_SSE2) && !((intptr_t(srcp) | intptr_t(dstp) | src_pitch | dst_pitch) & 15) && widtha2 >= 16)
      {
        VerticalBlurSSE2(srcp, dstp, src_pitch, dst_pitch, widtha2, height);
        widtha = widtha2;
      }
#ifdef ALLOW_MMX
      else if (cpu&CPUF_MMX)
        VerticalBlurMMX(srcp, dstp, src_pitch, dst_pitch, widtha, height);
#endif
      else 
        env->ThrowError("TDecimate: simd error in VerticalBlur");
      if (width != widtha)
      {
        int inc = (vi_t.IsYUY2() && !bchroma) ? 2 : 1;
        const unsigned char *srcpp = srcp - src_pitch;
        const unsigned char *srcpn = srcp + src_pitch;
        for (int x = widtha; x < width; x += inc) dstp[x] = (srcp[x] + srcpn[x] + 1) >> 1;
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        dstp += dst_pitch;
        for (int y = 1; y < height - 1; ++y)
        {
          for (int x = widtha; x < width; x += inc) dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
          srcpp += src_pitch;
          srcp += src_pitch;
          srcpn += src_pitch;
          dstp += dst_pitch;
        }
        for (int x = widtha; x < width; x += inc) dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
      }
    }
    else
    {
      int inc = (vi_t.IsYUY2() && !bchroma) ? 2 : 1;
      const unsigned char *srcpp = srcp - src_pitch;
      const unsigned char *srcpn = srcp + src_pitch;
      for (int x = 0; x < width; x += inc) dstp[x] = (srcp[x] + srcpn[x] + 1) >> 1;
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      dstp += dst_pitch;
      for (int y = 1; y < height - 1; ++y)
      {
        for (int x = 0; x < width; x += inc) dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        dstp += dst_pitch;
      }
      for (int x = 0; x < width; x += inc) dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
    }
  }
}

// always mod 8, sse2 unaligned
template<bool use_sse2>
void TDecimate::HorizontalBlurMMXorSSE2_YV12(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  if(use_sse2)
    HorizontalBlurSSE2_YV12_R(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
#ifndef _M_X64
  else
    HorizontalBlurMMX_YV12_R(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
#endif
  for (int y = 0; y < height; ++y)
  {
    dstp[0] = (srcp[0] + srcp[1] + 1) >> 1;
    dstp[1] = (srcp[0] + (srcp[1] << 1) + srcp[2] + 2) >> 2;
    dstp[2] = (srcp[1] + (srcp[2] << 1) + srcp[3] + 2) >> 2;
    dstp[3] = (srcp[2] + (srcp[3] << 1) + srcp[4] + 2) >> 2;
    for (int x = 4; x < 8; ++x)
      dstp[x] = (srcp[x - 1] + (srcp[x] << 1) + srcp[x + 1] + 2) >> 2;
    for (int x = width - 8; x < width - 4; ++x)
      dstp[x] = (srcp[x - 1] + (srcp[x] << 1) + srcp[x + 1] + 2) >> 2;
    dstp[width - 4] = (srcp[width - 5] + (srcp[width - 4] << 1) + srcp[width - 3] + 2) >> 2;
    dstp[width - 3] = (srcp[width - 4] + (srcp[width - 3] << 1) + srcp[width - 2] + 2) >> 2;
    dstp[width - 2] = (srcp[width - 3] + (srcp[width - 2] << 1) + srcp[width - 1] + 2) >> 2;
    dstp[width - 1] = (srcp[width - 2] + srcp[width - 1] + 1) >> 1;
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

template<bool use_sse2>
void TDecimate::HorizontalBlurMMXorSSE2_YUY2_luma(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
#ifdef _M_X64
  HorizontalBlurSSE2_YUY2_R_luma(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
#else
  if (use_sse2)
    HorizontalBlurSSE2_YUY2_R_luma(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
  else
    HorizontalBlurMMX_YUY2_R_luma(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
#endif

  for (int y = 0; y < height; ++y)
  {
    dstp[0] = (srcp[0] + srcp[2] + 1) >> 1;
    dstp[2] = (srcp[0] + (srcp[2] << 1) + srcp[4] + 2) >> 2;
    dstp[4] = (srcp[2] + (srcp[4] << 1) + srcp[6] + 2) >> 2;
    dstp[6] = (srcp[4] + (srcp[6] << 1) + srcp[8] + 2) >> 2;
    dstp[width - 8] = (srcp[width - 10] + (srcp[width - 8] << 1) + srcp[width - 6] + 2) >> 2;
    dstp[width - 6] = (srcp[width - 8] + (srcp[width - 6] << 1) + srcp[width - 4] + 2) >> 2;
    dstp[width - 4] = (srcp[width - 6] + (srcp[width - 4] << 1) + srcp[width - 2] + 2) >> 2;
    dstp[width - 2] = (srcp[width - 4] + srcp[width - 2] + 1) >> 1;
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

template<bool use_sse2>
void TDecimate::HorizontalBlurMMXorSSE2_YUY2(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
#ifdef _M_X64
  HorizontalBlurSSE2_YUY2_R(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
#else
  if (use_sse2)
    HorizontalBlurSSE2_YUY2_R(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
  else
    HorizontalBlurMMX_YUY2_R(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
#endif
  for (int y = 0; y < height; ++y)
  {
    dstp[0] = (srcp[0] + srcp[2] + 1) >> 1;
    dstp[1] = (srcp[1] + srcp[5] + 1) >> 1;
    dstp[2] = (srcp[0] + (srcp[2] << 1) + srcp[4] + 2) >> 2;
    dstp[3] = (srcp[3] + srcp[7] + 1) >> 1;
    dstp[4] = (srcp[2] + (srcp[4] << 1) + srcp[6] + 2) >> 2;
    dstp[5] = (srcp[1] + (srcp[5] << 1) + srcp[9] + 2) >> 2;
    dstp[6] = (srcp[4] + (srcp[6] << 1) + srcp[8] + 2) >> 2;
    dstp[7] = (srcp[3] + (srcp[7] << 1) + srcp[11] + 2) >> 2;
    dstp[width - 8] = (srcp[width - 10] + (srcp[width - 8] << 1) + srcp[width - 6] + 2) >> 2;
    dstp[width - 7] = (srcp[width - 11] + (srcp[width - 7] << 1) + srcp[width - 3] + 2) >> 2;
    dstp[width - 6] = (srcp[width - 8] + (srcp[width - 6] << 1) + srcp[width - 4] + 2) >> 2;
    dstp[width - 5] = (srcp[width - 9] + (srcp[width - 5] << 1) + srcp[width - 1] + 2) >> 2;
    dstp[width - 4] = (srcp[width - 6] + (srcp[width - 4] << 1) + srcp[width - 2] + 2) >> 2;
    dstp[width - 3] = (srcp[width - 7] + srcp[width - 3] + 1) >> 1;
    dstp[width - 2] = (srcp[width - 4] + srcp[width - 2] + 1) >> 1;
    dstp[width - 1] = (srcp[width - 5] + srcp[width - 1] + 1) >> 1;
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

#ifndef _M_X64
void TDecimate::VerticalBlurMMX(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  VerticalBlurMMX_R(srcp + src_pitch, dstp + dst_pitch, src_pitch, dst_pitch, width, height - 2);
  int temps = (height - 1)*src_pitch, tempd = (height - 1)*dst_pitch;
  for (int x = 0; x < width; ++x)
  {
    dstp[x] = (srcp[x] + srcp[x + src_pitch] + 1) >> 1;
    dstp[tempd + x] = (srcp[temps + x] + srcp[temps + x - src_pitch] + 1) >> 1;
  }
}
#endif

void TDecimate::VerticalBlurSSE2(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  VerticalBlurSSE2_R(srcp + src_pitch, dstp + dst_pitch, src_pitch, dst_pitch, width, height - 2);
  int temps = (height - 1)*src_pitch, tempd = (height - 1)*dst_pitch;
  for (int x = 0; x < width; ++x)
  {
    dstp[x] = (srcp[x] + srcp[x + src_pitch] + 1) >> 1;
    dstp[tempd + x] = (srcp[temps + x] + srcp[temps + x - src_pitch] + 1) >> 1;
  }
}

#ifndef _M_X64
__declspec(align(16)) const __int64 twos_mmx[2] = { 0x0002000200020002, 0x0002000200020002 };
__declspec(align(16)) const __int64 chroma_mask = 0xFF00FF00FF00FF00;
__declspec(align(16)) const __int64 luma_mask = 0x00FF00FF00FF00FF;
#endif

// always mod 8, sse2 unaligned!
void TDecimate::HorizontalBlurSSE2_YV12_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(0x0002); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 8) {
      __m128i left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 1));
      __m128i center = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i right = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x + 1));
      __m128i left_lo = _mm_unpacklo_epi8(left, zero);
      __m128i center_lo = _mm_unpacklo_epi8(center, zero);
      __m128i right_lo = _mm_unpacklo_epi8(right, zero);
      __m128i left_hi = _mm_unpackhi_epi8(left, zero);
      __m128i center_hi = _mm_unpackhi_epi8(center, zero);
      __m128i right_hi = _mm_unpackhi_epi8(right, zero);
      
      // (center*2 + left + right + 2) >> 2
      __m128i centermul2_lo = _mm_slli_epi16(center_lo, 1);
      __m128i centermul2_hi = _mm_slli_epi16(center_hi, 1);
      auto res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      auto res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two),2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two),2);
      __m128i res = _mm_packus_epi16(res_lo, res_hi);
      _mm_storel_epi64(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

#ifndef _M_X64
void TDecimate::HorizontalBlurMMX_YV12_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __asm
  {
    mov eax, srcp
    mov ebx, dstp
    mov edx, width
    mov esi, src_pitch
    mov edi, dst_pitch
    movq mm6, twos_mmx
    pxor mm7, mm7
    yloop :
    xor ecx, ecx
      align 16
      xloop :
      movq mm0, [eax + ecx - 1]
      movq mm1, [eax + ecx]
      movq mm4, [eax + ecx + 1]
      movq mm2, mm0
      movq mm3, mm1
      movq mm5, mm4
      punpcklbw mm0, mm7
      punpcklbw mm1, mm7
      punpcklbw mm4, mm7
      punpckhbw mm2, mm7
      punpckhbw mm3, mm7
      punpckhbw mm5, mm7
      psllw mm1, 1
      psllw mm3, 1
      paddw mm1, mm0
      paddw mm3, mm2
      paddw mm1, mm4
      paddw mm3, mm5
      paddw mm1, mm6
      paddw mm3, mm6
      psrlw mm1, 2
      psrlw mm3, 2
      packuswb mm1, mm3
      movq[ebx + ecx], mm1
      add ecx, 8
      cmp ecx, edx
      jl xloop
      add eax, esi
      add ebx, edi
      dec height
      jnz yloop
      emms
  }
}
#endif

#ifndef _M_X64
void TDecimate::HorizontalBlurMMX_YUY2_R_luma(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __asm
  {
    mov eax, srcp
    mov ebx, dstp
    mov edx, width
    mov esi, src_pitch
    mov edi, dst_pitch
    movq mm6, twos_mmx
    pxor mm7, mm7
    yloop :
    xor ecx, ecx
      align 16
      xloop :
      movq mm0, [eax + ecx - 2]
      movq mm1, [eax + ecx]
      movq mm4, [eax + ecx + 2]
      movq mm2, mm0
      movq mm3, mm1
      movq mm5, mm4
      punpcklbw mm0, mm7
      punpcklbw mm1, mm7
      punpcklbw mm4, mm7
      punpckhbw mm2, mm7
      punpckhbw mm3, mm7
      punpckhbw mm5, mm7
      psllw mm1, 1
      psllw mm3, 1
      paddw mm1, mm0
      paddw mm3, mm2
      paddw mm1, mm4
      paddw mm3, mm5
      paddw mm1, mm6
      paddw mm3, mm6
      psrlw mm1, 2
      psrlw mm3, 2
      packuswb mm1, mm3
      movq[ebx + ecx], mm1
      add ecx, 8
      cmp ecx, edx
      jl xloop
      add eax, esi
      add ebx, edi
      dec height
      jnz yloop
      emms
  }
}
#endif

void TDecimate::HorizontalBlurSSE2_YUY2_R_luma(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(0x0002); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 8) {
      __m128i left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 2)); // same as Y12 but +/-2 instead of +/-1
      __m128i center = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i right = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x + 2));
      __m128i left_lo = _mm_unpacklo_epi8(left, zero);
      __m128i center_lo = _mm_unpacklo_epi8(center, zero);
      __m128i right_lo = _mm_unpacklo_epi8(right, zero);
      __m128i left_hi = _mm_unpackhi_epi8(left, zero);
      __m128i center_hi = _mm_unpackhi_epi8(center, zero);
      __m128i right_hi = _mm_unpackhi_epi8(right, zero);

      // (center*2 + left + right + 2) >> 2
      __m128i centermul2_lo = _mm_slli_epi16(center_lo, 1);
      __m128i centermul2_hi = _mm_slli_epi16(center_hi, 1);
      auto res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      auto res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two),2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two),2);
      __m128i res = _mm_packus_epi16(res_lo, res_hi);

      _mm_storel_epi64(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

#ifndef _M_X64
void TDecimate::HorizontalBlurMMX_YUY2_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __asm
  {
    mov eax, srcp
    mov ebx, dstp
    mov edx, width
    mov esi, src_pitch
    mov edi, dst_pitch
    pxor mm7, mm7
    yloop :
    xor ecx, ecx
      align 16
      xloop :
      movq mm0, [eax + ecx - 2]
      movq mm1, [eax + ecx]
      movq mm4, [eax + ecx + 2]
      movq mm2, mm0
      movq mm3, mm1
      movq mm5, mm4
      movq mm6, mm3
      punpcklbw mm0, mm7
      punpcklbw mm1, mm7
      punpcklbw mm4, mm7
      punpckhbw mm2, mm7
      punpckhbw mm3, mm7
      punpckhbw mm5, mm7
      psllw mm1, 1
      psllw mm3, 1
      paddw mm1, mm0
      paddw mm3, mm2
      paddw mm1, mm4
      paddw mm3, mm5
      movq mm0, [eax + ecx - 4]
      movq mm4, [eax + ecx + 4]
      paddw mm1, twos_mmx
      paddw mm3, twos_mmx
      psrlw mm1, 2
      psrlw mm3, 2
      movq mm2, mm6
      movq mm5, mm0
      packuswb mm1, mm3
      movq mm3, mm4
      punpcklbw mm0, mm7
      punpcklbw mm6, mm7
      punpcklbw mm4, mm7
      punpckhbw mm5, mm7
      punpckhbw mm2, mm7
      punpckhbw mm3, mm7
      psllw mm6, 1
      psllw mm2, 1
      paddw mm6, mm0
      paddw mm2, mm5
      paddw mm6, mm4
      paddw mm2, mm3
      paddw mm6, twos_mmx
      paddw mm2, twos_mmx
      psrlw mm6, 2
      psrlw mm2, 2
      packuswb mm6, mm2
      pand mm1, luma_mask
      pand mm6, chroma_mask
      por mm1, mm6
      movq[ebx + ecx], mm1
      add ecx, 8
      cmp ecx, edx
      jl xloop
      add eax, esi
      add ebx, edi
      dec height
      jnz yloop
      emms
  }
}
#endif

// todo to sse2, mod 8 always, unaligned
void TDecimate::HorizontalBlurSSE2_YUY2_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(2); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 8) {
      // luma part
      __m128i left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 2)); // same as Y12 but +/-2 instead of +/-1
      __m128i center = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i right = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x + 2));
      __m128i left_lo = _mm_unpacklo_epi8(left, zero);
      __m128i center_lo = _mm_unpacklo_epi8(center, zero);
      __m128i right_lo = _mm_unpacklo_epi8(right, zero);
      __m128i left_hi = _mm_unpackhi_epi8(left, zero);
      __m128i center_hi = _mm_unpackhi_epi8(center, zero);
      __m128i right_hi = _mm_unpackhi_epi8(right, zero);

      // (center*2 + left + right + 2) >> 2
      __m128i centermul2_lo = _mm_slli_epi16(center_lo, 1);
      __m128i centermul2_hi = _mm_slli_epi16(center_hi, 1);
      auto res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      auto res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two), 2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two), 2);
      __m128i res1 = _mm_packus_epi16(res_lo, res_hi);

      // chroma part
      left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 4)); // same as Y12 but +/-2 instead of +/-1
      // we have already filler center 
      right = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x + 4));
      left_lo = _mm_unpacklo_epi8(left, zero);
      center_lo = _mm_unpacklo_epi8(center, zero);
      right_lo = _mm_unpacklo_epi8(right, zero);
      left_hi = _mm_unpackhi_epi8(left, zero);
      center_hi = _mm_unpackhi_epi8(center, zero);
      right_hi = _mm_unpackhi_epi8(right, zero);

      // (center*2 + left + right + 2) >> 2
      centermul2_lo = _mm_slli_epi16(center_lo, 1);
      centermul2_hi = _mm_slli_epi16(center_hi, 1);
      res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two), 2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two), 2);
      __m128i res2 = _mm_packus_epi16(res_lo, res_hi);

      __m128i chroma_mask = _mm_set1_epi16((short)0xFF00);
      __m128i luma_mask = _mm_set1_epi16(0x00FF);

      res1 = _mm_and_si128(res1, luma_mask);
      res2 = _mm_and_si128(res1, chroma_mask);
      __m128i res = _mm_or_si128(res1, res2);

      _mm_storel_epi64(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

#ifndef _M_X64
void TDecimate::VerticalBlurMMX_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __asm
  {
    mov eax, srcp
    mov ebx, dstp
    mov edx, width
    mov esi, src_pitch
    mov edi, esi
    add edi, edi
    add eax, esi
    movq mm6, twos_mmx
    pxor mm7, mm7
    yloop :
    xor ecx, ecx
      align 16
      xloop :
      sub eax, edi
      movq mm0, [eax + ecx]
      add eax, esi
      movq mm1, [eax + ecx]
      add eax, esi
      movq mm4, [eax + ecx]
      movq mm2, mm0
      movq mm3, mm1
      movq mm5, mm4
      punpcklbw mm0, mm7
      punpcklbw mm1, mm7
      punpcklbw mm4, mm7
      punpckhbw mm2, mm7
      punpckhbw mm3, mm7
      punpckhbw mm5, mm7
      psllw mm1, 1
      psllw mm3, 1
      paddw mm1, mm0
      paddw mm3, mm2
      paddw mm1, mm4
      paddw mm3, mm5
      paddw mm1, mm6
      paddw mm3, mm6
      psrlw mm1, 2
      psrlw mm3, 2
      packuswb mm1, mm3
      movq[ebx + ecx], mm1
      add ecx, 8
      cmp ecx, edx
      jl xloop
      add eax, esi
      add ebx, dst_pitch
      dec height
      jnz yloop
      emms
  }
}
#endif

void TDecimate::VerticalBlurSSE2_R(const unsigned char *srcp, unsigned char *dstp,
  int src_pitch, int dst_pitch, int width, int height)
{
#ifdef USE_INTR
  __m128i two = _mm_set1_epi16(0x0002); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      __m128i left = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x - src_pitch));
      __m128i center = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i right = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x + src_pitch));
      __m128i left_lo = _mm_unpacklo_epi8(left, zero);
      __m128i center_lo = _mm_unpacklo_epi8(center, zero);
      __m128i right_lo = _mm_unpacklo_epi8(right, zero);
      __m128i left_hi = _mm_unpackhi_epi8(left, zero);
      __m128i center_hi = _mm_unpackhi_epi8(center, zero);
      __m128i right_hi = _mm_unpackhi_epi8(right, zero);

      // (center*2 + left + right + 2) >> 2
      __m128i centermul2_lo = _mm_slli_epi16(center_lo, 1);
      __m128i centermul2_hi = _mm_slli_epi16(center_hi, 1);
      auto res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      auto res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two), 2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two), 2);
      __m128i res = _mm_packus_epi16(res_lo, res_hi);

      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
#else
  __asm
  {
    mov eax, srcp
    mov ebx, dstp
    mov edx, width
    mov esi, src_pitch
    mov edi, esi
    add edi, edi
    add eax, esi
    movdqa xmm6, twos_mmx
    pxor xmm7, xmm7
    yloop :
    xor ecx, ecx
      align 16
      xloop :
      sub eax, edi
      movdqa xmm0, [eax + ecx]
      add eax, esi
      movdqa xmm1, [eax + ecx]
      add eax, esi
      movdqa xmm4, [eax + ecx]
      movdqa xmm2, xmm0
      movdqa xmm3, xmm1
      movdqa xmm5, xmm4
      punpcklbw xmm0, xmm7
      punpcklbw xmm1, xmm7
      punpcklbw xmm4, xmm7
      punpckhbw xmm2, xmm7
      punpckhbw xmm3, xmm7
      punpckhbw xmm5, xmm7
      psllw xmm1, 1
      psllw xmm3, 1
      paddw xmm1, xmm0
      paddw xmm3, xmm2
      paddw xmm1, xmm4
      paddw xmm3, xmm5
      paddw xmm1, xmm6
      paddw xmm3, xmm6
      psrlw xmm1, 2
      psrlw xmm3, 2
      packuswb xmm1, xmm3
      movdqa[ebx + ecx], xmm1
      add ecx, 16
      cmp ecx, edx
      jl xloop
      add eax, esi
      add ebx, dst_pitch
      dec height
      jnz yloop
  }
#endif
}