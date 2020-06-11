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

#include <math.h>
#define TFMPP_INCLUDED
#ifndef TFM_INCLUDED
#include "TFM.h"
#endif
#ifdef VERSION
#undef VERSION
#endif
#define VERSION "v1.0.3"

template<typename pixel_t>
void maskClip2_C(const uint8_t* srcp, const uint8_t* dntp,
  const uint8_t* maskp, uint8_t* dstp, int src_pitch, int dnt_pitch,
  int msk_pitch, int dst_pitch, int width, int height);

void maskClip2_SSE2(const uint8_t* srcp, const uint8_t* dntp,
  const uint8_t* maskp, uint8_t* dstp, int src_pitch, int dnt_pitch,
  int msk_pitch, int dst_pitch, int width, int height);

template<typename pixel_t>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif 
void maskClip2_SSE4(const uint8_t* srcp, const uint8_t* dntp,
  const uint8_t* maskp, uint8_t* dstp, int src_pitch, int dnt_pitch,
  int msk_pitch, int dst_pitch, int width, int height);

template<bool with_mask>
void blendDeintMask_SSE2(const uint8_t* srcp, uint8_t* dstp,
  const uint8_t* maskp, int src_pitch, int dst_pitch, int msk_pitch,
  int width, int height);

template<typename pixel_t, bool with_mask>
void blendDeintMask_C(const pixel_t* srcp, pixel_t* dstp,
  const uint8_t* maskp, int src_pitch, int dst_pitch, int msk_pitch,
  int width, int height);

template<bool with_mask>
void cubicDeintMask_SSE2(const uint8_t* srcp, uint8_t* dstp,
  const uint8_t* maskp, int src_pitch, int dst_pitch, int msk_pitch,
  int width, int height);

template<typename pixel_t, int bits_per_pixel, bool with_mask>
void cubicDeintMask_C(const pixel_t* srcp, pixel_t* dstp,
  const uint8_t* maskp, int src_pitch, int dst_pitch, int msk_pitch,
  int width, int height);

class TFMPP : public GenericVideoFilter
{
private:

  bool has_at_least_v8;
  int cpuFlags;

  char buf[512];
  int PP, mthresh;
  const char* ovr;
  bool display;
  PClip clip2;
  bool usehints;
  int opt;
  bool uC2; // use clip2
  int PP_origSaved;
  int mthresh_origSaved;
  int nfrms;
  int setArraySize;
  int* setArray;
  PlanarFrame *mmask;

  void buildMotionMask(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
    PlanarFrame *mask, int use, const VideoInfo& vi, IScriptEnvironment *env);
  template<typename pixel_t>
  void buildMotionMask_core(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt,
    PlanarFrame* mask, int use, const VideoInfo& vi, IScriptEnvironment* env);
  void maskClip2(PVideoFrame &src, PVideoFrame &deint, PlanarFrame *mask,

    PVideoFrame &dst, const VideoInfo& vi, IScriptEnvironment *env);

  void putHint(const VideoInfo& vi, PVideoFrame& dst, int field, unsigned int hint);
  template<typename pixel_t>
  void putHint_core(PVideoFrame &dst, int field, unsigned int hint);
  bool getHint(const VideoInfo &vi, PVideoFrame& src, int& field, bool& combed, unsigned int& hint);
  template<typename pixel_t>
  bool getHint_core(PVideoFrame& src, int& field, bool& combed, unsigned int& hint);

  void getSetOvr(int n);

  void denoiseYUY2(PlanarFrame *mask);
  void denoisePlanar(PlanarFrame *mask);

  void linkYUY2(PlanarFrame *mask);
  template<int planarType>
  void linkPlanar(PlanarFrame *mask);

  void destroyHint(const VideoInfo &vi, PVideoFrame &dst, unsigned int hint);
  template<typename pixel_t>
  void destroyHint_core(PVideoFrame& dst, unsigned int hint);

  void BlendDeint(PVideoFrame& src, PlanarFrame* mask, PVideoFrame& dst,
    bool nomask, const VideoInfo& vi, IScriptEnvironment* env);
  template<typename pixel_t>
  void BlendDeint_core(PVideoFrame& src, PlanarFrame* mask, PVideoFrame& dst,
    bool nomask, const VideoInfo& vi, IScriptEnvironment* env);

  void CubicDeint(PVideoFrame &src, PlanarFrame *mask, PVideoFrame &dst, bool nomask,
    int field, const VideoInfo &vi, IScriptEnvironment *env);
  template<typename pixel_t, int bits_per_pixel>
  void CubicDeint_core(PVideoFrame& src, PlanarFrame* mask, PVideoFrame& dst, bool nomask,
    int field, const VideoInfo& vi, IScriptEnvironment* env);

  void elaDeint(PVideoFrame &dst, PlanarFrame *mask, PVideoFrame &src, bool nomask, int field, const VideoInfo &vi);
  // not the same as in tdeinterlace.
  template<typename pixel_t, int bits_per_pixel>
  void elaDeintPlanar(PVideoFrame &dst, PlanarFrame *mask, PVideoFrame &src, bool nomask, int field, const VideoInfo &vi);
  void elaDeintYUY2(PVideoFrame &dst, PlanarFrame *mask, PVideoFrame &src, bool nomask, int field);

  void copyField(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env, const VideoInfo &vi, int field);
  void buildMotionMask1_SSE2(const uint8_t *srcp1, const uint8_t *srcp2,
    uint8_t *dstp, int s1_pitch, int s2_pitch, int dst_pitch, int width, int height, long cpu);
  void buildMotionMask2_SSE2(const uint8_t *srcp1, const uint8_t *srcp2,
    const uint8_t *srcp3, uint8_t *dstp, int s1_pitch, int s2_pitch,
    int s3_pitch, int dst_pitch, int width, int height, long cpu);

  void writeDisplay(PVideoFrame& dst, const VideoInfo& vi, int n, int field);

public:
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
  TFMPP(PClip _child, int _PP, int _mthresh, const char* _ovr, bool _display, PClip _clip2,
    bool _usehints, int _opt, IScriptEnvironment* env);
  ~TFMPP();

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_SERIALIZED : 0;
  }
};