/*
**                TDeinterlace v1.2 for Avisynth 2.6 interface
**
**   TDeinterlace is a bi-directionally motion adaptive deinterlacer.
**   It also uses a couple modified forms of ela interpolation which
**   help to reduce "jaggy" edges in places where interpolation must
**   be used. TDeinterlace currently supports YV12 and YUY2 colorspaces.
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

#include <windows.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <malloc.h>
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
*/
#define TDEINT_VERSION "v1.2"
#define TDEINT_DATE "04/04/2020"

class TDeinterlace : public GenericVideoFilter
{
  bool has_at_least_v8;

  friend class TDHelper;
  TDBuf *db;
  VideoInfo vi_saved;

/*
  mode(_mode), order(_order), field(_field), mthreshL(_mthreshL),
  mthreshC(_mthreshC), map(_map), ovr(_ovr), ovrDefault(_ovrDefault), type(_type),
  debug(_debug), mtnmode(_mtnmode), sharp(_sharp), hints(_hints), clip2(_clip2), full(_full),
  cthresh(_cthresh), chroma(_chroma), MI(_MI), tryWeave(_tryWeave), link(_link),
  denoise(_denoise), AP(_AP), blockx(_blockx), blocky(_blocky), APType(_APType),
  edeint(_edeint), emask(_emask), metric(_metric), expand(_expand), slow(_slow),
  emtn(_emtn), tshints(_tshints), opt(_opt)
*/

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

  int countOvr, nfrms, nfrms2, orderS, fieldS;
  int mthreshLS, mthreshCS, typeS, cthresh6;
  int blockx_half, blocky_half, blockx_shift, blocky_shift;
  std::vector<int> input;
  int* cArray;
  int sa_pos, rmatch;
  unsigned int passHint;
  int accumNn, accumPn, accumNm, accumPm;
  bool autoFO, useClip2;
  int tpitchy, tpitchuv;
  unsigned char *tbuffer;
  char buf[120];

  void createMotionMap4_PlanarOrYUY2(PVideoFrame &prv2, PVideoFrame &prv,
    PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask,
    int n, bool isYUY2, IScriptEnvironment *env);
  void createMotionMap5_PlanarOrYUY2(PVideoFrame &prv2, PVideoFrame &prv,
    PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask,
    int n, bool IsYUY2, IScriptEnvironment *env);
  
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
  
  void subtractFields(PVideoFrame &prv, PVideoFrame &src,
    PVideoFrame &nxt, VideoInfo &vit, int &aPn, int &aNn, int &aPm, int &aNm,
    int fieldt, int ordert, int optt, bool d2, int _slow, IScriptEnvironment *env);
  void subtractFields1(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
    VideoInfo &vit, int &aPn, int &aNn, int &aPm, int &aNm, int fieldt, int ordert,
    int optt, bool d2, IScriptEnvironment *env);
  void subtractFields2(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
    VideoInfo &vit, int &aPn, int &aNn, int &aPm, int &aNm, int fieldt, int ordert,
    int optt, bool d2, IScriptEnvironment *env);
  
  void mapColorsPlanar(PVideoFrame &dst, PVideoFrame &mask);
  void mapColorsYUY2(PVideoFrame &dst, PVideoFrame &mask);
  void mapMergePlanar(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
  void mapMergeYUY2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);

  void smartELADeintPlanar(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
  void smartELADeintYUY2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
  void cubicDeintPlanar(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
  void cubicDeintYUY2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
  void eDeintPlanar(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &efrm);
  void eDeintYUY2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &efrm);
  void kernelDeintPlanar(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
  void kernelDeintYUY2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
  void blendDeint(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);
  void blendDeint2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);

  bool checkCombedYUY2(PVideoFrame &src, int &MIC, IScriptEnvironment *env);
  bool checkCombedPlanar(PVideoFrame &src, int &MIC, IScriptEnvironment *env);

  void createWeaveFrameYUY2(PVideoFrame &dst, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);
  void createWeaveFramePlanar(PVideoFrame &dst, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);

  PVideoFrame GetFramePlanar(int n, IScriptEnvironment* env, bool &wdtd);
  PVideoFrame GetFrameYUY2(int n, IScriptEnvironment* env, bool &wdtd);
  
  void ELADeintYUY2(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
  void ELADeintPlanar(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);

  void apPostCheck(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &efrm,
    IScriptEnvironment *env);
  void copyForUpsize(PVideoFrame &dst, PVideoFrame &src, int np, IScriptEnvironment *env);
  void setMaskForUpsize(PVideoFrame &msk, int np);
  void copyFrame(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env);

  void absDiff(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, int pos, IScriptEnvironment *env);
  void buildDiffMapPlane2(const unsigned char *prvp, const unsigned char *nxtp,
    unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int optt, IScriptEnvironment *env);
  void buildABSDiffMask(const unsigned char *prvp, const unsigned char *nxtp,
    int prv_pitch, int nxt_pitch, int tpitch, int width, int height, IScriptEnvironment *env);
  void buildDiffMapPlane_Planar(const unsigned char *prvp, const unsigned char *nxtp,
    unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch, IScriptEnvironment *env);
  void buildDiffMapPlaneYUY2(const unsigned char *prvp, const unsigned char *nxtp,
    unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch, IScriptEnvironment *env);

  void InsertDiff(PVideoFrame &p1, PVideoFrame &p2, int n, int pos, IScriptEnvironment *env);
  void insertCompStats(int n, int norm1, int norm2, int mtn1, int mtn2);
  int getMatch(int norm1, int norm2, int mtn1, int mtn2);
  void expandMap_YUY2(PVideoFrame &mask);
  template<int planarType>
  void expandMap_Planar(PVideoFrame &mask);
  void stackVertical(PVideoFrame &dst2, PVideoFrame &p1, PVideoFrame &p2, IScriptEnvironment *env);
  void updateMapAP(PVideoFrame &dst, PVideoFrame &mask, IScriptEnvironment *env);
  void putHint2(PVideoFrame &dst, bool wdtd);
  PVideoFrame createMap(PVideoFrame &src, int c, IScriptEnvironment *env, int tf);

  inline unsigned char cubicInt(unsigned char p1, unsigned char p2, unsigned char p3,
    unsigned char p4)
  {
    const int temp = (19 * (p2 + p3) - 3 * (p1 + p4) + 16) >> 5;
    if (temp > 255) return 255;
    if (temp < 0) return 0;
    return (unsigned char)temp;
  }



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
  static int getHint(PVideoFrame &src, unsigned int &storeHint, int &hintField);
  static void putHint(PVideoFrame &dst, unsigned int hint, int fieldt);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return 
      cachehints == CACHE_GET_MTMODE ? MT_SERIALIZED : 
      cachehints == CACHE_DONT_CACHE_ME ? 1 : 
      0;
  }
};