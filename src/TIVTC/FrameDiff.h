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

#include <stdio.h>
#include <malloc.h>
#include <limits.h>
#include "internal.h"
#include "TDecimate.h"
#include "TDecimateASM.h"

#ifdef VERSION
#undef VERSION
#endif

#define VERSION "v1.6"

class FrameDiff : public GenericVideoFilter
{
private:
  int cpuFlags;

  char buf[512];
  bool predenoise, ssd, rpos;
  int nt, nfrms, blockx, blocky, mode, display;
  double thresh;
  int opt;
  bool chroma, debug, prevf, norm;
  int blocky_shift, blockx_shift, blocky_half, blockx_half;
  uint64_t *diff, MAX_DIFF, threshU;
  void calcMetric(PVideoFrame &prevt, PVideoFrame &currt, const VideoInfo &vi, IScriptEnvironment *env);
#if 0
  void fillBox(PVideoFrame &dst, int blockN, int xblocks, bool dot, const VideoInfo& vi);
  void fillBoxPlanar(PVideoFrame &dst, int blockN, int xblocks, bool dot, const VideoInfo& vi);
  void fillBoxYUY2(PVideoFrame &dst, int blockN, int xblocks, bool dot);
  void Draw(PVideoFrame &dst, int x1, int y1, const char *s, const VideoInfo& vi);
  void DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s);
  void DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s);
  void drawBox(PVideoFrame &dst, int blockN, int xblocks, const VideoInfo& vi);
  void drawBoxPlanar(PVideoFrame &dst, int blockN, int xblocks, const VideoInfo& vi);
  void drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks);
#endif
  int mapn(int n);
  bool checkOnImage(int x, int xblocks4);
  void setBlack(PVideoFrame& dst, const VideoInfo& vi);
  int getCoord(int blockN, int xblocks);

public:
  FrameDiff(PClip _child, int _mode, bool _prevf, int _nt, int _blockx, int _blocky,
    bool _chroma, double _thresh, int _display, bool _debug, bool _norm, bool _predenoise,
    bool _ssd, bool _rpos, int _opt, IScriptEnvironment *env);
  ~FrameDiff();
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) override;
  AVSValue ConditionalFrameDiff(int n, IScriptEnvironment* env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_SERIALIZED : 0;
  }
};