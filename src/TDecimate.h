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
#include <math.h>
#include "internal.h"
#include "font.h"
#include "Cycle.h"
#include "calcCRC.h"
#include "profUtil.h"
#include "Cache.h"
#include "memset_simd.h"
#define ISP 0x00000000 // p
#define ISC 0x00000001 // c
#define ISN 0x00000002 // n
#define ISB 0x00000003 // b
#define ISU 0x00000004 // u
#define ISDB 0x00000005 // l = (deinterlaced c bottom field)
#define ISDT 0x00000006 // h = (deinterlaced c top field)
#define MTC(n) n == 0 ? 'p' : n == 1 ? 'c' : n == 2 ? 'n' : n == 3 ? 'b' : n == 4 ? 'u' : \
			   n == 5 ? 'l' : n == 6 ? 'h' : 'x'
#define TOP_FIELD 0x00000008
#define D2VFILM 0x00000020
#define MAGIC_NUMBER (0xdeadfeed)
#define MAGIC_NUMBER_2 (0xdeadbeef)
#define DROP_FRAME 0x00000001 // ovr array - bit 1
#define KEEP_FRAME 0x00000002 // ovr array - 2
#define FILM 0x00000004	// ovr array - bit 3
#define VIDEO 0x00000008 // ovr array - bit 4
#define ISMATCH 0x00000070 // ovr array - bits 5-7
#define ISD2VFILM 0x00000080 // ovr array - bit 8
#define VERSION "v1.0.3"
#define cfps(n) n == 1 ? "119.880120" : n == 2 ? "59.940060" : n == 3 ? "39.960040" : \
				n == 4 ? "29.970030" : n == 5 ? "23.976024" : "unknown"

