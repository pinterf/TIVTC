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

#include <windows.h>
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
  char buf[512];
  bool predenoise, ssd, rpos;
  int nt, nfrms, blockx, blocky, mode, display;
  double thresh;
  int opt;
  bool chroma, debug, prevf, norm;
  int blocky_shift, blockx_shift, blocky_half, blockx_half;
  uint64_t *diff, MAX_DIFF, threshU;
  void calcMetric(PVideoFrame &prevt, PVideoFrame &currt, int np, IScriptEnvironment *env);
  void fillBox(PVideoFrame &dst, int blockN, int xblocks, bool dot);
  void fillBoxPlanar(PVideoFrame &dst, int blockN, int xblocks, bool dot);
  void fillBoxYUY2(PVideoFrame &dst, int blockN, int xblocks, bool dot);
  void Draw(PVideoFrame &dst, int x1, int y1, const char *s, int np);
  void DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s);
  void DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s);
  void drawBox(PVideoFrame &dst, int blockN, int xblocks, int np);
  void drawBoxYV12(PVideoFrame &dst, int blockN, int xblocks);
  void drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks);
  int mapn(int n);
  bool checkOnImage(int x, int xblocks4);
  void setBlack(PVideoFrame &d);
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