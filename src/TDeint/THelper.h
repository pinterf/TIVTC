/*
**                TDeinterlace for AviSynth 2.6 interface
**
**   TDeinterlace is a bi-directionally motion adaptive deinterlacer.
**   It also uses a couple modified forms of ela interpolation which
**   help to reduce "jaggy" edges in places where interpolation must
**   be used. TDeinterlace currently supports 8 bit planar YUV and YUY2 colorspaces.
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

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <limits.h>
#include "internal.h"
#define TDHelper_included
#ifndef TDeint_included
#include "TDeinterlace.h"
#endif
#include <vector>

class TDeinterlace;

template<typename pixel_t>
void subtractFrames_SSE2(const uint8_t* srcp1, int src1_pitch,
  const uint8_t* srcp2, int src2_pitch, int height, int width, int inc,
  uint64_t& diff);

class TDHelper : public GenericVideoFilter
{
private:
  bool has_at_least_v8;
  int cpuFlags;

  char buf[512];
  int nfrms, order, field;
  uint64_t lim;
  int debug;
  int opt;
  std::vector<int> &sa;
  int slow;
  TDeinterlace* tdptr;

  int mapn(int n);

  template<typename pixel_t>
  uint64_t subtractFrames(PVideoFrame &src1, PVideoFrame &src2, IScriptEnvironment *env);

  template<typename pixel_t>
  void blendFrames_5050(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, IScriptEnvironment *env);
public:
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) override;
  ~TDHelper();
  TDHelper(PClip _child, int _order, int _field, double _lim, bool _debug,
    int _opt, std::vector<int>& _sa, int _slow, TDeinterlace *_tdptr, IScriptEnvironment *env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_SERIALIZED : 0;
  }
};