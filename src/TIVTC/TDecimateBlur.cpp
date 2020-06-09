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

#include "TDecimate.h"
#include "TDecimateASM.h"

// hbd ready
void blurFrame(PVideoFrame &src, PVideoFrame &dst, int iterations,
  bool bchroma, IScriptEnvironment *env, VideoInfo& vi_t, int cpuFlags)
{
  PVideoFrame tmp = env->NewVideoFrame(vi_t);
  HorizontalBlur(src, tmp, bchroma, vi_t, cpuFlags);
  VerticalBlur(tmp, dst, bchroma, vi_t, cpuFlags);
  for (int i = 1; i < iterations; ++i)
  {
    HorizontalBlur(dst, tmp, bchroma, vi_t, cpuFlags);
    VerticalBlur(tmp, dst, bchroma, vi_t, cpuFlags);
  }
}

void HorizontalBlur(PVideoFrame &src, PVideoFrame &dst, bool bchroma,
  VideoInfo& vi_t, int cpuFlags)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };

  const int np = (vi_t.IsYUY2() || vi_t.IsY() || !bchroma) ? 1 : 3; // luma only (!chroma) only 1 planar planes

  const bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

  const int pixelsize = vi_t.ComponentSize();

  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const uint8_t *srcp = src->GetReadPtr(plane);
    int src_pitch = src->GetPitch(plane);
    int width = src->GetRowSize(plane) / pixelsize;
    int widtha = (width >> 3) << 3; // mod 8
    int height = src->GetHeight(plane);
    uint8_t *dstp = dst->GetWritePtr(plane);
    int dst_pitch = dst->GetPitch(plane);
    if (vi_t.IsPlanar())
    {
      if (pixelsize == 1 && use_sse2 && width >= 8)
      {
        // always mod 8, sse2 unaligned!
        HorizontalBlur_Planar_SSE2(srcp, dstp, src_pitch, dst_pitch, widtha, height);
        // rest non mod 8 no the right
        HorizontalBlur_Planar_c<uint8_t>(srcp + widtha, dstp + widtha, src_pitch, dst_pitch, width - widtha, height, true);
      }
      else
      {
        // fixme: implement SIMD for 10-16 bits
        if(pixelsize == 1)
          HorizontalBlur_Planar_c<uint8_t>(srcp, dstp, src_pitch, dst_pitch, width, height, false);
        else // 10-16 bits
          HorizontalBlur_Planar_c<uint16_t>(srcp, dstp, src_pitch, dst_pitch, width, height, false);
      }
    }
    else
    {
      // YUY2
      if (bchroma)
      {
        if (use_sse2 && width >= 8)
        {
          HorizontalBlur_YUY2_SSE2(srcp, dstp, src_pitch, dst_pitch, widtha, height);
          // rest non mod 8 no the right
          HorizontalBlur_YUY2_c(srcp + widtha, dstp + widtha, src_pitch, dst_pitch, width - widtha, height, true);
        }
        else
        {
          // YUY2 luma-chroma C
          HorizontalBlur_YUY2_c(srcp, dstp, src_pitch, dst_pitch, width, height, false);
        }
      }
      else
      {
        // YUY2 luma only
        if (use_sse2 && width >= 8)
        {
          HorizontalBlur_YUY2_lumaonly_SSE2(srcp, dstp, src_pitch, dst_pitch, widtha, height);
          // rest non mod 8 no the right
          HorizontalBlur_YUY2_lumaonly_c(srcp + widtha, dstp + widtha, src_pitch, dst_pitch, width - widtha, height, true);
        }
        else
        {
          // rest non mod 8 no the right
          HorizontalBlur_YUY2_lumaonly_c(srcp, dstp, src_pitch, dst_pitch, width, height, false);
        }
      }
    }
  }
}

template<typename pixel_t>
void VerticalBlur_c(const uint8_t* srcp0, uint8_t* dstp0, int src_pitch,
  int dst_pitch, int width, int height)
{
  if (width == 0) return;

  pixel_t* dstp = reinterpret_cast<pixel_t *>(dstp0);
  const pixel_t* srcp = reinterpret_cast<const pixel_t*>(srcp0);
  const pixel_t* srcpp = reinterpret_cast<const pixel_t*>(srcp0 - src_pitch);
  const pixel_t* srcpn = reinterpret_cast<const pixel_t*>(srcp0 + src_pitch);
  src_pitch /= sizeof(pixel_t);
  dst_pitch /= sizeof(pixel_t);

  // top line
  for (int x = 0; x < width; x++)
    dstp[x] = (srcp[x] + srcpn[x] + 1) >> 1;
  srcpp += src_pitch;
  srcp += src_pitch;
  srcpn += src_pitch;
  dstp += dst_pitch;
  // height - 2 lines in between
  for (int y = 1; y < height - 1; ++y)
  {
    for (int x = 0; x < width; x++)
      dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    dstp += dst_pitch;
  }
  // bottom line
  for (int x = 0; x < width; x++)
    dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
}

