/*
**                    TIVTC v1.0.5 for Avisynth 2.5.x
**
**   TIVTC includes a field matching filter (TFM) and a decimation
**   filter (TDecimate) which can be used together to achieve an
**   IVTC or for other uses. TIVTC currently supports YV12 and 
**   YUY2 colorspaces.
**
**   Copyright (C) 2004-2008 Kevin Stone
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
#include "memset_simd.h"
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
	int order, field, mode, cthresh, MI, y0, y1, PP, PPS, MIS;
	const char *ovr, *input, *output, *outputC, *d2v, *trimIn;
	bool debug, chroma, mChroma, display;
	int cNum, nfrms, orderS, fieldS, modeS, blockx, blocky, opt;
	int xhalf, yhalf, xshift, yshift, ovrDefault, flags, slow, metric;
	int vidCount, setArraySize, fieldO, micout, micmatching, mode7_field;
	unsigned int outputCrc;
	unsigned long diffmaxsc;
	int *cArray, *setArray;
	bool *trimArray, usehints, batch, ubsco, mmsco;
	double d2vpercent;
	unsigned char *ovrArray, *outArray, *d2vfilmarray, *tbuffer;
	int tpitchy, tpitchuv, *moutArray, *moutArrayE;
	MTRACK lastMatch;
	SCTRACK sclast;
	double scthresh;
	char buf[4096], outputFull[270], outputCFull[270];
	PlanarFrame *map, *cmask;
	void TFM::buildDiffMapPlaneYV12(const unsigned char *prvp, const unsigned char *nxtp, 
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height, 
		int Width, int tpitch, IScriptEnvironment *env);
	void TFM::buildDiffMapPlaneYUY2(const unsigned char *prvp, const unsigned char *nxtp, 
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height, 
		int Width, int tpitch, IScriptEnvironment *env);
	void TFM::buildDiffMapPlane2(const unsigned char *prvp, const unsigned char *nxtp, 
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height, 
		int Width, IScriptEnvironment *env);
	void TFM::fileOut(int match, int combed, bool d2vfilm, int n, int MICount, int mics[5]);
	void TFM::copyFrame(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env, int np);
	int TFM::compareFields(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1, 
		int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, int np, int n, IScriptEnvironment *env);
	int TFM::compareFieldsSlow(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1, 
		int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, int np, int n, IScriptEnvironment *env);
	int TFM::compareFieldsSlow2(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1, 
		int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, int np, int n, IScriptEnvironment *env);
	void TFM::createWeaveFrame(PVideoFrame &dst, PVideoFrame &prv, PVideoFrame &src, 
		PVideoFrame &nxt, IScriptEnvironment *env, int match, int &cfrm, int np);
	bool TFM::getMatchOvr(int n, int &match, int &combed, bool &d2vmatch, bool isSC);
	void TFM::getSettingOvr(int n);
	bool TFM::checkCombed(PVideoFrame &src, int n, IScriptEnvironment *env, int np, int match,
		int *blockN, int &xblocksi, int *mics, bool ddebug);
	bool TFM::checkCombedYV12(PVideoFrame &src, int n, IScriptEnvironment *env, int match,
		int *blockN, int &xblocksi, int *mics, bool ddebug);
	bool TFM::checkCombedYUY2(PVideoFrame &src, int n, IScriptEnvironment *env, int match,
		int *blockN, int &xblocksi, int *mics, bool ddebug);
	void TFM::writeDisplay(PVideoFrame &dst, int np, int n, int fmatch, int combed, bool over,
		int blockN, int xblocks, bool d2vmatch, int *mics, PVideoFrame &prv, 
		PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);
	void TFM::putHint(PVideoFrame &dst, int match, int combed, bool d2vfilm);
	void TFM::drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks);
	void TFM::drawBoxYV12(PVideoFrame &dst, int blockN, int xblocks);
	void TFM::parseD2V(IScriptEnvironment *env);
	int TFM::D2V_find_and_correct(int *array, bool &found, int &tff);
	void TFM::D2V_find_fix(int a1, int a2, int sync, int &f1, int &f2, int &change);
	bool TFM::D2V_check_illegal(int a1, int a2);
	int TFM::D2V_check_final(int *array);
	int TFM::D2V_initialize_array(int *&array, int &d2vtype, int &frames);
	int TFM::D2V_write_array(int *array, char wfile[]);
	int TFM::D2V_get_output_filename(char wfile[]);
	int TFM::D2V_fill_d2vfilmarray(int *array, int frames);
	bool TFM::d2vduplicate(int match, int combed, int n);
	bool TFM::checkD2VCase(int check);
	bool TFM::checkInPatternD2V(int *array, int i);
	int TFM::fillTrimArray(IScriptEnvironment *env, int frames);
	void TFM::checkSceneChangeYUY2_2_SSE2(const unsigned char *prvp, const unsigned char *srcp, 
		const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
		int nxt_pitch, unsigned long &diffp, unsigned long &diffn);
	void TFM::checkSceneChangeYUY2_1_SSE2(const unsigned char *prvp, const unsigned char *srcp, 
		int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp);
	void TFM::checkSceneChangeYV12_1_SSE2(const unsigned char *prvp, const unsigned char *srcp, 
		int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp);
	void TFM::checkSceneChangeYV12_2_SSE2(const unsigned char *prvp, const unsigned char *srcp, 
		const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
	int nxt_pitch, unsigned long &diffp, unsigned long &diffn);
	void TFM::checkSceneChangeYUY2_1_ISSE(const unsigned char *prvp, const unsigned char *srcp, 
		int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp);
	void TFM::checkSceneChangeYUY2_2_ISSE(const unsigned char *prvp, const unsigned char *srcp, 
		const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
		int nxt_pitch, unsigned long &diffp, unsigned long &diffn);
	void TFM::checkSceneChangeYV12_1_ISSE(const unsigned char *prvp, const unsigned char *srcp, 
		int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp);
	void TFM::checkSceneChangeYV12_2_ISSE(const unsigned char *prvp, const unsigned char *srcp, 
		const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
		int nxt_pitch, unsigned long &diffp, unsigned long &diffn);
	void TFM::checkSceneChangeYUY2_1_MMX(const unsigned char *prvp, const unsigned char *srcp, 
		int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp);
	void TFM::checkSceneChangeYUY2_2_MMX(const unsigned char *prvp, const unsigned char *srcp, 
		const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
		int nxt_pitch, unsigned long &diffp, unsigned long &diffn);
	void TFM::checkSceneChangeYV12_1_MMX(const unsigned char *prvp, const unsigned char *srcp, 
		int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp);
	void TFM::checkSceneChangeYV12_2_MMX(const unsigned char *prvp, const unsigned char *srcp, 
		const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
		int nxt_pitch, unsigned long &diffp, unsigned long &diffn);
	bool TFM::checkSceneChange(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, 
			IScriptEnvironment *env, int n);
	void TFM::check_combing_MMX(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, 
		__int64 thresh6w);
	void TFM::check_combing_MMX_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, 
		__int64 thresh6w);
	void TFM::check_combing_iSSE(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, 
		__int64 thresh6w);
	void TFM::check_combing_iSSE_Luma(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, 
		__int64 thresh6w);
	void TFM::check_combing_SSE2(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int src_pitch2, int dst_pitch, __m128 threshb, 
		__m128 thresh6w);
	void TFM::check_combing_SSE2_Luma(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int src_pitch2, int dst_pitch, __m128 threshb, 
		__m128 thresh6w);
	void TFM::check_combing_MMX_M1(const unsigned char *srcp, unsigned char *dstp, 
			int width, int height, int src_pitch, int dst_pitch, __int64 thresh);
	void TFM::check_combing_SSE2_M1(const unsigned char *srcp, unsigned char *dstp, 
			int width, int height, int src_pitch, int dst_pitch, __m128 thresh);
	void TFM::check_combing_MMX_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
			int width, int height, int src_pitch, int dst_pitch, __int64 thresh);
	void TFM::check_combing_SSE2_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
			int width, int height, int src_pitch, int dst_pitch, __m128 thresh);
	void TFM::micChange(int n, int m1, int m2, PVideoFrame &dst, PVideoFrame &prv,
		PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env, int np, int &fmatch, 
		int &combed, int &cfrm);
	void TFM::checkmm(int &cmatch, int m1, int m2, PVideoFrame &dst, int &dfrm, PVideoFrame &tmp, int &tfrm, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env, int np, int n,
		int *blockN, int &xblocks, int *mics);
	void TFM::buildABSDiffMask(const unsigned char *prvp, const unsigned char *nxtp, 
		int prv_pitch, int nxt_pitch, int tpitch, int width, int height, IScriptEnvironment *env);
	void TFM::buildABSDiffMask_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);
	void TFM::buildABSDiffMask_MMX(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);
	void TFM::buildABSDiffMask2_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);
	void TFM::buildABSDiffMask2_MMX(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);
	void TFM::compute_sum_8x8_mmx(const unsigned char *srcp, int pitch, int &sum);
	void TFM::compute_sum_8x8_isse(const unsigned char *srcp, int pitch, int &sum);
	void TFM::compute_sum_8x16_mmx_luma(const unsigned char *srcp, int pitch, int &sum);
	void TFM::compute_sum_8x16_isse_luma(const unsigned char *srcp, int pitch, int &sum);
	void TFM::compute_sum_8x16_sse2_luma(const unsigned char *srcp, int pitch, int &sum);
	void TFM::generateOvrHelpOutput(FILE *f);

public:
	PVideoFrame __stdcall TFM::GetFrame(int n, IScriptEnvironment* env);
	AVSValue TFM::ConditionalIsCombedTIVTC(int n, IScriptEnvironment* env);
	static void TFM::DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s);
	static void TFM::DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s);
	TFM::TFM(PClip _child, int _order, int _field, int _mode, int _PP, const char* _ovr, const char* _input, 
		const char* _output, const char * _outputC, bool _debug, bool _display, int _slow, 
		bool _mChroma, int _cNum, int _cthresh, int _MI, bool _chroma, int _blockx, int _blocky, 
		int _y0, int _y1, const char* _d2v, int _ovrDefault, int _flags, double _scthresh, int _micout,
		int _micmatching, const char* _trimIn, bool _usehints, int _metric, bool _batch, bool _ubsco,
		bool _mmsco, int _opt, IScriptEnvironment* env);
	TFM::~TFM();
};