class TDecimate : public GenericVideoFilter 
{
private:
	int nfrms, nfrmsN, nt, blockx, blocky, linearCount, maxndl;
	int yshiftS, xshiftS, xhalfS, yhalfS, mode, conCycleTP, opt;
	int cycleR, cycle, hybrid, vidDetect, conCycle, vfrDec, lastn;
	int lastFrame, lastCycle, lastGroup, lastType, retFrames;
	unsigned __int64 MAX_DIFF, sceneThreshU, sceneDivU, diff_thresh, same_thresh;
	double rate, fps, mkvfps, mkvfps2, dupThresh, vidThresh, sceneThresh;
	bool debug, display, useTFMPP, batch, tcfv1, se, cve, ecf, fullInfo;
	bool noblend, m2PA, predenoise, chroma, exPP, ssd, usehints, useclip2;
	unsigned __int64 *diff, *metricsArray, *metricsOutArray, *mode2_metrics;
	int *aLUT, *mode2_decA, *mode2_order, sdlim;
	unsigned int outputCrc;
	unsigned char *ovrArray;
	const char *ovr, *input, *output, *mkvOut, *tfmIn;
	int mode2_num, mode2_den, mode2_numCycles, mode2_cfs[10];
	Cycle prev, curr, next, nbuf;
	FILE *mkvOutF;
	PClip clip2;
	char buf[8192], outputFull[270];
	void TDecimate::rerunFromStart(int s, int np, IScriptEnvironment *env);
	void TDecimate::setBlack(PVideoFrame &dst, int np);
	void TDecimate::checkVideoMetrics(Cycle &c, double thresh);
	void TDecimate::checkVideoMatches(Cycle &p, Cycle &c);
	bool TDecimate::checkMatchDup(int mp, int mc);
	void TDecimate::copyFrame(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env, int np);
	void TDecimate::findDupStrings(Cycle &p, Cycle &c, Cycle &n, IScriptEnvironment *env);
	int TDecimate::getHint(PVideoFrame &src, int &d2vfilm);
	void TDecimate::restoreHint(PVideoFrame &dst, IScriptEnvironment *env);
	void TDecimate::blendFrames(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst,
			double amount1, double amount2, int np, IScriptEnvironment *env);
	void TDecimate::calcBlendRatios(double &amount1, double &amount2, int &frame1, int &frame2, int n,
				int bframe, int cycleI);
	void TDecimate::drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks);
	void TDecimate::drawBoxYV12(PVideoFrame &dst, int blockN, int xblocks);
	void TDecimate::drawBox(PVideoFrame &dst, int blockN, int xblocks, int np);
	int TDecimate::DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s, int start);
	int TDecimate::DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s, int start);
	int TDecimate::Draw(PVideoFrame &dst, int x1, int y1, const char *s, int np, int start=0);
	PVideoFrame TDecimate::GetFrameMode01(int n, IScriptEnvironment *env, int np);
	PVideoFrame TDecimate::GetFrameMode2(int n, IScriptEnvironment *env, int np);
	PVideoFrame TDecimate::GetFrameMode3(int n, IScriptEnvironment *env, int np);
	PVideoFrame TDecimate::GetFrameMode4(int n, IScriptEnvironment *env, int np);
	PVideoFrame TDecimate::GetFrameMode5(int n, IScriptEnvironment *env, int np);
	PVideoFrame TDecimate::GetFrameMode6(int n, IScriptEnvironment *env, int np);
	PVideoFrame TDecimate::GetFrameMode7(int n, IScriptEnvironment *env, int np);
	void TDecimate::getOvrFrame(int n, unsigned __int64 &metricU, unsigned __int64 &metricF);
	void TDecimate::getOvrCycle(Cycle &current, bool mode2);
	void TDecimate::displayOutput(IScriptEnvironment* env, PVideoFrame &dst, int n, 
			int ret, bool film, double amount1, double amount2, int f1, int f2, int np);
	void TDecimate::formatMetrics(Cycle &current);
	void TDecimate::formatDups(Cycle &current);
	void TDecimate::formatDecs(Cycle &current);
	void TDecimate::formatMatches(Cycle &current);
	void TDecimate::formatMatches(Cycle &current, Cycle &previous);
	void TDecimate::debugOutput1(int n, bool film, int blend);
	void TDecimate::debugOutput2(int n, int ret, bool film, int f1, int f2, double amount1,
		double amount2);
	void TDecimate::addMetricCycle(Cycle &j);
	bool TDecimate::checkForObviousDecFrame(Cycle &p, Cycle &c, Cycle &n);
	void TDecimate::mostSimilarDecDecision(Cycle &p, Cycle &c, Cycle &n, IScriptEnvironment *env);
	int TDecimate::checkForD2VDecFrame(Cycle &p, Cycle &c, Cycle &n);
	bool TDecimate::checkForTwoDropLongestString(Cycle &p, Cycle &c, Cycle &n);
	int TDecimate::getNonDecMode2(int n, int start, int stop);
	double TDecimate::buildDecStrategy(IScriptEnvironment *env);
	void TDecimate::mode2MarkDecFrames(int cycleF);
	void TDecimate::removeMinN(int m, int n, int start, int stop);
	void TDecimate::removeMinN(int m, int n, unsigned __int64 *metricsT, int *orderT, int &ovrC);
	int TDecimate::findDivisor(double decRatio, int min_den);
	int TDecimate::findNumerator(double decRatio, int divisor);
	double TDecimate::findCorrectionFactors(double decRatio, int num, int den, int rc[10], IScriptEnvironment *env);
	void TDecimate::sortMetrics(unsigned __int64 *metrics, int *order, int length);
	void TDecimate::SedgeSort(unsigned __int64 *metrics, int *order, int length);
	void TDecimate::pQuickerSort(unsigned __int64 *metrics, int *order, int lower, int upper);
	void TDecimate::calcMetricCycle(Cycle &current, IScriptEnvironment *env, int np,
								bool scene, bool hnt);
	unsigned __int64 TDecimate::calcMetric(PVideoFrame &prevt, PVideoFrame &currt, int np, int &blockNI, 
		int &xblocksI, unsigned __int64 &metricF, IScriptEnvironment *env, bool scene);
	void TDecimate::calcDiffSSD_Generic_MMX(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np);
	void TDecimate::calcDiffSAD_Generic_iSSE(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np);
	void TDecimate::calcDiffSAD_Generic_MMX(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np);
	void TDecimate::calcDiffSSD_32x32_MMX(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, bool use_sse2);
	void TDecimate::calcDiffSAD_32x32_iSSE(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, bool use_sse2);
	void TDecimate::calcDiffSAD_32x32_MMX(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np);
	unsigned __int64 TDecimate::calcLumaDiffYUY2SSD(const unsigned char *prvp, const unsigned char *nxtp,
		int width, int height, int prv_pitch, int nxt_pitch, IScriptEnvironment *env);
	unsigned __int64 TDecimate::calcLumaDiffYUY2SAD(const unsigned char *prvp, const unsigned char *nxtp,
		int width, int height, int prv_pitch, int nxt_pitch, IScriptEnvironment *env);
	void TDecimate::blend_MMX_8(unsigned char* dstp, const unsigned char* srcp,  
			const unsigned char* nxtp, int width, int height, int dst_pitch, 
			int src_pitch, int nxt_pitch, double w1, double w2);
	void TDecimate::blend_SSE2_16(unsigned char* dstp, const unsigned char* srcp,  
			const unsigned char* nxtp, int width, int height, int dst_pitch, 
			int src_pitch, int nxt_pitch, double w1, double w2);
	void TDecimate::calcLumaDiffYUY2SSD_MMX_8(const unsigned char *prvp, const unsigned char *nxtp, 
		int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &ssd);
	void TDecimate::calcLumaDiffYUY2SAD_ISSE_8(const unsigned char *prvp, const unsigned char *nxtp, 
		int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
	void TDecimate::calcLumaDiffYUY2SAD_MMX_8(const unsigned char *prvp, const unsigned char *nxtp, 
		int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
	void TDecimate::calcLumaDiffYUY2SSD_SSE2_16(const unsigned char *prvp, const unsigned char *nxtp, 
		int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &ssd);
	void TDecimate::calcLumaDiffYUY2SSD_MMX_16(const unsigned char *prvp, const unsigned char *nxtp, 
		int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &ssd);
	void TDecimate::calcLumaDiffYUY2SAD_SSE2_16(const unsigned char *prvp, const unsigned char *nxtp, 
		int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
	void TDecimate::calcLumaDiffYUY2SAD_ISSE_16(const unsigned char *prvp, const unsigned char *nxtp, 
		int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
	void TDecimate::calcLumaDiffYUY2SAD_MMX_16(const unsigned char *prvp, const unsigned char *nxtp, 
		int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad);
	void TDecimate::calcSSD_SSE2_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd);
	void TDecimate::calcSSD_SSE2_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd);
	void TDecimate::calcSSD_SSE2_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd);
	void TDecimate::calcSSD_MMX_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd);
	void TDecimate::calcSSD_MMX_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd);
	void TDecimate::calcSSD_MMX_4x4(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd);
	void TDecimate::calcSSD_MMX_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd);
	void TDecimate::calcSSD_MMX_8x8(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd);
	void TDecimate::calcSSD_MMX_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd);
	void TDecimate::calcSAD_SSE2_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_SSE2_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_SSE2_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_iSSE_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_iSSE_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_iSSE_4x4(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_iSSE_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_iSSE_8x8(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_iSSE_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_MMX_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_MMX_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_MMX_4x4(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_MMX_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_MMX_8x8(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcSAD_MMX_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad);
	void TDecimate::calcBlendRatios2(double &amount1, double &amount2, int &frame1, 
		int &frame2, int tf, Cycle &p, Cycle &c, Cycle &n, int remove);
	void TDecimate::blend_iSSE_5050(unsigned char* dstp, const unsigned char* srcp,  
			const unsigned char* nxtp, int width, int height, int dst_pitch, 
			int src_pitch, int nxt_pitch);
	void TDecimate::blend_SSE2_5050(unsigned char* dstp, const unsigned char* srcp,  
			const unsigned char* nxtp, int width, int height, int dst_pitch, 
			int src_pitch, int nxt_pitch);
	bool TDecimate::similar_group(int f1, int f2, IScriptEnvironment *env);
	bool TDecimate::same_group(int f1, int f2, IScriptEnvironment *env);
	bool TDecimate::diff_group(int f1, int f2, IScriptEnvironment *env);
	int TDecimate::diff_f(int f1, int f2, IScriptEnvironment *env);
	int TDecimate::mode7_analysis(int n, IScriptEnvironment *env);
	static void TDecimate::VerticalBlurMMX_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
			int dst_pitch, int width, int height);
	static void TDecimate::VerticalBlurSSE2_R(const unsigned char *srcp, unsigned char *dstp, 
			int src_pitch, int dst_pitch, int width, int height);
	static void TDecimate::HorizontalBlurMMX_YUY2_R_luma(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
			int dst_pitch, int width, int height);
	static void TDecimate::HorizontalBlurMMX_YV12_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
			int dst_pitch, int width, int height);
	static void TDecimate::VerticalBlurMMX(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
			int dst_pitch, int width, int height);
	static void TDecimate::VerticalBlurSSE2(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
			int dst_pitch, int width, int height);
	static void TDecimate::HorizontalBlurMMX_YV12(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
			int dst_pitch, int width, int height);
	static void TDecimate::HorizontalBlurMMX_YUY2_luma(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
			int dst_pitch, int width, int height);
	static void TDecimate::VerticalBlur(PVideoFrame &src, PVideoFrame &dst, int np, bool bchroma, 
			IScriptEnvironment *env, VideoInfo& vi_t, int opti);
	static void TDecimate::HorizontalBlur(PVideoFrame &src, PVideoFrame &dst, int np, bool bchroma, 
			IScriptEnvironment *env, VideoInfo& vi_t, int opti);
	static void TDecimate::HorizontalBlurMMX_YUY2(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
			int dst_pitch, int width, int height);
	static void TDecimate::HorizontalBlurMMX_YUY2_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
			int dst_pitch, int width, int height);
	bool TDecimate::wasChosen(int i, int n);
	void TDecimate::calcMetricPreBuf(int n1, int n2, int pos, int np, bool scene, bool gethint,
		IScriptEnvironment *env);
public:
	PVideoFrame __stdcall TDecimate::GetFrame(int n, IScriptEnvironment *env);
	TDecimate::TDecimate(PClip _child, int _mode, int _cycleR, int _cycle, double _rate, 
		double _dupThresh, double _vidThresh, double _sceneThresh, int _hybrid,
		int _vidDetect, int _conCycle, int _conCycleTP, const char* _ovr, 
		const char* _output, const char* _input, const char* _tfmIn, const char* _mkvOut,
		int _nt, int _blockx, int _blocky, bool _debug, bool _display, int _vfrDec, 
		bool _batch, bool _tcfv1, bool _se, bool _chroma, bool _exPP, int _maxndl, 
		bool _m2PA, bool _predenoise, bool _noblend, bool _ssd, int _usehints,
		PClip _clip2, int _sdlim, int _opt, IScriptEnvironment* env);
	TDecimate::~TDecimate();
	static void TDecimate::blurFrame(PVideoFrame &src, PVideoFrame &dst, int np, int iterations, 
		bool bchroma, IScriptEnvironment *env, VideoInfo& vi_t, int opti);
};