void VerticalBlur_YUY2_c(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height, int inc)
{
  if (width == 0) return;

  const uint8_t* srcpp = srcp - src_pitch;
  const uint8_t* srcpn = srcp + src_pitch;
  // top line
  for (int x = 0; x < width; x += inc)
    dstp[x] = (srcp[x] + srcpn[x] + 1) >> 1;
  srcpp += src_pitch;
  srcp += src_pitch;
  srcpn += src_pitch;
  dstp += dst_pitch;
  // height - 2 lines in between
  for (int y = 1; y < height - 1; ++y)
  {
    for (int x = 0; x < width; x += inc)
      dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    dstp += dst_pitch;
  }
  // bottom line
  for (int x = 0; x < width; x += inc)
    dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
}

void VerticalBlur_SSE2(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  VerticalBlurSSE2_R(srcp + src_pitch, dstp + dst_pitch, src_pitch, dst_pitch, width, height - 2);
  int temps = (height - 1) * src_pitch;
  int tempd = (height - 1) * dst_pitch;
  for (int x = 0; x < width; ++x)
  {
    dstp[x] = (srcp[x] + srcp[x + src_pitch] + 1) >> 1;
    dstp[tempd + x] = (srcp[temps + x] + srcp[temps + x - src_pitch] + 1) >> 1;
  }
}

void VerticalBlur(PVideoFrame& src, PVideoFrame& dst, bool bchroma,
  VideoInfo& vi_t, int cpuFlags)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };

  const int np = (vi_t.IsYUY2() || vi_t.IsY() || !bchroma) ? 1 : 3; // luma only (!chroma) only 1 planar planes

  const bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

  const int pixelsize = vi_t.ComponentSize();

  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const uint8_t* srcp = src->GetReadPtr(plane);
    int src_pitch = src->GetPitch(plane);
    int width = src->GetRowSize(plane) / pixelsize;
    int widtha = (width >> 4) << 4; // mod 16
    int height = src->GetHeight(plane);
    uint8_t* dstp = dst->GetWritePtr(plane);
    int dst_pitch = dst->GetPitch(plane);

    if (vi_t.IsYUY2()) {
      const int inc = !bchroma ? 2 : 1; // YUY2 luma only: step 2 on 1st plane
      if (use_sse2 && widtha >= 16)
      {
        // 16x block is Ok
        // note: SSE2 version ignores chroma-only, it's quicker without it
        // chroma result is n/a in this use case of blur
        VerticalBlur_SSE2(srcp, dstp, src_pitch, dst_pitch, widtha, height);
        VerticalBlur_YUY2_c(srcp + widtha, dstp + widtha, src_pitch, dst_pitch, width - widtha, height, inc);
      }
      else {
        VerticalBlur_YUY2_c(srcp, dstp, src_pitch, dst_pitch, width, height, inc);
      }
    }
    else {
      if (pixelsize == 1 && use_sse2 && widtha >= 16)
      {
        // 16x block is Ok
        VerticalBlur_SSE2(srcp, dstp, src_pitch, dst_pitch, widtha, height);
        //the rest on the right not covered by SIMD
        VerticalBlur_c<uint8_t>(srcp + widtha, dstp + widtha, src_pitch, dst_pitch, width - widtha, height);
      }
      else {
        // fixme: implement SIMD for 10-16 bits
        if(pixelsize == 1)
          VerticalBlur_c<uint8_t>(srcp, dstp, src_pitch, dst_pitch, width, height);
        else // 10-16 bits
          VerticalBlur_c<uint16_t>(srcp, dstp, src_pitch, dst_pitch, width, height);
      }
    }
  }
}

