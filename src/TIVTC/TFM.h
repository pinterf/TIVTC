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

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xmmintrin.h>
#include "Font.h"
#include "calcCRC.h"
#include "internal.h"
#include "profUtil.h"
#include "PlanarFrame.h"
#define TFM_INCLUDED
#ifndef TFMPP_INCLUDED
#include "TFMPP.h"
#endif
#define ISP 0x00000000 // p
#define ISC 0x00000001 // c
#define ISN 0x00000002 // n
#define ISB 0x00000003 // b
#define ISU 0x00000004 // u
#define ISDB 0x00000005 // l
#define ISDT 0x00000006 // h
#define MTC(n) n == 0 ? 'p' : n == 1 ? 'c' : n == 2 ? 'n' : n == 3 ? 'b' : n == 4 ? 'u' : \
			   n == 5 ? 'l' : n == 6 ? 'h' : 'x'
#define TOP_FIELD 0x00000008
#define COMBED 0x00000010
#define D2VFILM 0x00000020
#define MAGIC_NUMBER (0xdeadfeed)
#define MAGIC_NUMBER_2 (0xdeadbeef)
#define FILE_COMBED 0x00000030
#define FILE_NOTCOMBED 0x00000020
#define FILE_ENTRY 0x00000080
#define FILE_D2V 0x00000008
#define D2VARRAY_DUP_MASK 0x03
#define D2VARRAY_MATCH_MASK 0x3C

#ifdef VERSION
#undef VERSION
#endif
#define VERSION "v1.0.4"

struct MTRACK {
  int frame, match;
  int field, combed;
};

struct SCTRACK {
  int frame;
  unsigned long diff;
  bool sc;
};

class TFM : public GenericVideoFilter
{
private:
  bool has_at_least_v8;
  int cpuFlags;

  int order, field, mode;
  int PP;
  const char* ovr;
  const char* input;
  const char* output;
  const char* outputC;
  bool debug, display;
  int slow;
  bool mChroma;
  int cNum;
  int cthresh;
  int MI;
  bool chroma;
  int blockx, blocky;
  int y0, y1;
  const char* d2v;
  int ovrDefault;
  int flags;
  double scthresh;
  int micout, micmatching;
  const char* trimIn;
  bool usehints;
  bool metric;
  bool batch, ubsco, mmsco;
  int opt;

  int PPS, MIS;
  int nfrms, orderS, fieldS, modeS;
  int xhalf, yhalf, xshift, yshift;
  int vidCount, setArraySize, fieldO, mode7_field;
  unsigned int outputCrc;
  unsigned long diffmaxsc;
  int *cArray, *setArray;
  bool *trimArray;
  double d2vpercent;
  uint8_t* ovrArray, * outArray, * d2vfilmarray;
  uint8_t *tbuffer; // absdiff buffer
  int tpitchy, tpitchuv, *moutArray, *moutArrayE;
  MTRACK lastMatch;
  SCTRACK sclast;
  char buf[4096], outputFull[270], outputCFull[270];
  PlanarFrame *map, *cmask;
  void buildDiffMapPlane_Planar(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch, IScriptEnvironment *env);
  void buildDiffMapPlaneYUY2(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch, IScriptEnvironment *env);
  void buildDiffMapPlane2(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, IScriptEnvironment *env);
  void fileOut(int match, int combed, bool d2vfilm, int n, int MICount, int mics[5]);
  void copyFrame(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env, int np);
  int compareFields(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1,
    int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, int np, int n, IScriptEnvironment *env);
  int compareFieldsSlow(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1,
    int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, int np, int n, IScriptEnvironment *env);
  int compareFieldsSlow2(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1,
    int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, int np, int n, IScriptEnvironment *env);
  void createWeaveFrame(PVideoFrame &dst, PVideoFrame &prv, PVideoFrame &src,
    PVideoFrame &nxt, IScriptEnvironment *env, int match, int &cfrm, int np);
  bool getMatchOvr(int n, int &match, int &combed, bool &d2vmatch, bool isSC);
  void getSettingOvr(int n);
  bool checkCombed(PVideoFrame &src, int n, IScriptEnvironment *env, int np, int match,
    int *blockN, int &xblocksi, int *mics, bool ddebug);
  bool checkCombedPlanar(PVideoFrame &src, int n, IScriptEnvironment *env, int match,
    int *blockN, int &xblocksi, int *mics, bool ddebug);
  bool checkCombedYUY2(PVideoFrame &src, int n, IScriptEnvironment *env, int match,
    int *blockN, int &xblocksi, int *mics, bool ddebug);
  void writeDisplay(PVideoFrame &dst, int np, int n, int fmatch, int combed, bool over,
    int blockN, int xblocks, bool d2vmatch, int *mics, PVideoFrame &prv,
    PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);
  void putHint(PVideoFrame &dst, int match, int combed, bool d2vfilm);
  void drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks);
  void drawBoxYV12(PVideoFrame &dst, int blockN, int xblocks);
  void parseD2V(IScriptEnvironment *env);
  int D2V_find_and_correct(int *array, bool &found, int &tff);
  void D2V_find_fix(int a1, int a2, int sync, int &f1, int &f2, int &change);
  bool D2V_check_illegal(int a1, int a2);
  int D2V_check_final(int *array);
  int D2V_initialize_array(int *&array, int &d2vtype, int &frames);
  int D2V_write_array(int *array, char wfile[]);
  int D2V_get_output_filename(char wfile[]);
  int D2V_fill_d2vfilmarray(int *array, int frames);
  bool d2vduplicate(int match, int combed, int n);
  bool checkD2VCase(int check);
  bool checkInPatternD2V(int *array, int i);
  int fillTrimArray(IScriptEnvironment *env, int frames);
  bool checkSceneChange(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
    IScriptEnvironment *env, int n);
  void micChange(int n, int m1, int m2, PVideoFrame &dst, PVideoFrame &prv,
    PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env, int np, int &fmatch,
    int &combed, int &cfrm);
  void checkmm(int &cmatch, int m1, int m2, PVideoFrame &dst, int &dfrm, PVideoFrame &tmp, int &tfrm,
    PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env, int np, int n,
    int *blockN, int &xblocks, int *mics);

  // O.K. common parts with TDeint
  void buildABSDiffMask(const uint8_t *prvp, const uint8_t *nxtp,
    int prv_pitch, int nxt_pitch, int tpitch, int width, int height, IScriptEnvironment *env);

  void generateOvrHelpOutput(FILE *f);

public:
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
  AVSValue ConditionalIsCombedTIVTC(int n, IScriptEnvironment* env);
  static void DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s);
  static void DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s);
  TFM(PClip _child, int _order, int _field, int _mode, int _PP, const char* _ovr, const char* _input,
    const char* _output, const char * _outputC, bool _debug, bool _display, int _slow,
    bool _mChroma, int _cNum, int _cthresh, int _MI, bool _chroma, int _blockx, int _blocky,
    int _y0, int _y1, const char* _d2v, int _ovrDefault, int _flags, double _scthresh, int _micout,
    int _micmatching, const char* _trimIn, bool _usehints, int _metric, bool _batch, bool _ubsco,
    bool _mmsco, int _opt, IScriptEnvironment* env);
  ~TFM();

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_SERIALIZED : 0;
  }
};