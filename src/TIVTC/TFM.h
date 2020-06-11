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
constexpr int ISP = 0x00000000; // p
constexpr int ISC = 0x00000001; // c
constexpr int ISN = 0x00000002; // n
constexpr int ISB = 0x00000003; // b
constexpr int ISU = 0x00000004; // u
constexpr int ISDB = 0x00000005; // l = (deinterlaced c bottom field)
constexpr int ISDT = 0x00000006; // h = (deinterlaced c top field)

#define MTC(n) n == 0 ? 'p' : n == 1 ? 'c' : n == 2 ? 'n' : n == 3 ? 'b' : n == 4 ? 'u' : \
			   n == 5 ? 'l' : n == 6 ? 'h' : 'x'

constexpr int TOP_FIELD = 0x00000008;
constexpr int COMBED = 0x00000010;
constexpr int D2VFILM = 0x00000020;

constexpr uint32_t MAGIC_NUMBER = 0xdeadfeed;
constexpr uint32_t MAGIC_NUMBER_2 = 0xdeadbeef;

constexpr int FILE_COMBED = 0x00000030;
constexpr int FILE_NOTCOMBED = 0x00000020;
constexpr int FILE_ENTRY = 0x00000080;
constexpr int FILE_D2V = 0x00000008;
constexpr int D2VARRAY_DUP_MASK = 0x03;
constexpr int D2VARRAY_MATCH_MASK = 0x3C;

#ifdef VERSION
#undef VERSION
#endif
#define VERSION "v1.0.5"

template<int planarType>
void FillCombedPlanarUpdateCmaskByUV(PlanarFrame* cmask);

template<typename pixel_t>
void checkCombedPlanarAnalyze_core(const VideoInfo& vi, int cthresh, bool chroma, int cpuFlags, int metric, PVideoFrame& src, PlanarFrame* cmask);

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
  const char* ovr; // override file name
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
  int y0, y1; // band exclusion
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

  int PP_origSaved, MI_origSaved;
  int order_origSaved, field_origSaved, mode_origSaved;
  int nfrms;
  int xhalf, yhalf, xshift, yshift;
  int vidCount, setArraySize, fieldO, mode7_field;
  uint32_t outputCrc;
  unsigned long diffmaxsc;
  
  int *cArray, *setArray;
  bool *trimArray;

  double d2vpercent;
  
  uint8_t* ovrArray, * outArray, * d2vfilmarray; // fixme: to vector

  uint8_t *tbuffer; // absdiff buffer
  int tpitchy, tpitchuv;

  int* moutArray;
  int* moutArrayE;
  
  MTRACK lastMatch;
  SCTRACK sclast;
  char buf[4096], outputFull[270], outputCFull[270];
  PlanarFrame* map;
  PlanarFrame *cmask;

  template<typename pixel_t>
  void buildDiffMapPlane_Planar(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch, int bits_per_pixel, IScriptEnvironment *env);
  void buildDiffMapPlaneYUY2(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch, IScriptEnvironment *env);
  
  template<typename pixel_t>
  void buildDiffMapPlane2(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int bits_per_pixel, IScriptEnvironment *env);

  void fileOut(int match, int combed, bool d2vfilm, int n, int MICount, int mics[5]);

  int compareFields(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1,
    int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, const VideoInfo &vi, int n, IScriptEnvironment *env);
  template<typename pixel_t>
  int compareFields_core(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, int match1,
    int match2, int& norm1, int& norm2, int& mtn1, int& mtn2, const VideoInfo& vi, int n, IScriptEnvironment* env);

  int compareFieldsSlow(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1,
    int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, const VideoInfo &vi, int n, IScriptEnvironment *env);
  template<typename pixel_t>
  int compareFieldsSlow_core(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, int match1,
    int match2, int& norm1, int& norm2, int& mtn1, int& mtn2, const VideoInfo& vi, int n, IScriptEnvironment* env);
  template<typename pixel_t>
  int compareFieldsSlow2_core(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, int match1,
    int match2, int& norm1, int& norm2, int& mtn1, int& mtn2, const VideoInfo& vi, int n, IScriptEnvironment* env);

  void createWeaveFrame(PVideoFrame &dst, PVideoFrame &prv, PVideoFrame &src,
    PVideoFrame &nxt, IScriptEnvironment *env, int match, int &cfrm, const VideoInfo &vi);
  
  bool getMatchOvr(int n, int &match, int &combed, bool &d2vmatch, bool isSC);
  void getSettingOvr(int n);
  
  bool checkCombed(PVideoFrame &src, int n, IScriptEnvironment *env, const VideoInfo &vi, int match,
    int *blockN, int &xblocksi, int *mics, bool ddebug, bool chroma, int cthresh);
  bool checkCombedPlanar(const VideoInfo &vi, PVideoFrame &src, int n, IScriptEnvironment *env, int match,
    int *blockN, int &xblocksi, int *mics, bool ddebug, bool chroma, int cthresh);
  template<typename pixel_t>
  bool checkCombedPlanar_core(PVideoFrame& src, int n, IScriptEnvironment* env, int match,
    int* blockN, int& xblocksi, int* mics, bool ddebug, int bits_per_pixel);
  bool checkCombedYUY2(PVideoFrame &src, int n, IScriptEnvironment *env, int match,
    int *blockN, int &xblocksi, int *mics, bool ddebug, bool chroma,int cthresh);
  
  void writeDisplay(PVideoFrame &dst, const VideoInfo &vi_disp, int n, int fmatch, int combed, bool over,
    int blockN, int xblocks, bool d2vmatch, int *mics, PVideoFrame &prv,
    PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);

  void putHint(const VideoInfo &vi, PVideoFrame& dst, int match, int combed, bool d2vfilm);
  template<typename pixel_t>
  void putHint_core(PVideoFrame &dst, int match, int combed, bool d2vfilm);

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

  bool checkSceneChange(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int n);
  template<typename pixel_t>
  bool checkSceneChange_core(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt,
    int n, int bits_per_pixel);

  void micChange(int n, int m1, int m2, PVideoFrame &dst, PVideoFrame &prv,
    PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env, const VideoInfo &vi, int &fmatch,
    int &combed, int &cfrm);
  void checkmm(int &cmatch, int m1, int m2, PVideoFrame &dst, int &dfrm, PVideoFrame &tmp, int &tfrm,
    PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env, const VideoInfo &vi, int n,
    int *blockN, int &xblocks, int *mics);

  // O.K. common parts with TDeint
  // fixme: hbd!
  template<typename pixel_t>
  void buildABSDiffMask(const uint8_t *prvp, const uint8_t *nxtp,
    int prv_pitch, int nxt_pitch, int tpitch, int width, int height, IScriptEnvironment *env);

  void generateOvrHelpOutput(FILE *f);

public:
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
  AVSValue ConditionalIsCombedTIVTC(int n, IScriptEnvironment* env);
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