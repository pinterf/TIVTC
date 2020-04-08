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

#ifndef __PlanarFrame_H__
#define __PlanarFrame_H__

#include <windows.h>
#include <malloc.h>
#include "internal.h"

#define MIN_ALIGNMENT 16

#define PLANAR_420 1
#define PLANAR_422 2
#define PLANAR_444 3

__declspec(align(16)) const int64_t Ymask[2] = { 0x00FF00FF00FF00FF, 0x00FF00FF00FF00FF };

class PlanarFrame
{
private:
  int cpu;
  bool useSIMD, packed;
  int ypitch, uvpitch;
  int ywidth, uvwidth;
  int yheight, uvheight;
  unsigned char *y, *u, *v;
  bool allocSpace(VideoInfo &viInfo);
  bool allocSpace(int specs[4]);
  int getCPUInfo();
  void copyInternalFrom(PVideoFrame &frame, VideoInfo &viInfo);
  void copyInternalFrom(PlanarFrame &frame);
  void copyInternalTo(PVideoFrame &frame, VideoInfo &viInfo);
  void copyInternalTo(PlanarFrame &frame);
  void copyInternalPlaneTo(PlanarFrame &frame, int plane);
#ifdef ALLOW_MMX
  static void asm_BitBlt_ISSE(unsigned char* dstp, int dst_pitch,
    const unsigned char* srcp, int src_pitch, int row_size, int height);
#endif
  void convYUY2to422(const unsigned char *src, unsigned char *py, unsigned char *pu,
    unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height);
#ifdef ALLOW_MMX
  void convYUY2to422_MMX(const unsigned char *src, unsigned char *py, unsigned char *pu,
    unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height);
#endif
  void convYUY2to422_SSE2(const unsigned char *src, unsigned char *py, unsigned char *pu,
    unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height);
  void conv422toYUY2(unsigned char *py, unsigned char *pu, unsigned char *pv,
    unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);
  void conv422toYUY2_SSE2(unsigned char *py, unsigned char *pu, unsigned char *pv,
    unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);
#ifndef _M_X64
  void conv422toYUY2_MMX(unsigned char *py, unsigned char *pu, unsigned char *pv,
    unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);
#endif

public:
  PlanarFrame();
  PlanarFrame(VideoInfo &viInfo);
  PlanarFrame(VideoInfo &viInfo, bool _packed);
  ~PlanarFrame();
  void createPlanar(int yheight, int uvheight, int ywidth, int uvwidth);
  void createPlanar(int height, int width, int chroma_format);
  void createFromProfile(VideoInfo &viInfo);
  void createFromFrame(PVideoFrame &frame, VideoInfo &viInfo);
  void createFromPlanar(PlanarFrame &frame);
  void copyFrom(PVideoFrame &frame, VideoInfo &viInfo);
  void copyTo(PVideoFrame &frame, VideoInfo &viInfo);
  void copyFrom(PlanarFrame &frame);
  void copyTo(PlanarFrame &frame);
  void copyChromaTo(PlanarFrame &dst);
  void copyToForBMP(PVideoFrame &dst, VideoInfo &viInfo);
  void copyPlaneTo(PlanarFrame &dst, int plane);
  void freePlanar();
  unsigned char* GetPtr(int plane = 0);
  int GetWidth(int plane = 0);
  int GetHeight(int plane = 0);
  int GetPitch(int plane = 0);
  void BitBlt(unsigned char* dstp, int dst_pitch, const unsigned char* srcp,
    int src_pitch, int row_size, int height);
  PlanarFrame& operator=(PlanarFrame &ob2);
};

#endif