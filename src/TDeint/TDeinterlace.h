/*
**                TDeinterlace for AviSynth 2.6 interface
**
**   TDeinterlace is a bi-directionally motion adaptive deinterlacer.
**   It also uses a couple modified forms of ela interpolation which
**   help to reduce "jaggy" edges in places where interpolation must
**   be used. TDeinterlace supports 8-16 bit planar YUV and YUY2 colorspaces.
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

#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>
#include "internal.h"
#define TDeint_included
#ifndef TDHelper_included
#include "THelper.h"
#endif
#include "TDBuf.h"
#include "vector"

/*
#define TDEINT_VERSION "v1.1"
#define TDEINT_DATE "01/22/2007"
#define TDEINT_VERSION "v1.2"
#define TDEINT_DATE "04/04/2020"
#define TDEINT_VERSION "v1.3"
#define TDEINT_DATE "05/08/2020"
#define TDEINT_VERSION "v1.4"
#define TDEINT_DATE "05/12/2020"
*/
#define TDEINT_VERSION "v1.5"
#define TDEINT_DATE "05/13/2020"

void dispatch_smartELADeintPlanar(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo& vi);
template<typename pixel_t, int bits_per_pixel>
void smartELADeintPlanar(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt);
void smartELADeintYUY2(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt);

class TDeinterlace : public GenericVideoFilter
{
  bool has_at_least_v8;
  int cpuFlags;

  friend class TDHelper;
  TDBuf *db;
  VideoInfo vi_saved;
  VideoInfo vi_mask; // always 8 bit

  int mode;
  int order;
  int field;
  int mthreshL, mthreshC;
  int map;
  const char* ovr;
  int ovrDefault;
  int type;
  bool debug;
  int mtnmode;
  bool sharp;
  bool hints;
  PClip clip2;
  bool full;
  int cthresh;
  bool chroma;
  int MI;
  bool tryWeave;
  int link;
  bool denoise;
  int AP;
  int blockx, blocky;
  int APType;
  PClip edeint;
  PClip emask;
  int metric;
  int expand;
  int slow;
  PClip emtn;
  bool tshints;
  int opt;

  int countOvr, nfrms, nfrms2, order_origSaved, field_origSaved;
  int mthreshL_origSaved, mthreshC_origSaved, type_origSaved, cthresh6;
  int blockx_half, blocky_half, blockx_shift, blocky_shift;
  std::vector<int> input;
  int* cArray;
  int sa_pos, rmatch;
  unsigned int passHint;
  int accumNn, accumPn, accumNm, accumPm;
  bool autoFO, useClip2;
  int tpitchy, tpitchuv;
  uint8_t *tbuffer;
  char buf[120];

  void createMotionMap4_PlanarOrYUY2(PVideoFrame &prv2, PVideoFrame &prv,
    PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask,
    int n, bool isYUY2);
  void createMotionMap5_PlanarOrYUY2(PVideoFrame &prv2, PVideoFrame &prv,
    PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask,
    int n, bool IsYUY2);
  
  template<int planarType>
  void linkFULL_Planar(PVideoFrame &mask);
  template<int planarType>
  void linkYtoUV_Planar(PVideoFrame &mask);
  template<int planarType>
  void linkUVtoY_Planar(PVideoFrame &mask);

  void linkFULL_YUY2(PVideoFrame &mask);
  void linkYtoUV_YUY2(PVideoFrame &mask);
  void linkUVtoY_YUY2(PVideoFrame &mask);
  
  void denoisePlanar(PVideoFrame &mask);
  void denoiseYUY2(PVideoFrame &mask);

  template<typename pixel_t>
  void subtractFields(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
    VideoInfo &vit, 
    int &aPn, int &aNn, int &aPm, int &aNm,
    int fieldt, int ordert, bool d2, int _slow, IScriptEnvironment *env);

  template<typename pixel_t>
  void subtractFields1(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
    VideoInfo &vit, 
    int &aPn, int &aNn, int &aPm, int &aNm, 
    int fieldt, int ordert,
    bool d2, int bits_per_pixel, IScriptEnvironment *env);

  template<typename pixel_t>
  void subtractFields2(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
    VideoInfo &vit, 
    int &aPn, int &aNn, int &aPm, int &aNm, 
    int fieldt, int ordert,
    bool d2, int bits_per_pixel, IScriptEnvironment *env);
  
  template<typename pixel_t, int bits_per_pixel>
  void mapColorsPlanar(PVideoFrame &dst, PVideoFrame &mask);
  void dispatch_mapColorsPlanar(PVideoFrame& dst, PVideoFrame& mask, const VideoInfo& vi);
  void mapColorsYUY2(PVideoFrame &dst, PVideoFrame &mask);

