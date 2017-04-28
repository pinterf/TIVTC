/*
**                TDeinterlace v1.1 for Avisynth 2.5.x
**
**   TDeinterlace is a bi-directionally motion adaptive deinterlacer.
**   It also uses a couple modified forms of ela interpolation which 
**   help to reduce "jaggy" edges in places where interpolation must 
**   be used. TDeinterlace currently supports YV12 and YUY2 colorspaces.
**   
**   Copyright (C) 2004-2007 Kevin Stone
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
#include "memset_simd.h"
#define VERSION "v1.1"
#define DATE "01/22/2007"

class TDeinterlace : public GenericVideoFilter
{
	friend class TDHelper;
	TDBuf *db;
	VideoInfo vi_saved;
	int mode, order, field, ovrDefault, type, mtnmode;
	int mthreshL, mthreshC, map, cthresh, MI, link;
	int countOvr, nfrms, nfrms2, orderS, fieldS, metric;
	int mthreshLS, mthreshCS, typeS, cthresh6, AP;
	int xhalf, yhalf, xshift, yshift, blockx, blocky;
	int *input, *cArray, APType, opt, sa_pos, rmatch;
	unsigned int passHint;
	int accumNn, accumPn, accumNm, accumPm;
	bool debug, sharp, hints, full, chroma;
	bool autoFO, useClip2, tryWeave, denoise, tshints;
	int expand, slow, tpitchy, tpitchuv;
	const char* ovr;
	unsigned char *tbuffer;
	char buf[120];
	PClip clip2, edeint, emask, emtn;
	void TDeinterlace::createMotionMap4YV12(PVideoFrame &prv2, PVideoFrame &prv, 
		PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask, 
		int n, IScriptEnvironment *env);
	void TDeinterlace::createMotionMap4YUY2(PVideoFrame &prv2, PVideoFrame &prv, 
		PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask, 
		int n, IScriptEnvironment *env);
	void TDeinterlace::createMotionMap5YV12(PVideoFrame &prv2, PVideoFrame &prv, 
		PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask, 
		int n, IScriptEnvironment *env);
	void TDeinterlace::createMotionMap5YUY2(PVideoFrame &prv2, PVideoFrame &prv, 
		PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask, 
		int n, IScriptEnvironment *env);
	void TDeinterlace::linkFULL_YV12(PVideoFrame &mask);
	void TDeinterlace::linkYtoUV_YV12(PVideoFrame &mask);
	void TDeinterlace::linkUVtoY_YV12(PVideoFrame &mask);
	void TDeinterlace::linkFULL_YUY2(PVideoFrame &mask);
	void TDeinterlace::linkYtoUV_YUY2(PVideoFrame &mask);
	void TDeinterlace::linkUVtoY_YUY2(PVideoFrame &mask);
	void TDeinterlace::denoiseYV12(PVideoFrame &mask);
	void TDeinterlace::denoiseYUY2(PVideoFrame &mask);
	void TDeinterlace::subtractFields(PVideoFrame &prv, PVideoFrame &src, 
		PVideoFrame &nxt, VideoInfo &vit, int &aPn, int &aNn, int &aPm, int &aNm,
		int fieldt, int ordert, int optt, bool d2, int _slow, IScriptEnvironment *env);
	void TDeinterlace::subtractFields1(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
		VideoInfo &vit, int &aPn, int &aNn, int &aPm, int &aNm, int fieldt, int ordert, 
		int optt, bool d2, IScriptEnvironment *env);
	void TDeinterlace::subtractFields2(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
		VideoInfo &vit, int &aPn, int &aNn, int &aPm, int &aNm, int fieldt, int ordert, 
		int optt, bool d2, IScriptEnvironment *env);
	void TDeinterlace::mapColorsYV12(PVideoFrame &dst, PVideoFrame &mask);
	void TDeinterlace::mapColorsYUY2(PVideoFrame &dst, PVideoFrame &mask);
	void TDeinterlace::mapMergeYV12(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::mapMergeYUY2(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::smartELADeintYV12(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::smartELADeintYUY2(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::cubicDeintYV12(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::cubicDeintYUY2(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::eDeintYV12(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &efrm);
	void TDeinterlace::eDeintYUY2(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &efrm);
	void TDeinterlace::kernelDeintYV12(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::kernelDeintYUY2(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::blendDeint(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, 
		IScriptEnvironment *env);
	void TDeinterlace::blendDeint2(PVideoFrame &dst, PVideoFrame &mask, 
			PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, 
			IScriptEnvironment *env);
	unsigned char TDeinterlace::cubicInt(unsigned char p1, unsigned char p2, 
		unsigned char p3, unsigned char p4);
	bool TDeinterlace::checkCombedYUY2(PVideoFrame &src, int &MIC, IScriptEnvironment *env);
	bool TDeinterlace::checkCombedYV12(PVideoFrame &src, int &MIC, IScriptEnvironment *env);
	void TDeinterlace::createWeaveFrameYUY2(PVideoFrame &dst, PVideoFrame &prv, 
		PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);
	void TDeinterlace::createWeaveFrameYV12(PVideoFrame &dst, PVideoFrame &prv, 
		PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env);
	PVideoFrame TDeinterlace::GetFrameYV12(int n, IScriptEnvironment* env, bool &wdtd);
	PVideoFrame TDeinterlace::GetFrameYUY2(int n, IScriptEnvironment* env, bool &wdtd);
	void TDeinterlace::ELADeintYUY2(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::ELADeintYV12(PVideoFrame &dst, PVideoFrame &mask, 
		PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt);
	void TDeinterlace::apPostCheck(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &efrm,
		IScriptEnvironment *env);
	void TDeinterlace::copyForUpsize(PVideoFrame &dst, PVideoFrame &src, int np, IScriptEnvironment *env);
	void TDeinterlace::setMaskForUpsize(PVideoFrame &msk, int np);
	void TDeinterlace::copyFrame(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env);
	void TDeinterlace::absDiff(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, int pos,
		IScriptEnvironment *env);
	void TDeinterlace::absDiffSSE2(const unsigned char *srcp1, const unsigned char *srcp2,
	   unsigned char *dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width, int height,
	   int mthresh1, int mthresh2);
	void TDeinterlace::absDiffMMX(const unsigned char *srcp1, const unsigned char *srcp2,
	   unsigned char *dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width, int height,
	   int mthresh1, int mthresh2);
	static void TDeinterlace::buildDiffMapPlane(const unsigned char *prvp, const unsigned char *nxtp, 
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height, 
		int Width, int optt, IScriptEnvironment *env);
	static void TDeinterlace::buildABSDiffMask2_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);
	static void TDeinterlace::buildABSDiffMask2_MMX(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width,
		int height);
	void TDeinterlace::buildABSDiffMask(const unsigned char *prvp, const unsigned char *nxtp, 
		int prv_pitch, int nxt_pitch, int tpitch, int width, int height, IScriptEnvironment *env);
	void TDeinterlace::buildABSDiffMask_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);
	void TDeinterlace::buildABSDiffMask_MMX(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height);
	void TDeinterlace::buildDiffMapPlaneYV12(const unsigned char *prvp, const unsigned char *nxtp, 
			unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height, 
			int Width, int tpitch, IScriptEnvironment *env);
	void TDeinterlace::buildDiffMapPlaneYUY2(const unsigned char *prvp, const unsigned char *nxtp, 
			unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height, 
			int Width, int tpitch, IScriptEnvironment *env);
	void TDeinterlace::InsertDiff(PVideoFrame &p1, PVideoFrame &p2, int n, int pos, IScriptEnvironment *env);
	void TDeinterlace::insertCompStats(int n, int norm1, int norm2, int mtn1, int mtn2);
	int TDeinterlace::getMatch(int norm1, int norm2, int mtn1, int mtn2);
	void TDeinterlace::compute_sum_8x8_mmx(const unsigned char *srcp, int pitch, int &sum);
	void TDeinterlace::compute_sum_8x8_isse(const unsigned char *srcp, int pitch, int &sum);
	void TDeinterlace::compute_sum_8x16_mmx_luma(const unsigned char *srcp, int pitch, int &sum);
	void TDeinterlace::compute_sum_8x16_isse_luma(const unsigned char *srcp, int pitch, int &sum);
	void TDeinterlace::compute_sum_8x16_sse2_luma(const unsigned char *srcp, int pitch, int &sum);
	void TDeinterlace::check_combing_SSE2_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int dst_pitch, __m128 thresh);
	void TDeinterlace::check_combing_MMX_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int dst_pitch, __int64 thresh);
	void TDeinterlace::check_combing_SSE2_M1(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int dst_pitch, __m128 thresh);
	void TDeinterlace::check_combing_MMX_M1(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int dst_pitch, __int64 thresh);
	void TDeinterlace::check_combing_MMX_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w);
	void TDeinterlace::check_combing_iSSE_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w);
	void TDeinterlace::check_combing_SSE2_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __m128 threshb, __m128 thresh6w);
	void TDeinterlace::check_combing_MMX(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w);
	void TDeinterlace::check_combing_iSSE(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w);
	void TDeinterlace::check_combing_SSE2(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __m128 threshb, __m128 thresh6w);
	void TDeinterlace::expandMap_YUY2(PVideoFrame &mask);
	void TDeinterlace::expandMap_YV12(PVideoFrame &mask);
	void TDeinterlace::stackVertical(PVideoFrame &dst2, PVideoFrame &p1, PVideoFrame &p2,
		IScriptEnvironment *env);
	void TDeinterlace::updateMapAP(PVideoFrame &dst, PVideoFrame &mask, IScriptEnvironment *env);
	void TDeinterlace::putHint2(PVideoFrame &dst, bool wdtd);
	PVideoFrame TDeinterlace::createMap(PVideoFrame &src, int c, IScriptEnvironment *env,
		int tf);

public:
	int *sa;
	PVideoFrame __stdcall TDeinterlace::GetFrame(int n, IScriptEnvironment* env);
	TDeinterlace::TDeinterlace(PClip _child, int _mode, int _order, int _field, int _mthreshL, 
		int _mthreshC, int _map, const char* _ovr, int _ovrDefault, int _type, bool _debug, 
		int _mtnmode, bool _sharp, bool _hints, PClip _clip2, bool _full, int _cthresh, 
		bool _chroma, int _MI, bool _tryWeave, int _link, bool _denoise, int _AP, 
		int _blockx, int _blocky, int _APType, PClip _edeint, PClip _emask, int _metric, 
		int _expand, int _slow, PClip _emtn, bool _tshints, int _opt, IScriptEnvironment* env);
	TDeinterlace::~TDeinterlace();
	static int TDeinterlace::getHint(PVideoFrame &src, unsigned int &storeHint, int &hintField);
	static void TDeinterlace::putHint(PVideoFrame &dst, unsigned int hint, int fieldt);
};