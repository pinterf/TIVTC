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
#include "internal.h"
#include "TFM.h"
#include "emmintrin.h"

#ifdef VERSION
#undef VERSION
#endif

#define VERSION "v1.3"

class FieldDiff : public GenericVideoFilter
{
private:
  int nt, nfrms, opt;
  bool chroma, debug, display;
  bool sse; // sum of squared errors instead of sad
  char buf[512];
  static int64_t getDiff_SAD(PVideoFrame &src, int np, bool chromaIn, int ntIn,
    int opti, IScriptEnvironment *env);
  static int64_t getDiff_SSE(PVideoFrame &src, int np, bool chromaIn, int ntIn,
    int opti, IScriptEnvironment *env);
  static void calcFieldDiff_SSE_SSE2_16(const unsigned char *src2p, int src_pitch,
    int width, int height, __m128i nt, int64_t &diff);
  static void calcFieldDiff_SSE_SSE2_8(const unsigned char *src2p, int src_pitch,
    int width, int height, __m128i nt, int64_t &diff);
  static void calcFieldDiff_SSE_SSE2_Luma_16(const unsigned char *src2p, int src_pitch,
    int width, int height, __m128i nt, int64_t &diff);
  static void calcFieldDiff_SSE_SSE2_Luma_8(const unsigned char *src2p, int src_pitch,
    int width, int height, __m128i nt, int64_t &diff);
  static void calcFieldDiff_SAD_SSE2_16(const unsigned char *src2p, int src_pitch,
    int width, int height, __m128i nt, int64_t &diff);
  static void calcFieldDiff_SAD_SSE2_8(const unsigned char *src2p, int src_pitch,
    int width, int height, __m128i nt, int64_t &diff);
  static void calcFieldDiff_SAD_SSE2_YUY2_LumaOnly_16(const unsigned char *src2p, int src_pitch,
    int width, int height, __m128i nt, int64_t &diff);
  static void calcFieldDiff_SAD_SSE2_YUY2_LumaOnly_8(const unsigned char *src2p, int src_pitch,
    int width, int height, __m128i nt, int64_t &diff);

public:
  FieldDiff(PClip _child, int _nt, bool _chroma, bool _display,
    bool _debug, bool _sse, int _opt, IScriptEnvironment *env);
  ~FieldDiff();
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) override;
  AVSValue ConditionalFieldDiff(int n, IScriptEnvironment* env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_SERIALIZED : 0;
  }
};