  template<typename pixel_t>
  void mapMergePlanar(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int bits_per_pixel);
  void dispatch_mapMergePlanar(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo &vi);
  void mapMergeYUY2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);

  template<typename pixel_t, int bits_per_pixel>
  void cubicDeintPlanar(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt);
  void dispatch_cubicDeintPlanar(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, const VideoInfo &vi);
  void cubicDeintYUY2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);

  template<typename pixel_t>
  void eDeintPlanar(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &efrm);
  void dispatch_eDeintPlanar(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, PVideoFrame& efrm, const VideoInfo& vi);
  void eDeintYUY2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &efrm);

  template<typename pixel_t>
  void blendDeint(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);
  void dispatch_blendDeint(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo& vi, IScriptEnvironment* env);

  template<typename pixel_t>
  void blendDeint2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);
  void dispatch_blendDeint2(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo& vi, IScriptEnvironment* env);

  template<typename pixel_t, int bits_per_pixel>
  void kernelDeintPlanar(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt);
  void dispatch_kernelDeintPlanar(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo& vi);
  void kernelDeintYUY2(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt);

  // not the same as in tfmpp
  template<typename pixel_t, int bits_per_pixel>
  void ELADeintPlanar(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt);
  void dispatch_ELADeintPlanar(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo& vi);
  void ELADeintYUY2(PVideoFrame& dst, PVideoFrame& mask, PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt);

  template<typename pixel_t>
  bool checkCombedPlanar(PVideoFrame& src, int& MIC, int bits_per_pixel, bool chroma, int cthresh, IScriptEnvironment* env);
  bool dispatch_checkCombedPlanar(PVideoFrame& src, int& MIC, const VideoInfo& vi, bool chroma, int cthresh, IScriptEnvironment* env);
  bool checkCombedYUY2(PVideoFrame &src, int &MIC, bool chroma, int cthresh, IScriptEnvironment *env);
  
  void createWeaveFrame(PVideoFrame &dst, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);

  PVideoFrame GetFramePlanar(int n, IScriptEnvironment* env, bool &wdtd);
  PVideoFrame GetFrameYUY2(int n, IScriptEnvironment* env, bool &wdtd);
  
  template<typename pixel_t>
  void apPostCheck(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &efrm, IScriptEnvironment *env);
  
  void copyForUpsize(PVideoFrame &dst, PVideoFrame &src, const VideoInfo& vi, IScriptEnvironment *env);
  void setMaskForUpsize(PVideoFrame &msk, const VideoInfo& vi_mask);

  // hbd dispatch inside
  void absDiff(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, int pos);

  template<typename pixel_t>
  void buildABSDiffMask(const uint8_t *prvp, const uint8_t *nxtp,
    int prv_pitch, int nxt_pitch, int tpitch, int width, int height);

  template<typename pixel_t>
  void buildDiffMapPlane_Planar(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch, int bits_per_pixel);

  void buildDiffMapPlaneYUY2(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch);

  template<typename pixel_t>
  void buildDiffMapPlane2(const uint8_t* prvp, const uint8_t* nxtp,
    uint8_t* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int bits_per_pixel);

  void InsertDiff(PVideoFrame &p1, PVideoFrame &p2, int n, int pos);
  void insertCompStats(int n, int norm1, int norm2, int mtn1, int mtn2);
  int getMatch(int norm1, int norm2, int mtn1, int mtn2);

  template<int planarType>
  void expandMap_Planar(PVideoFrame &mask);
  void expandMap_YUY2(PVideoFrame& mask);
  void stackVertical(PVideoFrame &dst2, PVideoFrame &p1, PVideoFrame &p2, IScriptEnvironment *env);

  template<typename pixel_t>
  void updateMapAP(PVideoFrame &dst, PVideoFrame &mask, IScriptEnvironment *env);

  PVideoFrame createMap(PVideoFrame &src, int c, IScriptEnvironment *env, int tf);

  void putHint2(const VideoInfo& vi, PVideoFrame& dst, bool wdtd);
  template<typename pixel_t>
  void putHint2_core(PVideoFrame& dst, bool wdtd);

public:
  std::vector<int> sa;
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
  TDeinterlace(PClip _child, int _mode, int _order, int _field, int _mthreshL,
    int _mthreshC, int _map, const char* _ovr, int _ovrDefault, int _type, bool _debug,
    int _mtnmode, bool _sharp, bool _hints, PClip _clip2, bool _full, int _cthresh,
    bool _chroma, int _MI, bool _tryWeave, int _link, bool _denoise, int _AP,
    int _blockx, int _blocky, int _APType, PClip _edeint, PClip _emask, int _metric,
    int _expand, int _slow, PClip _emtn, bool _tshints, int _opt, IScriptEnvironment* env);
  ~TDeinterlace();

  static int getHint(const VideoInfo &vi, PVideoFrame& src, unsigned int& storeHint, int& hintField);
  template<typename pixel_t>
  static int getHint_core(PVideoFrame& src, unsigned int& storeHint, int& hintField);
  static void putHint(const VideoInfo& vi, PVideoFrame& dst, unsigned int hint, int fieldt);
  template<typename pixel_t>
  static void putHint_core(PVideoFrame &dst, unsigned int hint, int fieldt);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return 
      cachehints == CACHE_GET_MTMODE ? MT_SERIALIZED : 
      cachehints == CACHE_DONT_CACHE_ME ? 1 : 
      0;
  }
};