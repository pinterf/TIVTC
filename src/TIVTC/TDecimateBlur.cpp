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

void blurFrame(PVideoFrame &src, PVideoFrame &dst, int np, int iterations,
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

void HorizontalBlur(PVideoFrame &src, PVideoFrame &dst, int np, bool bchroma,
  IScriptEnvironment *env, VideoInfo& vi_t, int opti)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  if (vi_t.IsPlanar() && !bchroma) np = 1; // luma only

  long cpu = env->GetCPUFlags();
  if (opti == 0) cpu = 0;
  bool use_sse2 = (cpu & CPUF_SSE2) ? true : false;

  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const unsigned char *srcp = src->GetReadPtr(plane);
    int src_pitch = src->GetPitch(plane);
    int width = src->GetRowSize(plane);
    int widtha = (width >> 3) << 3; // mod 8
    int height = src->GetHeight(plane);
    unsigned char *dstp = dst->GetWritePtr(plane);
    int dst_pitch = dst->GetPitch(plane), x, y;
    if (vi_t.IsPlanar())
    {
      if ((cpu&CPUF_SSE2) && width >= 8)
      {
        // always mod 8, sse2 unaligned!
        HorizontalBlur_SSE2_Planar(srcp, dstp, src_pitch, dst_pitch, widtha, height);
        // rest non mod 8
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
      else
      {
        // full C
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
      // YUY2
      if (bchroma)
      {
        if ((cpu&CPUF_SSE2) && width >= 8)
        {
          HorizontalBlur_SSE2_YUY2(srcp, dstp, src_pitch, dst_pitch, widtha, height);
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
        else
        {
          // YUY2 luma-chroma C
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
        // YUY2 luma only
        if ((cpu&CPUF_SSE2) && width >= 8)
        {
          HorizontalBlur_SSE2_YUY2_lumaonly(srcp, dstp, src_pitch, dst_pitch, widtha, height);
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
        else
        {
          // YUY2 luma only C
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

void VerticalBlur(PVideoFrame &src, PVideoFrame &dst, int np, bool bchroma,
  IScriptEnvironment *env, VideoInfo& vi_t, int opti)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  if (vi_t.IsPlanar() && !bchroma) np = 1; // luma only

  long cpu = env->GetCPUFlags();
  if (opti == 0) cpu = 0;
  bool use_sse2 = (cpu & CPUF_SSE2) ? true : false;

  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const unsigned char* srcp = src->GetReadPtr(plane);
    int src_pitch = src->GetPitch(plane);
    int width = src->GetRowSize(plane);
    int widtha;
    int widtha2 = (width >> 4) << 4; // mod 16
    int height = src->GetHeight(plane);
    unsigned char* dstp = dst->GetWritePtr(plane);
    int dst_pitch = dst->GetPitch(plane);

    if (cpu & CPUF_SSE2 && widtha2 >= 16)
    {
      // 16x block is Ok
      VerticalBlurSSE2(srcp, dstp, src_pitch, dst_pitch, widtha2, height);
      widtha = widtha2;
    }
    else {
      widtha = 0;
    }
    // C. Full width or the rest on the right not covered by SIMD
    if (width != widtha)
    {
      const int inc = (vi_t.IsYUY2() && !bchroma) ? 2 : 1; // YUY2 luma only: step 2 on 1st plane
      const unsigned char* srcpp = srcp - src_pitch;
      const unsigned char* srcpn = srcp + src_pitch;
      // top line
      for (int x = widtha; x < width; x += inc)
        dstp[x] = (srcp[x] + srcpn[x] + 1) >> 1;
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      dstp += dst_pitch;
      // height - 2 lines in between
      for (int y = 1; y < height - 1; ++y)
      {
        for (int x = widtha; x < width; x += inc)
          dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        dstp += dst_pitch;
      }
      // bottom line
      for (int x = widtha; x < width; x += inc)
        dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
    }
  }
}

// always mod 8, sse2 unaligned
// fixme: this one is always sse2
void HorizontalBlur_SSE2_Planar(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  // left and right 8 pixel is omitted in SIMD, special
  HorizontalBlurSSE2_Planar_R(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
  for (int y = 0; y < height; ++y)
  {
    dstp[0] = (srcp[0] + srcp[1] + 1) >> 1;
    dstp[1] = (srcp[0] + (srcp[1] << 1) + srcp[2] + 2) >> 2;
    dstp[2] = (srcp[1] + (srcp[2] << 1) + srcp[3] + 2) >> 2;
    dstp[3] = (srcp[2] + (srcp[3] << 1) + srcp[4] + 2) >> 2;
    // 4-7
    for (int x = 4; x < 8; ++x)
      dstp[x] = (srcp[x - 1] + (srcp[x] << 1) + srcp[x + 1] + 2) >> 2;
    for (int x = width - 8; x < width - 4; ++x)
      dstp[x] = (srcp[x - 1] + (srcp[x] << 1) + srcp[x + 1] + 2) >> 2;
    // -8..-5
    dstp[width - 4] = (srcp[width - 5] + (srcp[width - 4] << 1) + srcp[width - 3] + 2) >> 2;
    dstp[width - 3] = (srcp[width - 4] + (srcp[width - 3] << 1) + srcp[width - 2] + 2) >> 2;
    dstp[width - 2] = (srcp[width - 3] + (srcp[width - 2] << 1) + srcp[width - 1] + 2) >> 2;
    dstp[width - 1] = (srcp[width - 2] + srcp[width - 1] + 1) >> 1;
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

// fixme: always sse2
void HorizontalBlur_SSE2_YUY2_lumaonly(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  HorizontalBlurSSE2_YUY2_R_luma(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);

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

void HorizontalBlur_SSE2_YUY2(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  HorizontalBlurSSE2_YUY2_R(srcp + 8, dstp + 8, src_pitch, dst_pitch, width - 16, height);
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


void VerticalBlurSSE2(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
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

