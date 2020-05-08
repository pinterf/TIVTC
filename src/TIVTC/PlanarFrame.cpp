/*
**   My PlanarFrame class... fast mmx/sse2 YUY2 packed to planar and planar
**   to packed conversions, and always gives 16 bit alignment for all
**   planes.  Supports YV12/YUY2 frames from avisynth, can do any planar format
**   internally.
**
**   Copyright (C) 2005-2006 Kevin Stone
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

#include "PlanarFrame.h"
#include <avs/cpuid.h>
#include <intrin.h>
#include <stdint.h>


PlanarFrame::PlanarFrame(int cpuFlags)
{
  ypitch = uvpitch = 0;
  ywidth = uvwidth = 0;
  yheight = uvheight = 0;
  y = u = v = NULL;
  useSIMD = true;
  packed = false;
  cpu = cpuFlags;
}

PlanarFrame::PlanarFrame(VideoInfo &viInfo, int cpuFlags)
{
  ypitch = uvpitch = 0;
  ywidth = uvwidth = 0;
  yheight = uvheight = 0;
  y = u = v = NULL;
  useSIMD = true;
  packed = false;
  cpu = cpuFlags;
  allocSpace(viInfo);
}

PlanarFrame::PlanarFrame(VideoInfo &viInfo, bool _packed, int cpuFlags)
{
  ypitch = uvpitch = 0;
  ywidth = uvwidth = 0;
  yheight = uvheight = 0;
  y = u = v = NULL;
  useSIMD = true;
  packed = _packed;
  cpu = cpuFlags;
  allocSpace(viInfo);
}

PlanarFrame::~PlanarFrame()
{
  if (y != NULL) { _aligned_free(y); y = NULL; }
  if (u != NULL) { _aligned_free(u); u = NULL; }
  if (v != NULL) { _aligned_free(v); v = NULL; }
}

bool PlanarFrame::allocSpace(VideoInfo &viInfo)
{
  if (y != NULL) { _aligned_free(y); y = NULL; }
  if (u != NULL) { _aligned_free(u); u = NULL; }
  if (v != NULL) { _aligned_free(v); v = NULL; }
  int height = viInfo.height;
  int width = viInfo.width;
  if (viInfo.IsPlanar())
  {
    ypitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT - (width%MIN_ALIGNMENT));
    ywidth = width;
    yheight = height;
    width >>= viInfo.GetPlaneWidthSubsampling(PLANAR_U);
    height >>= viInfo.GetPlaneHeightSubsampling(PLANAR_U);
    uvpitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT - (width%MIN_ALIGNMENT));
    uvwidth = width;
    uvheight = height;
    y = (unsigned char*)_aligned_malloc(ypitch*yheight, MIN_ALIGNMENT);
    if (y == NULL) return false;
    u = (unsigned char*)_aligned_malloc(uvpitch*uvheight, MIN_ALIGNMENT);
    if (u == NULL) return false;
    v = (unsigned char*)_aligned_malloc(uvpitch*uvheight, MIN_ALIGNMENT);
    if (v == NULL) return false;
    return true;
  }
  else if (viInfo.IsYUY2())
  {
    if (!packed)
    {
      ypitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT - (width%MIN_ALIGNMENT));
      ywidth = width;
      yheight = height;
      width >>= 1;
      uvpitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT - (width%MIN_ALIGNMENT));
      uvwidth = width;
      uvheight = height;
      y = (unsigned char*)_aligned_malloc(ypitch*yheight, MIN_ALIGNMENT);
      if (y == NULL) return false;
      u = (unsigned char*)_aligned_malloc(uvpitch*uvheight, MIN_ALIGNMENT);
      if (u == NULL) return false;
      v = (unsigned char*)_aligned_malloc(uvpitch*uvheight, MIN_ALIGNMENT);
      if (v == NULL) return false;
      return true;
    }
    else
    {
      width *= 2;
      ypitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT - (width%MIN_ALIGNMENT));
      ywidth = width;
      yheight = height;
      y = (unsigned char*)_aligned_malloc(ypitch*yheight, MIN_ALIGNMENT);
      if (y == NULL) return false;
      uvpitch = uvwidth = uvheight = 0;
      u = v = NULL;
      return true;
    }
  }
  return false;
}

bool PlanarFrame::allocSpace(int specs[4])
{
  if (y != NULL) { _aligned_free(y); y = NULL; }
  if (u != NULL) { _aligned_free(u); u = NULL; }
  if (v != NULL) { _aligned_free(v); v = NULL; }
  int height = specs[0];
  int width = specs[2];
  ypitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT - (width%MIN_ALIGNMENT));
  ywidth = width;
  yheight = height;
  height = specs[1];
  width = specs[3];
  uvpitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT - (width%MIN_ALIGNMENT));
  uvwidth = width;
  uvheight = height;
  y = (unsigned char*)_aligned_malloc(ypitch*yheight, MIN_ALIGNMENT);
  if (y == NULL) return false;
  u = (unsigned char*)_aligned_malloc(uvpitch*uvheight, MIN_ALIGNMENT);
  if (u == NULL) return false;
  v = (unsigned char*)_aligned_malloc(uvpitch*uvheight, MIN_ALIGNMENT);
  if (v == NULL) return false;
  return true;
}


void PlanarFrame::createPlanar(int yheight, int uvheight, int ywidth, int uvwidth)
{
  int specs[4] = { yheight, uvheight, ywidth, uvwidth };
  allocSpace(specs);
}

void PlanarFrame::createPlanar(int height, int width, int chroma_format)
{
  int specs[4];
  if (chroma_format <= PLANAR_420) // 420
  {
    specs[0] = height; specs[1] = height >> 1;
    specs[2] = width; specs[3] = width >> 1;
  }
  else if (chroma_format == PLANAR_422) // 422
  {
    specs[0] = height; specs[1] = height;
    specs[2] = width; specs[3] = width >> 1;
  }
  else if (chroma_format == PLANAR_444) // 444
  {
    specs[0] = height; specs[1] = height;
    specs[2] = width; specs[3] = width;
  }
  else if (chroma_format == PLANAR_411) // 411
  {
    specs[0] = height; specs[1] = height;
    specs[2] = width; specs[3] = width >> 2;
  }
  allocSpace(specs);
}

void PlanarFrame::createFromProfile(VideoInfo &viInfo)
{
  allocSpace(viInfo);
}

void PlanarFrame::createFromFrame(PVideoFrame &frame, VideoInfo &viInfo)
{
  allocSpace(viInfo);
  copyInternalFrom(frame, viInfo);
}

void PlanarFrame::createFromPlanar(PlanarFrame &frame)
{
  int specs[4] = { frame.yheight, frame.uvheight, frame.ywidth, frame.uvwidth };
  allocSpace(specs);
  copyInternalFrom(frame);
}

void PlanarFrame::copyFrom(PVideoFrame &frame, VideoInfo &viInfo)
{
  copyInternalFrom(frame, viInfo);
}

void PlanarFrame::copyFrom(PlanarFrame &frame)
{
  copyInternalFrom(frame);
}

void PlanarFrame::copyTo(PVideoFrame &frame, VideoInfo &viInfo)
{
  copyInternalTo(frame, viInfo);
}

void PlanarFrame::copyTo(PlanarFrame &frame)
{
  copyInternalTo(frame);
}

void PlanarFrame::copyPlaneTo(PlanarFrame &frame, int plane)
{
  copyInternalPlaneTo(frame, plane);
}

unsigned char* PlanarFrame::GetPtr(int plane)
{
  if (plane == 0) return y;
  if (plane == 1) return u;
  return v;
}

int PlanarFrame::GetWidth(int plane)
{
  if (plane == 0) return ywidth;
  else return uvwidth;
}

int PlanarFrame::GetHeight(int plane)
{
  if (plane == 0) return yheight;
  else return uvheight;
}

int PlanarFrame::GetPitch(int plane)
{
  if (plane == 0) return ypitch;
  else return uvpitch;
}

void PlanarFrame::freePlanar()
{
  if (y != NULL) { _aligned_free(y); y = NULL; }
  if (u != NULL) { _aligned_free(u); u = NULL; }
  if (v != NULL) { _aligned_free(v); v = NULL; }
  ypitch = uvpitch = 0;
  ywidth = uvwidth = 0;
  yheight = uvheight = 0;
  cpu = 0;
}

void PlanarFrame::copyInternalFrom(PVideoFrame &frame, VideoInfo &viInfo)
{
  if (y == NULL || u == NULL || v == NULL) return;
  if (viInfo.IsPlanar())
  {
    BitBlt(y, ypitch, frame->GetReadPtr(PLANAR_Y), frame->GetPitch(PLANAR_Y),
      frame->GetRowSize(PLANAR_Y), frame->GetHeight(PLANAR_Y));
    BitBlt(u, uvpitch, frame->GetReadPtr(PLANAR_U), frame->GetPitch(PLANAR_U),
      frame->GetRowSize(PLANAR_U), frame->GetHeight(PLANAR_U));
    BitBlt(v, uvpitch, frame->GetReadPtr(PLANAR_V), frame->GetPitch(PLANAR_V),
      frame->GetRowSize(PLANAR_V), frame->GetHeight(PLANAR_V));
  }
  else if (viInfo.IsYUY2())
  {
    convYUY2to422(frame->GetReadPtr(), y, u, v, frame->GetPitch(), ypitch, uvpitch,
      viInfo.width, viInfo.height);
  }
}

void PlanarFrame::copyInternalFrom(PlanarFrame &frame)
{
  if (y == NULL || u == NULL || v == NULL) return;
  BitBlt(y, ypitch, frame.y, frame.ypitch, frame.ywidth, frame.yheight);
  BitBlt(u, uvpitch, frame.u, frame.uvpitch, frame.uvwidth, frame.uvheight);
  BitBlt(v, uvpitch, frame.v, frame.uvpitch, frame.uvwidth, frame.uvheight);
}

void PlanarFrame::copyInternalTo(PVideoFrame &frame, VideoInfo &viInfo)
{
  if (y == NULL || u == NULL || v == NULL) return;
  if (viInfo.IsPlanar())
  {
    BitBlt(frame->GetWritePtr(PLANAR_Y), frame->GetPitch(PLANAR_Y), y, ypitch, ywidth, yheight);
    BitBlt(frame->GetWritePtr(PLANAR_U), frame->GetPitch(PLANAR_U), u, uvpitch, uvwidth, uvheight);
    BitBlt(frame->GetWritePtr(PLANAR_V), frame->GetPitch(PLANAR_V), v, uvpitch, uvwidth, uvheight);
  }
  else if (viInfo.IsYUY2())
  {
    conv422toYUY2(y, u, v, frame->GetWritePtr(), ypitch, uvpitch, frame->GetPitch(), ywidth, yheight);
  }
}

void PlanarFrame::copyInternalTo(PlanarFrame &frame)
{
  if (y == NULL || u == NULL || v == NULL) return;
  BitBlt(frame.y, frame.ypitch, y, ypitch, ywidth, yheight);
  BitBlt(frame.u, frame.uvpitch, u, uvpitch, uvwidth, uvheight);
  BitBlt(frame.v, frame.uvpitch, v, uvpitch, uvwidth, uvheight);
}

void PlanarFrame::copyInternalPlaneTo(PlanarFrame &frame, int plane)
{
  if (plane == 0 && y != NULL)
    BitBlt(frame.y, frame.ypitch, y, ypitch, ywidth, yheight);
  else if (plane == 1 && u != NULL)
    BitBlt(frame.u, frame.uvpitch, u, uvpitch, uvwidth, uvheight);
  else if (plane == 2 && v != NULL)
    BitBlt(frame.v, frame.uvpitch, v, uvpitch, uvwidth, uvheight);
}

void PlanarFrame::copyChromaTo(PlanarFrame &dst)
{
  BitBlt(dst.u, dst.uvpitch, u, uvpitch, dst.uvwidth, dst.uvheight);
  BitBlt(dst.v, dst.uvpitch, v, uvpitch, dst.uvwidth, dst.uvheight);
}

void PlanarFrame::copyToForBMP(PVideoFrame &dst, VideoInfo &viInfo)
{
  unsigned char *dstp = dst->GetWritePtr(PLANAR_Y);
  if (viInfo.IsPlanar())
  {
    int out_pitch = (ywidth + 3) & -4;
    BitBlt(dstp, out_pitch, y, ypitch, ywidth, yheight);
    BitBlt(dstp + (out_pitch*yheight), out_pitch >> 1, v, uvpitch, uvwidth, uvheight);
    BitBlt(dstp + (out_pitch*yheight) + ((out_pitch >> 1)*uvheight), out_pitch >> 1, u, uvpitch, uvwidth, uvheight);
  }
  else
  {
    int out_pitch = (dst->GetRowSize(PLANAR_Y) + 3) & -4;
    conv422toYUY2(y, u, v, dstp, ypitch, uvpitch, out_pitch, viInfo.width, viInfo.height);
  }
}

PlanarFrame& PlanarFrame::operator=(PlanarFrame &ob2)
{
  cpu = ob2.cpu;
  ypitch = ob2.ypitch;
  yheight = ob2.yheight;
  ywidth = ob2.ywidth;
  uvpitch = ob2.uvpitch;
  uvheight = ob2.uvheight;
  uvwidth = ob2.uvwidth;
  this->copyFrom(ob2);
  return *this;
}

void PlanarFrame::convYUY2to422(const unsigned char *src, unsigned char *py, unsigned char *pu,
  unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
  if ((cpu&CPUF_SSE2) && useSIMD)
    convYUY2to422_SSE2(src, py, pu, pv, pitch1, pitch2Y, pitch2UV, width, height);
  else
  {
    width >>= 1;
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        py[x << 1] = src[x << 2];
        pu[x] = src[(x << 2) + 1];
        py[(x << 1) + 1] = src[(x << 2) + 2];
        pv[x] = src[(x << 2) + 3];
      }
      py += pitch2Y;
      pu += pitch2UV;
      pv += pitch2UV;
      src += pitch1;
    }
  }
}


void PlanarFrame::convYUY2to422_SSE2(const unsigned char *src, unsigned char *py, unsigned char *pu,
  unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
  width >>= 1; // mov ecx, width
  __m128i Ymask = _mm_set1_epi16(0x00FF);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x += 4) {
      __m128i fullsrc = _mm_load_si128(reinterpret_cast<const __m128i *>(src + x * 4)); // VYUYVYUYVYUYVYUY
      __m128i yy = _mm_and_si128(fullsrc, Ymask); // 0Y0Y0Y0Y0Y0Y0Y0Y
      __m128i uvuv = _mm_srli_epi16(fullsrc, 8); // 0V0U0V0U0V0U0V0U
      yy = _mm_packus_epi16(yy, yy); // xxxxxxxxYYYYYYYY
      uvuv = _mm_packus_epi16(uvuv, uvuv); // xxxxxxxxVUVUVUVU
      __m128i uu = _mm_and_si128(uvuv, Ymask); // xxxxxxxx0U0U0U0U
      __m128i vv = _mm_srli_epi16(uvuv, 8); // xxxxxxxx0V0V0V0V
      uu = _mm_packus_epi16(uu, uu); // xxxxxxxxxxxxUUUU
      vv = _mm_packus_epi16(vv, vv); // xxxxxxxxxxxxVVVV
      _mm_storel_epi64(reinterpret_cast<__m128i *>(py + x * 2), yy); // store y
      *(uint32_t *)(pu + x) = _mm_cvtsi128_si32(uu); // store u
      *(uint32_t *)(pv + x) = _mm_cvtsi128_si32(vv); // store v
    }
    src += pitch1;
    py += pitch2Y;
    pu += pitch2UV;
    pv += pitch2UV;
  }
}

void PlanarFrame::conv422toYUY2(unsigned char *py, unsigned char *pu, unsigned char *pv,
  unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
  if ((cpu&CPUF_SSE2) && useSIMD)
    conv422toYUY2_SSE2(py, pu, pv, dst, pitch1Y, pitch1UV, pitch2, width, height);
  else
  {
    width >>= 1;
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        dst[x << 2] = py[x << 1];
        dst[(x << 2) + 1] = pu[x];
        dst[(x << 2) + 2] = py[(x << 1) + 1];
        dst[(x << 2) + 3] = pv[x];
      }
      py += pitch1Y;
      pu += pitch1UV;
      pv += pitch1UV;
      dst += pitch2;
    }
  }
}


void PlanarFrame::conv422toYUY2_SSE2(unsigned char *py, unsigned char *pu, unsigned char *pv,
  unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
  width >>= 1; // mov ecx, width
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x += 4) {
      __m128i yy = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(py + x * 2)); // YYYYYYYY
      __m128i uu = _mm_castps_si128(_mm_load_ss(reinterpret_cast<float *>(pu + x))); // 000000000000UUUU
      __m128i vv = _mm_castps_si128(_mm_load_ss(reinterpret_cast<float *>(pv + x))); // 000000000000VVVV
      __m128i uvuv = _mm_unpacklo_epi8(uu, vv); // 00000000VUVUVUVU
      __m128i yuyv = _mm_unpacklo_epi8(yy,uvuv); // VYUYVYUYVYUYVYUY
      _mm_store_si128(reinterpret_cast<__m128i *>(dst + x * 4), yuyv);
    }
    dst += pitch2;
    py += pitch1Y;
    pu += pitch1UV;
    pv += pitch1UV;
  }
}

// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

// from Avisynth 2.55 source...
// copied so we don't need an
// IScriptEnvironment pointer 
// to call it

#include "avisynth.h"

void PlanarFrame::BitBlt(unsigned char* dstp, int dst_pitch, const unsigned char* srcp,
  int src_pitch, int row_size, int height)
{
  if (!height || !row_size) return;
  if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size))
    memcpy(dstp, srcp, src_pitch * height);
  else
  {
    for (int y = height; y > 0; --y)
    {
      memcpy(dstp, srcp, row_size);
      dstp += dst_pitch;
      srcp += src_pitch;
    }
  }
}

