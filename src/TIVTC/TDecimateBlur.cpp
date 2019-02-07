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

#include "TDecimate.h"
#include "TDecimateASM.h"

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
#ifdef ALLOW_MMX
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
#ifndef ALLOW_MMX
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
#ifndef ALLOW_MMX
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

#ifdef ALLOW_MMX
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

