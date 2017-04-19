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
#include "internal.h"
#include "TFM.h"

#ifdef VERSION
#undef VERSION
#endif

#define VERSION "v1.3"

class FieldDiff : public GenericVideoFilter
{
private:
	int nt, nfrms, opt;
	bool chroma, debug, display, sse;
	char buf[512];
	static __int64 FieldDiff::getDiff(PVideoFrame &src, int np, bool chromaIn, int ntIn, 
		int opti, IScriptEnvironment *env);
	static __int64 FieldDiff::getDiff_SSE(PVideoFrame &src, int np, bool chromaIn, int ntIn,
		int opti, IScriptEnvironment *env);
	static void FieldDiff::calcFieldDiff_SSE_SSE2(const unsigned char *src2p, int src_pitch,
		int width, int height, __m128 nt, __int64 &diff);
	static void FieldDiff::calcFieldDiff_SSE_MMX(const unsigned char *src2p, int src_pitch,
		int width, int height, __int64 nt, __int64 &diff);
	static void FieldDiff::calcFieldDiff_SSE_SSE2_Luma(const unsigned char *src2p, int src_pitch,
		int width, int height, __m128 nt, __int64 &diff);
	static void FieldDiff::calcFieldDiff_SSE_MMX_Luma(const unsigned char *src2p, int src_pitch,
		int width, int height, __int64 nt, __int64 &diff);
	static void FieldDiff::calcFieldDiff_SAD_SSE2(const unsigned char *src2p, int src_pitch,
		int width, int height, __m128 nt, __int64 &diff);
	static void FieldDiff::calcFieldDiff_SAD_MMX(const unsigned char *src2p, int src_pitch,
		int width, int height, __int64 nt, __int64 &diff);
	static void FieldDiff::calcFieldDiff_SAD_SSE2_Luma(const unsigned char *src2p, int src_pitch,
		int width, int height, __m128 nt, __int64 &diff);
	static void FieldDiff::calcFieldDiff_SAD_MMX_Luma(const unsigned char *src2p, int src_pitch,
		int width, int height, __int64 nt, __int64 &diff);

public:
	FieldDiff::FieldDiff(PClip _child, int _nt, bool _chroma, bool _display,
		bool _debug, bool _sse, int _opt, IScriptEnvironment *env);
	FieldDiff::~FieldDiff();
	PVideoFrame __stdcall FieldDiff::GetFrame(int n, IScriptEnvironment *env);
	AVSValue FieldDiff::ConditionalFieldDiff(int n, IScriptEnvironment* env);
};