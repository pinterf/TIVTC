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
#include <stdlib.h>
#include <xmmintrin.h>
#include "Font.h"
#include "../common/internal.h"
#ifndef _WIN32
#include <limits.h>
#include <stdlib.h>
#include <strings.h>
#define _strnicmp strncasecmp
#define _fullpath(absolute, relative, max) realpath((relative), (absolute))
#define MAX_PATH PATH_MAX
#else
#include <windows.h>
#endif
#include <memory>
#include <vector>
#include "calcCRC.h"
#include "internal.h"
#include "profUtil.h"
#include "PlanarFrame.h"
#define TFM_INCLUDED
#ifndef TFMPP_INCLUDED
#include "TFMPP.h"
#endif

#ifdef VERSION
#undef VERSION
#endif
#define VERSION "v1.0.8"

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

// Custom deleter for _aligned_free
struct AlignedDeleter {
  void operator()(void* ptr) const {
    _aligned_free(ptr);
  }
};

class TFM : public GenericVideoFilter
{
private:
  bool has_at_least_v8;
  int cpuFlags;


  int order, field, mode; // modified in GetFrame
  int PP; // modified in GetFrame
  // Vapoursynth comment:
  // TFM must store a copy of the string obtained from propGetData, because that pointer doesn't live forever.
  std::string ovr; // override file name
  std::string input;
  std::string output;
  std::string outputC;
  bool debug, display;
  int slow;
  bool mChroma;
  int cNum;
  int cthresh;
  int MI; // modified in GetFrame
  bool chroma;
  int blockx, blocky;
  int y0, y1; // band exclusion
  std::string d2v;
  int ovrDefault;
  int flags;
  double scthresh;
  int micout, micmatching;
  std::string trimIn;
  bool usehints;
  bool metric;
  bool batch, ubsco, mmsco;
  int opt;

  int PP_origSaved, MI_origSaved;
  int order_origSaved, field_origSaved, mode_origSaved;
  int nfrms;
  int xhalf, yhalf, xshift, yshift;
  int vidCount, fieldO, mode7_field; // mode7_field modified in GetFrame, but only when mode is 7
  uint32_t outputCrc;
  unsigned long diffmaxsc;
  
  std::unique_ptr<int, AlignedDeleter> cArray; // modified in GetFrame
  std::vector<int> setArray;

  std::vector<bool> trimArray;

  double d2vpercent;
  
  std::vector<uint8_t> ovrArray;
  std::vector<uint8_t> outArray; // modified in GetFrame, but only the element corresponding to frame n, so multithreaded access is fine
  std::vector<uint8_t> d2vfilmarray;

  std::unique_ptr<uint8_t, decltype (&_aligned_free)> tbuffer; // absdiff buffer // modified in GetFrame
  int tpitchy, tpitchuv;

  std::vector<int> moutArray; // modified in GetFrame, but only the element corresponding to frame n
  std::vector<int> moutArrayE; // modified in GetFrame, but only the elements corresponding to frame n
  
  MTRACK lastMatch; // modified in GetFrame
  SCTRACK sclast;  // modified in GetFrame
  char buf[4096];
#ifdef _WIN32
  char outputFull[MAX_PATH + 1];
  char outputCFull[MAX_PATH + 1];
#else
  char outputFull[PATH_MAX + 1];
  char outputCFull[PATH_MAX + 1];
#endif
  PlanarFrame* map;
  PlanarFrame *cmask;

  template<typename pixel_t>
  void buildDiffMapPlane_Planar(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch, int bits_per_pixel);
  void buildDiffMapPlaneYUY2(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int tpitch);
  
  template<typename pixel_t>
  void buildDiffMapPlane2(const uint8_t *prvp, const uint8_t *nxtp,
    uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
    int Width, int bits_per_pixel) const;

  void fileOut(int match, int combed, bool d2vfilm, int n, int MICount, int mics[5]);

  int compareFields(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1,
    int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, const VideoInfo &vi, int n);
  template<typename pixel_t>
  int compareFields_core(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, int match1,
    int match2, int& norm1, int& norm2, int& mtn1, int& mtn2, const VideoInfo& vi, int n);

  int compareFieldsSlow(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1,
    int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, const VideoInfo &vi, int n);
  template<typename pixel_t>
  int compareFieldsSlow_core(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, int match1,
    int match2, int& norm1, int& norm2, int& mtn1, int& mtn2, const VideoInfo& vi, int n);
  template<typename pixel_t>
  int compareFieldsSlow2_core(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, int match1,
    int match2, int& norm1, int& norm2, int& mtn1, int& mtn2, const VideoInfo& vi, int n);

  void createWeaveFrame(PVideoFrame &dst, PVideoFrame &prv, PVideoFrame &src,
    PVideoFrame &nxt, int match, int &cfrm, IScriptEnvironment* env, const VideoInfo &vi) const;
  
  bool getMatchOvr(int n, int &match, int &combed, bool &d2vmatch, bool isSC);
  void getSettingOvr(int n);
  
  bool checkCombed(PVideoFrame &src, int n, const VideoInfo &vi, int match,
    int *blockN, int &xblocksi, int *mics, bool ddebug, bool chroma, int cthresh);
  bool checkCombedPlanar(const VideoInfo &vi, PVideoFrame &src, int n, int match,
    int *blockN, int &xblocksi, int *mics, bool ddebug, bool _chroma, int cthresh);
  template<typename pixel_t>
  bool checkCombedPlanar_core(PVideoFrame& src, int n, int match,
    int* blockN, int& xblocksi, int* mics, bool ddebug, int bits_per_pixel);
  bool checkCombedYUY2(PVideoFrame &src, int n, int match,
    int *blockN, int &xblocksi, int *mics, bool ddebug, bool chroma,int cthresh);
  
  void writeDisplay(PVideoFrame &dst, const VideoInfo &vi_disp, int n, int fmatch, int combed, bool over,
    int blockN, int xblocks, bool d2vmatch, int *mics, PVideoFrame &prv,
    PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);

  void putFrameProperties(PVideoFrame& dst, int match, int combed, bool d2vfilm, const int mics[5], IScriptEnvironment* env) const;

  void putHint(const VideoInfo &vi, PVideoFrame& dst, int match, int combed, bool d2vfilm);
  template<typename pixel_t>
  void putHint_core(PVideoFrame &dst, int match, int combed, bool d2vfilm);

  void parseD2V(IScriptEnvironment *env);
  int D2V_find_and_correct(std::vector<int> &array, bool &found, int &tff) const;
  void D2V_find_fix(int a1, int a2, int sync, int &f1, int &f2, int &change) const;
  bool D2V_check_illegal(int a1, int a2) const;
  int D2V_check_final(const std::vector<int> &array) const;
  int D2V_initialize_array(std::vector<int> &array, int &d2vtype, int &frames) const;
  int D2V_write_array(const std::vector<int> &array, char wfile[]) const;
  int D2V_get_output_filename(char wfile[]) const;
  int D2V_fill_d2vfilmarray(const std::vector<int> &array, int frames);
  bool d2vduplicate(int match, int combed, int n);
  bool checkD2VCase(int check) const;
  bool checkInPatternD2V(const std::vector<int> &array, int i) const;
  int fillTrimArray(int frames);

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
    int prv_pitch, int nxt_pitch, int tpitch, int width, int height) const;

  void generateOvrHelpOutput(FILE *f) const;

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