template<typename pixel_t>
void HorizontalBlur_Planar_c(const uint8_t* srcp0, uint8_t* dstp0, int src_pitch,
  int dst_pitch, int width, int height, bool allow_leftminus1)
{
  if (width == 0)
    return;

  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp0);
  const pixel_t* srcp = reinterpret_cast<const pixel_t*>(srcp0);
  src_pitch /= sizeof(pixel_t);
  dst_pitch /= sizeof(pixel_t);

  if (width >= 2) {
    const int startx = allow_leftminus1 ? 0 : 1;
    for (int y = 0; y < height; ++y)
    {
      if (!allow_leftminus1)
        dstp[0] = (srcp[0] + srcp[1] + 1) >> 1;
      int x;
      for (x = startx; x < width - 1; ++x)
        dstp[x] = (srcp[x - 1] + (srcp[x] << 1) + srcp[x + 1] + 2) >> 2;
      dstp[x] = (srcp[x - 1] + srcp[x] + 1) >> 1;
      srcp += src_pitch;
      dstp += dst_pitch;
    }
    return;
  }

  // width == 1
  for (int y = 0; y < height; ++y)
  {
    if (allow_leftminus1)
      dstp[0] = (srcp[-1] + srcp[0] + 1) >> 1;
    else
      dstp[0] = srcp[0];
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

void HorizontalBlur_YUY2_lumaonly_c(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height, bool allow_leftminus1)
{
  if (width == 0)
    return;

  // YUYV minimum width is 4, at least two luma
  const int startx = allow_leftminus1 ? 0 : 2;
  for (int y = 0; y < height; ++y)
  {
    if (!allow_leftminus1)
      dstp[0] = (srcp[0] + srcp[2] + 1) >> 1;
    int x;
    for (x = startx; x < width - 2; ++x)
      dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2;
    dstp[x] = (srcp[x - 2] + srcp[x] + 1) >> 1;
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

void HorizontalBlur_YUY2_c(const uint8_t* srcp, uint8_t* dstp, int src_pitch,
  int dst_pitch, int width, int height, bool allow_leftminus1)
{
  // width is rowwidth
  if (width == 0)
    return;

  // YUYV minimum rowsize is 4, at least two luma
  const int startx = allow_leftminus1 ? 0 : 4;

  if (width >= 8) {
    for (int y = 0; y < height; ++y)
    {
      if (!allow_leftminus1) {
        dstp[0] = (srcp[-2] + (srcp[0] << 1) + srcp[2] + 2) >> 2; // Y
        dstp[1] = (srcp[-3] + (srcp[1] << 1) + srcp[5] + 2) >> 2; // U
        dstp[2] = (srcp[0] + (srcp[2] << 1) + srcp[4] + 2) >> 2; // Y
        dstp[3] = (srcp[-1] + (srcp[3] << 1) + srcp[7] + 2) >> 2; // V
      }
      int x;
      for (x = startx; x < width - 4; ++x)
      {
        dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2; // Y
        ++x;
        dstp[x] = (srcp[x - 4] + (srcp[x] << 1) + srcp[x + 4] + 2) >> 2; // U or V
      }
      dstp[x] = (srcp[x - 2] + (srcp[x] << 1) + srcp[x + 2] + 2) >> 2; // Y
      ++x;
      dstp[x] = (srcp[x - 4] + srcp[x] + 1) >> 1; // U
      ++x;
      dstp[x] = (srcp[x - 2] + srcp[x] + 1) >> 1; // Y
      ++x;
      dstp[x] = (srcp[x - 4] + srcp[x] + 1) >> 1; // V
      srcp += src_pitch;
      dstp += dst_pitch;
    }
    return;
  }

  // width (rowsize) == 4
  for (int y = 0; y < height; ++y)
  {
    if (allow_leftminus1) {
      dstp[0] = (srcp[-2] + (srcp[0] << 1) + srcp[2] + 2) >> 2; // Y
      dstp[1] = (srcp[-3] + srcp[1] + 1) >> 1; // U
      dstp[2] = (srcp[0] + srcp[2] + 1) >> 1; // Y
      dstp[3] = (srcp[-1] + srcp[3] + 1) >> 1; // V
    }
    else {
      dstp[0] = (srcp[0] + srcp[2] + 1) >> 1; // Y
      dstp[1] = srcp[1]; // U
      dstp[2] = (srcp[0] + srcp[2] + 1) >> 1; // Y
      dstp[3] = srcp[3]; // V
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }

}

// always mod 8, sse2 unaligned
void HorizontalBlur_Planar_SSE2(const uint8_t *srcp, uint8_t *dstp, int src_pitch,
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

void HorizontalBlur_YUY2_lumaonly_SSE2(const uint8_t *srcp, uint8_t *dstp, int src_pitch,
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

void HorizontalBlur_YUY2_SSE2(const uint8_t *srcp, uint8_t *dstp, int src_pitch,
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
