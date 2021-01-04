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

#include <stdlib.h>
#include "internal.h"

#define MIN_ALIGNMENT 64

#define PLANAR_420 1
#define PLANAR_422 2
#define PLANAR_444 3
#define PLANAR_411 4
#define PLANAR_400 5

class PlanarFrame
{
private:
  int cpu;
  bool useSIMD, packed;
  int ypitch, uvpitch;
  int ywidth, uvwidth;
  int yheight, uvheight;
  uint8_t *y, *u, *v;
  bool allocSpace(VideoInfo &viInfo);
  bool allocSpace(int specs[4]);
  void copyInternalFrom(PVideoFrame &frame, VideoInfo &viInfo);
  void copyInternalFrom(PlanarFrame &frame);
  void copyInternalTo(PVideoFrame &frame, VideoInfo &viInfo);
  void copyInternalTo(PlanarFrame &frame);
  void copyInternalPlaneTo(PlanarFrame &frame, int plane);
  void convYUY2to422(const uint8_t *src, uint8_t *py, uint8_t *pu,
    uint8_t *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height);
  void convYUY2to422_SSE2(const uint8_t *src, uint8_t *py, uint8_t *pu,
    uint8_t *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height);
  void conv422toYUY2(uint8_t *py, uint8_t *pu, uint8_t *pv,
    uint8_t *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);
  void conv422toYUY2_SSE2(uint8_t *py, uint8_t *pu, uint8_t *pv,
    uint8_t *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);

public:
  PlanarFrame(int cpuInfo);
  PlanarFrame(VideoInfo &viInfo, int cpuInfo);
  PlanarFrame(VideoInfo &viInfo, bool _packed, int cpuInfo);
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
  uint8_t* GetPtr(int plane = 0);
  int NumComponents();
  int GetWidth(int plane = 0);
  int GetHeight(int plane = 0);
  int GetPitch(int plane = 0);
  void BitBlt(uint8_t* dstp, int dst_pitch, const uint8_t* srcp,
    int src_pitch, int row_size, int height);
  PlanarFrame& operator=(PlanarFrame &ob2);
};

#endif