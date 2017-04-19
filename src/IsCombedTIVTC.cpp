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

#include "TFM.h"

AVSValue TFM::ConditionalIsCombedTIVTC(int n, IScriptEnvironment* env) 
{
	if (n < 0) n = 0;
	else if (n > nfrms) n = nfrms;
	int np = vi.IsYV12() ? 3 : 1;
	int xblocks;
	int mics[5] = { -20, -20, -20, -20, -20 };
	int blockN[5] = { -20, -20, -20, -20, -20 };
	return checkCombed(child->GetFrame(n, env), n, env, np, 1, blockN, xblocks, mics, false);
}

AVSValue __cdecl Create_IsCombedTIVTC(AVSValue args, void* user_data, IScriptEnvironment* env) 
{
	AVSValue cnt = env->GetVar("current_frame");
	if (!cnt.IsInt())
		env->ThrowError("IsCombedTIVTC:  This filter can only be used within ConditionalFilter!");
	int n = cnt.AsInt();
	TFM *f = new TFM(args[0].AsClip(),-1,-1,1,5,"","","","",false,false,false,false,
		15,args[1].AsInt(9),args[2].AsInt(80),args[3].AsBool(false),args[4].AsInt(16),
		args[5].AsInt(16),0,0,"",0,0,12.0,0,0,"",false,args[6].AsInt(0),false,false,false,
		args[7].AsInt(4),env);
	AVSValue IsCombedTIVTC = f->ConditionalIsCombedTIVTC(n, env);	
	delete f;
	return IsCombedTIVTC;
}

#ifdef VERSION
#undef VERSION
#endif

#define VERSION "v1.2"

class ShowCombedTIVTC : public GenericVideoFilter
{
private:
	char buf[512];
	bool debug, chroma, fill;
	int cthresh, MI, blockx, blocky, display, opt;
	int yhalf, xhalf, yshift, xshift, nfrms, *cArray, metric;
	PlanarFrame *cmask;
	void ShowCombedTIVTC::fillCombedYUY2(PVideoFrame &src, int &MICount,
		int &b_over, int &c_over, IScriptEnvironment *env);
	void ShowCombedTIVTC::fillCombedYV12(PVideoFrame &src, int &MICount,
		int &b_over, int &c_over, IScriptEnvironment *env);
	void ShowCombedTIVTC::fillBox(PVideoFrame &dst, int blockN, int xblocks);
	void ShowCombedTIVTC::drawBox(PVideoFrame &dst, int blockN, int xblocks, int np);
	void ShowCombedTIVTC::Draw(PVideoFrame &dst, int x1, int y1, const char *s, int np);
	void ShowCombedTIVTC::fillBoxYUY2(PVideoFrame &dst, int blockN, int xblocks);
	void ShowCombedTIVTC::drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks);
	void ShowCombedTIVTC::DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s);
	void ShowCombedTIVTC::fillBoxYV12(PVideoFrame &dst, int blockN, int xblocks);
	void ShowCombedTIVTC::drawBoxYV12(PVideoFrame &dst, int blockN, int xblocks);
	void ShowCombedTIVTC::DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s);
	void ShowCombedTIVTC::check_combing_SSE2(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __m128 threshb, __m128 thresh6w);
	void ShowCombedTIVTC::check_combing_iSSE(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w);
	void ShowCombedTIVTC::check_combing_MMX(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w);
	void ShowCombedTIVTC::check_combing_SSE2_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __m128 threshb, __m128 thresh6w);
	void ShowCombedTIVTC::check_combing_iSSE_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w);
	void ShowCombedTIVTC::check_combing_MMX_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w);
	void ShowCombedTIVTC::check_combing_MMX_M1(const unsigned char *srcp, unsigned char *dstp, 
			int width, int height, int src_pitch, int dst_pitch, __int64 thresh);
	void ShowCombedTIVTC::check_combing_SSE2_M1(const unsigned char *srcp, unsigned char *dstp, 
			int width, int height, int src_pitch, int dst_pitch, __m128 thresh);
	void ShowCombedTIVTC::check_combing_MMX_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
			int width, int height, int src_pitch, int dst_pitch, __int64 thresh);
	void ShowCombedTIVTC::check_combing_SSE2_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
			int width, int height, int src_pitch, int dst_pitch, __m128 thresh);

public:
	PVideoFrame __stdcall ShowCombedTIVTC::GetFrame(int n, IScriptEnvironment *env);
	ShowCombedTIVTC::ShowCombedTIVTC(PClip _child, int _cthresh, bool _chroma, int _MI,
		int _blockx, int _blocky, int _metric, bool _debug, int _display, bool _fill, 
		int _opt, IScriptEnvironment *env);
	ShowCombedTIVTC::~ShowCombedTIVTC();
};

ShowCombedTIVTC::ShowCombedTIVTC(PClip _child, int _cthresh, bool _chroma, int _MI,
	int _blockx, int _blocky, int _metric, bool _debug, int _display, bool _fill, 
	int _opt, IScriptEnvironment *env) : GenericVideoFilter(_child), cthresh(_cthresh),
	chroma(_chroma), MI(_MI), blockx(_blockx), blocky(_blocky), metric(_metric), 
	debug(_debug), display(_display), fill(_fill), opt(_opt)
{
	cArray = NULL;
	cmask = NULL;
	if (!vi.IsYV12() && !vi.IsYUY2())
		env->ThrowError("ShowCombedTIVTC:  only YV12 and YUY2 input supported!");
	if (vi.height&1)
		env->ThrowError("ShowCombedTIVTC:  height must be mod 2!");
	if (display < 0 || display > 5)
		env->ThrowError("ShowCombedTIVTC:  display must be set to 0, 1, 2, 3, 4, or 5!");
	if (blockx != 4 && blockx != 8 && blockx != 16 && blockx != 32 && blockx != 64 && 
		blockx != 128 && blockx != 256 && blockx != 512 && blockx != 1024 && blockx != 2048)
		env->ThrowError("ShowCombedTIVTC:  illegal blockx size!");
	if (blocky != 4 && blocky != 8 && blocky != 16 && blocky != 32 && blocky != 64 && 
		blocky != 128 && blocky != 256 && blocky != 512 && blocky != 1024 && blocky != 2048)
		env->ThrowError("ShowCombedTIVTC:  illegal blocky size!");
	if (MI < 0 || MI > blockx*blocky)
		env->ThrowError("ShowCombedTIVTC:  MI must be >= 0 and <= blockx*blocky!");
	if (!debug && display == 5)
		env->ThrowError("ShowCombedTIVTC:  either debug or display output must be enabled!");
	if (opt < 0 || opt > 4)
		env->ThrowError("ShowCombedTIVTC:  opt must be set to either 0, 1, 2, 3, or 4!");
	if (metric != 0 && metric != 1)
		env->ThrowError("ShowCombedTIVTC:  metric must be set to 0 or 1!");
	xhalf = blockx >> 1;
	yhalf = blocky >> 1;
	xshift = blockx == 4 ? 2 : blockx == 8 ? 3 : blockx == 16 ? 4 : blockx == 32 ? 5 :
		blockx == 64 ? 6 : blockx == 128 ? 7 : blockx == 256 ? 8 : blockx == 512 ? 9 : 
		blockx == 1024 ? 10 : 11;
	yshift = blocky == 4 ? 2 : blocky == 8 ? 3 : blocky == 16 ? 4 : blocky == 32 ? 5 :
		blocky == 64 ? 6 : blocky == 128 ? 7 : blocky == 256 ? 8 : blocky == 512 ? 9 : 
		blocky == 1024 ? 10 : 11;
	cArray = (int *)_aligned_malloc((((vi.width+xhalf)>>xshift)+1)*(((vi.height+yhalf)>>yshift)+1)*4*sizeof(int), 16);
	if (!cArray)
		env->ThrowError("ShowCombedTIVTC:  malloc failure (cArray)!");
	cmask = new PlanarFrame(vi, true);
	if (vi.IsYUY2())
	{
		xhalf *= 2;
		++xshift;
	}
	nfrms = vi.num_frames-1;
	child->SetCacheHints(CACHE_NOTHING, 0);
	if (debug)
	{
		sprintf(buf,"ShowCombedTIVTC:  %s by tritical\n", VERSION);
		OutputDebugString(buf);
		sprintf(buf,"ShowCombedTIVTC:  MI = %d  cthresh = %d  chroma = %c", MI, 
			cthresh, chroma ? 'T' : 'F');
		OutputDebugString(buf);
	}
}

ShowCombedTIVTC::~ShowCombedTIVTC()
{
	if (cArray) _aligned_free(cArray);
	if (cmask) delete cmask;
}

PVideoFrame __stdcall ShowCombedTIVTC::GetFrame(int n, IScriptEnvironment *env)
{
	if (n < 0) n = 0;
	else if (n > nfrms) n = nfrms;
	PVideoFrame src = child->GetFrame(n, env);
	int MICount, b_over, c_over, np = vi.IsYV12() ? 3 : 1;
	if (vi.IsYV12()) fillCombedYV12(src, MICount, b_over, c_over, env);
	else fillCombedYUY2(src, MICount, b_over, c_over, env);
	if (debug)
	{
		sprintf(buf, "ShowCombedTIVTC:  frame %d -  MIC = %d  b_above = %d  c_above = %d", n, 
			MICount, b_over, c_over);
		OutputDebugString(buf);
		if (MICount > MI)
		{
			sprintf(buf, "ShowCombedTIVTC:  frame %d -  COMBED FRAME!", n);
			OutputDebugString(buf);
		}
	}
	if (display != 5)
	{
		sprintf(buf, "ShowCombedTIVTC %s by tritical", VERSION);
		Draw(src, 0, 0, buf, np);
		sprintf(buf, "MI = %d  cthresh = %d  chroma = %c", MI, cthresh, chroma ? 'T' : 'F');
		Draw(src, 0, 1, buf, np);
		sprintf(buf, "MIC = %d  b_above = %d  c_above = %d", MICount, b_over, c_over);
		Draw(src, 0, 2, buf, np);
		if (MICount > MI)
		{
			sprintf(buf, "COMBED FRAME!");
			Draw(src, 0, 3, buf, np);
		}
	}
	return src;
}

void ShowCombedTIVTC::fillCombedYUY2(PVideoFrame &src, int &MICount, 
		int &b_over, int &c_over, IScriptEnvironment *env) 
{
	bool use_mmx = (env->GetCPUFlags()&CPUF_MMX) ? true : false;
	bool use_isse = (env->GetCPUFlags()&CPUF_INTEGER_SSE) ? true : false;
	bool use_sse2 = ((env->GetCPUFlags()&CPUF_SSE2) && IsIntelP4()) ? true : false;
	if (opt != 4)
	{
		if (opt == 0) use_mmx = use_isse = use_sse2 = false;
		else if (opt == 1) { use_mmx = true; use_isse = use_sse2 = false; }
		else if (opt == 2) { use_mmx = use_isse = true; use_sse2 = false; }
		else if (opt == 3) use_mmx = use_isse = use_sse2 = true;
	}
	const unsigned char *srcp = src->GetReadPtr();
	const int src_pitch = src->GetPitch();
	const int Width = src->GetRowSize();
	const int Height = src->GetHeight();
	const unsigned char *srcpp = srcp - src_pitch;
	const unsigned char *srcppp = srcpp - src_pitch;
	const unsigned char *srcpn = srcp + src_pitch;
	const unsigned char *srcpnn = srcpn + src_pitch;
	unsigned char *cmkw = cmask->GetPtr();
	const int cmk_pitch = cmask->GetPitch();
	const int inc = chroma ? 1 : 2;
	const int xblocks = ((Width+xhalf)>>xshift) + 1;
	const int xblocks4 = xblocks<<2;
	const int yblocks = ((Height+yhalf)>>yshift) + 1;
	const int arraysize = (xblocks*yblocks)<<2;
	if (cthresh < 0) { memset(cmkw, 255, Height*cmk_pitch); goto cjump; }
	fmemset(env->GetCPUFlags(),cmkw,Height*cmk_pitch,opt);
	if (metric == 0)
	{
		const int cthresh6 = cthresh*6;
		__int64 cthreshb[2] = { 0, 0 }, cthresh6w[2] = { 0, 0 };
		if (use_mmx || use_isse || use_sse2)
		{
			unsigned int cthresht = min(max(255-cthresh-1,0),255);
			cthreshb[0] = (cthresht<<24)+(cthresht<<16)+(cthresht<<8)+cthresht;
			cthreshb[0] += (cthreshb[0]<<32);
			cthreshb[1] = cthreshb[0];
			unsigned int cthresh6t = min(max(65535-cthresh*6-1,0),65535);
			cthresh6w[0] = (cthresh6t<<16)+cthresh6t;
			cthresh6w[0] += (cthresh6w[0]<<32);
			cthresh6w[1] = cthresh6w[0];
		}
		for (int x=0; x<Width; x+=inc)
		{
			const int sFirst = srcp[x] - srcpn[x];
			if (sFirst > cthresh || sFirst < -cthresh)
			{
				if (abs(srcpnn[x]+(srcp[x]<<2)+srcpnn[x]-(3*(srcpn[x]+srcpn[x]))) > cthresh6) 
					cmkw[x] = 0xFF;
			}
		}
		srcppp += src_pitch;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		srcpnn += src_pitch;
		cmkw += cmk_pitch;
		for (int x=0; x<Width; x+=inc)
		{
			const int sFirst = srcp[x] - srcpp[x];
			const int sSecond = srcp[x] - srcpn[x];
			if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
			{
				if (abs(srcpnn[x]+(srcp[x]<<2)+srcpnn[x]-(3*(srcpp[x]+srcpn[x]))) > cthresh6) 
					cmkw[x] = 0xFF;
			}
		}
		srcppp += src_pitch;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		srcpnn += src_pitch;
		cmkw += cmk_pitch;
		if (use_mmx || use_isse || use_sse2)
		{
			if (chroma)
			{
				if (use_sse2 && !((int(srcp)|int(cmkw)|src_pitch|cmk_pitch)&15))
				{
					__m128 cthreshb128, cthresh6w128;
					__asm
					{
						movups xmm1,xmmword ptr[cthreshb]
						movups xmm2,xmmword ptr[cthresh6w]
						movaps cthreshb128,xmm1
						movaps cthresh6w128,xmm2
					}
					check_combing_SSE2(srcp,cmkw,Width,Height-4,src_pitch,src_pitch*2,
						cmk_pitch,cthreshb128,cthresh6w128);
				}
				else if (use_isse)
					check_combing_iSSE(srcp,cmkw,Width,Height-4,src_pitch,src_pitch*2,
						cmk_pitch,cthreshb[0],cthresh6w[0]);
				else if (use_mmx)
					check_combing_MMX(srcp,cmkw,Width,Height-4,src_pitch,src_pitch*2,
						cmk_pitch,cthreshb[0],cthresh6w[0]);
				else env->ThrowError("ShowCombedTIVTC:  simd error (0)!");
				srcppp += src_pitch*(Height-4);
				srcpp += src_pitch*(Height-4);
				srcp += src_pitch*(Height-4);
				srcpn += src_pitch*(Height-4);
				srcpnn += src_pitch*(Height-4);
				cmkw += cmk_pitch*(Height-4);
			}
			else
			{
				if (use_sse2 && !((int(srcp)|int(cmkw)|src_pitch|cmk_pitch)&15))
				{
					__m128 cthreshb128, cthresh6w128;
					__asm
					{
						movups xmm1,xmmword ptr[cthreshb]
						movups xmm2,xmmword ptr[cthresh6w]
						movaps cthreshb128,xmm1
						movaps cthresh6w128,xmm2
					}
					check_combing_SSE2_Luma(srcp,cmkw,Width,Height-4,src_pitch,src_pitch*2,
						cmk_pitch,cthreshb128,cthresh6w128);
				}
				else if (use_isse)
					check_combing_iSSE_Luma(srcp,cmkw,Width,Height-4,src_pitch,src_pitch*2,
						cmk_pitch,cthreshb[0],cthresh6w[0]);
				else if (use_mmx)
					check_combing_MMX_Luma(srcp,cmkw,Width,Height-4,src_pitch,src_pitch*2,
						cmk_pitch,cthreshb[0],cthresh6w[0]);
				else env->ThrowError("ShowCombedTIVTC:  simd error (1)!");
				srcppp += src_pitch*(Height-4);
				srcpp += src_pitch*(Height-4);
				srcp += src_pitch*(Height-4);
				srcpn += src_pitch*(Height-4);
				srcpnn += src_pitch*(Height-4);
				cmkw += cmk_pitch*(Height-4);
			}
		}
		else
		{
			for (int y=2; y<Height-2; ++y)
			{
				for (int x=0; x<Width; x+=inc)
				{
					const int sFirst = srcp[x] - srcpp[x];
					const int sSecond = srcp[x] - srcpn[x];
					if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
					{
						if (abs(srcppp[x]+(srcp[x]<<2)+srcpnn[x]-(3*(srcpp[x]+srcpn[x]))) > cthresh6) 
							cmkw[x] = 0xFF;
					}	
				}
				srcppp += src_pitch;
				srcpp += src_pitch;
				srcp += src_pitch;
				srcpn += src_pitch;
				srcpnn += src_pitch;
				cmkw += cmk_pitch;
			}
		}
		for (int x=0; x<Width; x+=inc)
		{
			const int sFirst = srcp[x] - srcpp[x];
			const int sSecond = srcp[x] - srcpn[x];
			if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
			{
				if (abs(srcppp[x]+(srcp[x]<<2)+srcppp[x]-(3*(srcpp[x]+srcpn[x]))) > cthresh6) 
					cmkw[x] = 0xFF;
			}
		}
		srcppp += src_pitch;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		srcpnn += src_pitch;
		cmkw += cmk_pitch;
		for (int x=0; x<Width; x+=inc)
		{
			const int sFirst = srcp[x] - srcpp[x];
			if (sFirst > cthresh || sFirst < -cthresh)
			{
				if (abs(srcppp[x]+(srcp[x]<<2)+srcppp[x]-(3*(srcpp[x]+srcpp[x]))) > cthresh6) 
					cmkw[x] = 0xFF;
			}
		}
	}
	else
	{
		const int cthreshsq = cthresh*cthresh;
		__int64 cthreshb[2] = { 0, 0 };
		if (use_mmx || use_isse || use_sse2)
		{
			cthreshb[0] = cthreshsq;
			cthreshb[0] += (cthreshb[0]<<32);
			cthreshb[1] = cthreshb[0];
		}
		for (int x=0; x<Width; x+=inc)
		{
			if ((srcp[x]-srcpn[x])*(srcp[x]-srcpn[x]) > cthreshsq) 
				cmkw[x] = 0xFF;
		}
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		cmkw += cmk_pitch;
		if (use_mmx || use_isse || use_sse2)
		{
			if (chroma)
			{
				if (use_sse2 && !((int(srcp)|int(cmkw)|src_pitch|cmk_pitch)&15))
				{
					__m128 cthreshb128;
					__asm
					{
						movups xmm1,xmmword ptr[cthreshb]
						movaps cthreshb128,xmm1
					}
					check_combing_SSE2_M1(srcp,cmkw,Width,Height-2,src_pitch,
						cmk_pitch,cthreshb128);
				}
				else if (use_mmx)
					check_combing_MMX_M1(srcp,cmkw,Width,Height-2,src_pitch,
						cmk_pitch,cthreshb[0]);
				else env->ThrowError("ShowCombedTIVTC:  simd error (6)!");
				srcpp += src_pitch*(Height-2);
				srcp += src_pitch*(Height-2);
				srcpn += src_pitch*(Height-2);
				cmkw += cmk_pitch*(Height-2);
			}
			else
			{
				if (use_sse2 && !((int(srcp)|int(cmkw)|src_pitch|cmk_pitch)&15))
				{
					__m128 cthreshb128;
					__asm
					{
						movups xmm1,xmmword ptr[cthreshb]
						movaps cthreshb128,xmm1
					}
					check_combing_SSE2_Luma_M1(srcp,cmkw,Width,Height-2,src_pitch,
						cmk_pitch,cthreshb128);
				}
				else if (use_mmx)
					check_combing_MMX_Luma_M1(srcp,cmkw,Width,Height-2,src_pitch,
						cmk_pitch,cthreshb[0]);
				else env->ThrowError("ShowCombedTIVTC:  simd error (5)!");
				srcpp += src_pitch*(Height-2);
				srcp += src_pitch*(Height-2);
				srcpn += src_pitch*(Height-2);
				cmkw += cmk_pitch*(Height-2);
			}
		}
		else
		{
			for (int y=1; y<Height-1; ++y)
			{
				for (int x=0; x<Width; x+=inc)
				{
					if ((srcp[x]-srcpp[x])*(srcp[x]-srcpn[x]) > cthreshsq) 
						cmkw[x] = 0xFF;
				}
				srcpp += src_pitch;
				srcp += src_pitch;
				srcpn += src_pitch;
				cmkw += cmk_pitch;
			}
		}
		for (int x=0; x<Width; x+=inc)
		{
			if ((srcp[x]-srcpp[x])*(srcp[x]-srcpp[x]) > cthreshsq) 
					cmkw[x] = 0xFF;
		}
	}
cjump:
	if (chroma)
	{
		unsigned char *cmkp = cmask->GetPtr() + cmk_pitch;
		unsigned char *cmkpp = cmkp - cmk_pitch;
		unsigned char *cmkpn = cmkp + cmk_pitch;
		for (int y=1; y<Height-1; ++y)
		{
			for (int x=4; x<Width-4; x+=4)
			{
				if ((cmkp[x+1] == 0xFF && (cmkpp[x-3] == 0xFF || cmkpp[x+1] == 0xFF || cmkpp[x+5] == 0xFF || 
					cmkp[x-3] == 0xFF || cmkp[x+5] == 0xFF || cmkpn[x-3] == 0xFF || cmkpn[x+1] == 0xFF || 
					cmkpn[x+5] == 0xFF)) || (cmkp[x+3] == 0xFF && (cmkpp[x-1] == 0xFF || cmkpp[x+3] == 0xFF || 
					cmkpp[x+7] == 0xFF || cmkp[x-1] == 0xFF || cmkp[x+7] == 0xFF || cmkpn[x-1] == 0xFF || 
					cmkpn[x+3] == 0xFF || cmkpn[x+7] == 0xFF))) cmkp[x] = cmkp[x+2] = 0xFF;
			}
			cmkpp += cmk_pitch;
			cmkp += cmk_pitch;
			cmkpn += cmk_pitch;
		}
	}
	const unsigned char *cmkp = cmask->GetPtr() + cmk_pitch;
	const unsigned char *cmkpp = cmkp - cmk_pitch;
	const unsigned char *cmkpn = cmkp + cmk_pitch;
	memset(cArray,0,arraysize*sizeof(int));
	env->MakeWritable(&src);
	unsigned char *dstp = src->GetWritePtr();
	const int dst_pitch = src->GetPitch();
	dstp += dst_pitch;
	c_over = 0;
	for (int y=1; y<Height-1; ++y)
	{
		const int temp1 = (y>>yshift)*xblocks4;
		const int temp2 = ((y+yhalf)>>yshift)*xblocks4;
		for (int x=0; x<Width; x+=2)
		{
			if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF)
			{
				const int box1 = (x>>xshift)<<2;
				const int box2 = ((x+xhalf)>>xshift)<<2;
				++cArray[temp1+box1+0];
				++cArray[temp1+box2+1];
				++cArray[temp2+box1+2];
				++cArray[temp2+box2+3];
				++c_over;
				if (display == 0 || (display > 2 && display != 5)) dstp[x] = 0xFF;
			}
		}
		cmkpp += cmk_pitch;
		cmkp += cmk_pitch;
		cmkpn += cmk_pitch;
		dstp += dst_pitch;
	}
	MICount = -1;
	b_over = 0;
	int high_block = 0;
	for (int x=0; x<arraysize; ++x)
	{
		if (cArray[x] > MICount) 
		{
			MICount = cArray[x];
			high_block = x;
		}
		if (cArray[x] > MI)
		{
			++b_over;
			if (display == 2 || display == 4)
			{
				if (fill) fillBox(src, x, xblocks4);
				else drawBox(src, x, xblocks4, vi.IsYV12() ? 3 : 1);
			}
		}
	}
	if (display == 1 || display == 3)
	{
		if (fill) fillBox(src, high_block, xblocks4);
		else drawBox(src, high_block, xblocks4, vi.IsYV12() ? 3 : 1);
	}
}

void ShowCombedTIVTC::fillCombedYV12(PVideoFrame &src, int &MICount,
			 int &b_over, int &c_over, IScriptEnvironment *env) 
{
	bool use_mmx = (env->GetCPUFlags()&CPUF_MMX) ? true : false;
	bool use_isse = (env->GetCPUFlags()&CPUF_INTEGER_SSE) ? true : false;
	bool use_sse2 = ((env->GetCPUFlags()&CPUF_SSE2) && IsIntelP4()) ? true : false;
	if (opt != 4)
	{
		if (opt == 0) use_mmx = use_isse = use_sse2 = false;
		else if (opt == 1) { use_mmx = true; use_isse = use_sse2 = false; }
		else if (opt == 2) { use_mmx = use_isse = true; use_sse2 = false; }
		else if (opt == 3) use_mmx = use_isse = use_sse2 = true;
	}
	const int cthresh6 = cthresh*6;
	__int64 cthreshb[2] = { 0, 0} , cthresh6w[2] = { 0, 0 };
	if (metric == 0 && (use_mmx || use_isse || use_sse2))
	{
		unsigned int cthresht = min(max(255-cthresh-1,0),255);
		cthreshb[0] = (cthresht<<24)+(cthresht<<16)+(cthresht<<8)+cthresht;
		cthreshb[0] += (cthreshb[0]<<32);
		cthreshb[1] = cthreshb[0];
		unsigned int cthresh6t = min(max(65535-cthresh*6-1,0),65535);
		cthresh6w[0] = (cthresh6t<<16)+cthresh6t;
		cthresh6w[0] += (cthresh6w[0]<<32);
		cthresh6w[1] = cthresh6w[0];
	}
	else if (metric == 1 && (use_mmx || use_isse || use_sse2))
	{
		cthreshb[0] = cthresh*cthresh;
		cthreshb[0] += (cthreshb[0]<<32);
		cthreshb[1] = cthreshb[0];
	}
	for (int b=chroma ? 3 : 1; b>0; --b)
	{
		int plane;
		if (b == 3) plane = PLANAR_U;
		else if (b == 2) plane = PLANAR_V;
		else plane = PLANAR_Y;
		const unsigned char *srcp = src->GetReadPtr(plane);
		const int src_pitch = src->GetPitch(plane);
		const int Width = src->GetRowSize(plane);
		const int Height = src->GetHeight(plane);
		const unsigned char *srcpp = srcp - src_pitch;
		const unsigned char *srcppp = srcpp - src_pitch;
		const unsigned char *srcpn = srcp + src_pitch;
		const unsigned char *srcpnn = srcpn + src_pitch;
		unsigned char *cmkp = cmask->GetPtr(b-1);
		const int cmk_pitch = cmask->GetPitch(b-1);
		if (cthresh < 0) { memset(cmkp,255,Height*cmk_pitch); continue; }
		fmemset(env->GetCPUFlags(),cmkp,Height*cmk_pitch,opt);
		if (metric == 0)
		{
			for (int x=0; x<Width; ++x)
			{
				const int sFirst = srcp[x] - srcpn[x];
				if (sFirst > cthresh || sFirst < -cthresh)
				{
					if (abs(srcpnn[x]+(srcp[x]<<2)+srcpnn[x]-(3*(srcpn[x]+srcpn[x]))) > cthresh6) 
						cmkp[x] = 0xFF;
				}
			}
			srcppp += src_pitch;
			srcpp += src_pitch;
			srcp += src_pitch;
			srcpn += src_pitch;
			srcpnn += src_pitch;
			cmkp += cmk_pitch;
			for (int x=0; x<Width; ++x)
			{
				const int sFirst = srcp[x] - srcpp[x];
				const int sSecond = srcp[x] - srcpn[x];
				if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
				{
					if (abs(srcpnn[x]+(srcp[x]<<2)+srcpnn[x]-(3*(srcpp[x]+srcpn[x]))) > cthresh6) 
						cmkp[x] = 0xFF;
				}
			}
			srcppp += src_pitch;
			srcpp += src_pitch;
			srcp += src_pitch;
			srcpn += src_pitch;
			srcpnn += src_pitch;
			cmkp += cmk_pitch;
			if (use_mmx || use_isse || use_sse2)
			{
				if (use_sse2 && !((int(srcp)|int(cmkp)|cmk_pitch|src_pitch)&15))
				{
					__m128 cthreshb128, cthresh6w128;
					__asm
					{
						movups xmm1,xmmword ptr[cthreshb]
						movups xmm2,xmmword ptr[cthresh6w]
						movaps cthreshb128,xmm1
						movaps cthresh6w128,xmm2
					}
					check_combing_SSE2(srcp, cmkp, Width, Height-4, src_pitch, 
						src_pitch*2, cmk_pitch, cthreshb128, cthresh6w128);
				}
				else if (use_isse)
					check_combing_iSSE(srcp, cmkp, Width, Height-4, src_pitch, 
						src_pitch*2, cmk_pitch, cthreshb[0], cthresh6w[0]);
				else if (use_mmx)
					check_combing_MMX(srcp, cmkp, Width, Height-4, src_pitch,
						src_pitch*2, cmk_pitch, cthreshb[0], cthresh6w[0]);
				else env->ThrowError("ShowCombedTIVTC:  simd error (3)!");
				srcppp += src_pitch*(Height-4);
				srcpp += src_pitch*(Height-4);
				srcp += src_pitch*(Height-4);
				srcpn += src_pitch*(Height-4);
				srcpnn += src_pitch*(Height-4);
				cmkp += cmk_pitch*(Height-4);
			}
			else
			{
				for (int y=2; y<Height-2; ++y)
				{
					for (int x=0; x<Width; ++x)
					{
						const int sFirst = srcp[x] - srcpp[x];
						const int sSecond = srcp[x] - srcpn[x];
						if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
						{
							if (abs(srcppp[x]+(srcp[x]<<2)+srcpnn[x]-(3*(srcpp[x]+srcpn[x]))) > cthresh6) 
								cmkp[x] = 0xFF;
						}	
					}
					srcppp += src_pitch;
					srcpp += src_pitch;
					srcp += src_pitch;
					srcpn += src_pitch;
					srcpnn += src_pitch;
					cmkp += cmk_pitch;
				}
			}
			for (int x=0; x<Width; ++x)
			{
				const int sFirst = srcp[x] - srcpp[x];
				const int sSecond = srcp[x] - srcpn[x];
				if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
				{
					if (abs(srcppp[x]+(srcp[x]<<2)+srcppp[x]-(3*(srcpp[x]+srcpn[x]))) > cthresh6) 
						cmkp[x] = 0xFF;
				}
			}
			srcppp += src_pitch;
			srcpp += src_pitch;
			srcp += src_pitch;
			srcpn += src_pitch;
			srcpnn += src_pitch;
			cmkp += cmk_pitch;
			for (int x=0; x<Width; ++x)
			{
				const int sFirst = srcp[x] - srcpp[x];
				if (sFirst > cthresh || sFirst < -cthresh)
				{
					if (abs(srcppp[x]+(srcp[x]<<2)+srcppp[x]-(3*(srcpp[x]+srcpp[x]))) > cthresh6) 
						cmkp[x] = 0xFF;
				}
			}
		}
		else
		{
			const int cthreshsq = cthresh*cthresh;
			for (int x=0; x<Width; ++x)
			{
				if ((srcp[x]-srcpn[x])*(srcp[x]-srcpn[x]) > cthreshsq) 
						cmkp[x] = 0xFF;
			}
			srcpp += src_pitch;
			srcp += src_pitch;
			srcpn += src_pitch;
			cmkp += cmk_pitch;
			if (use_mmx || use_isse || use_sse2)
			{
				if (use_sse2 && !((int(srcp)|int(cmkp)|cmk_pitch|src_pitch)&15))
				{
					__m128 cthreshb128;
					__asm
					{
						movups xmm1,xmmword ptr[cthreshb]
						movaps cthreshb128,xmm1
					}
					check_combing_SSE2_M1(srcp, cmkp, Width, Height-2, src_pitch, cmk_pitch, 
						cthreshb128);
				}
				else if (use_mmx)
					check_combing_MMX_M1(srcp, cmkp, Width, Height-2, src_pitch, cmk_pitch, 
						cthreshb[0]);
				else env->ThrowError("ShowCombedTIVTC:  simd error (4)!");
				srcpp += src_pitch*(Height-2);
				srcp += src_pitch*(Height-2);
				srcpn += src_pitch*(Height-2);
				cmkp += cmk_pitch*(Height-2);
			}
			else
			{
				for (int y=1; y<Height-1; ++y)
				{
					for (int x=0; x<Width; ++x)
					{
						if ((srcp[x]-srcpp[x])*(srcp[x]-srcpn[x]) > cthreshsq) 
								cmkp[x] = 0xFF;
					}
					srcpp += src_pitch;
					srcp += src_pitch;
					srcpn += src_pitch;
					cmkp += cmk_pitch;
				}
			}
			for (int x=0; x<Width; ++x)
			{
				if ((srcp[x]-srcpp[x])*(srcp[x]-srcpp[x]) > cthreshsq) 
					cmkp[x] = 0xFF;
			}
		}
	}
	if (chroma)
	{
		unsigned char *cmkp = cmask->GetPtr(0);
		unsigned char *cmkpU = cmask->GetPtr(1);
		unsigned char *cmkpV = cmask->GetPtr(2);
		const int Width = cmask->GetWidth(2);
		const int Height = cmask->GetHeight(2);
		const int cmk_pitch = cmask->GetPitch(0)<<1;
		const int cmk_pitchUV = cmask->GetPitch(2);
		unsigned char *cmkpp = cmkp - (cmk_pitch>>1);
		unsigned char *cmkpn = cmkp + (cmk_pitch>>1);
		unsigned char *cmkpnn = cmkpn + (cmk_pitch>>1);
		unsigned char *cmkppU = cmkpU - cmk_pitchUV;
		unsigned char *cmkpnU = cmkpU + cmk_pitchUV;
		unsigned char *cmkppV = cmkpV - cmk_pitchUV;
		unsigned char *cmkpnV = cmkpV + cmk_pitchUV;
		for (int y=1; y<Height-1; ++y)
		{
			cmkpp += cmk_pitch;
			cmkp += cmk_pitch;
			cmkpn += cmk_pitch;
			cmkpnn += cmk_pitch;
			cmkppV += cmk_pitchUV;
			cmkpV += cmk_pitchUV;
			cmkpnV += cmk_pitchUV;
			cmkppU += cmk_pitchUV;
			cmkpU += cmk_pitchUV;
			cmkpnU += cmk_pitchUV;
			for (int x=1; x<Width-1; ++x)
			{
				if ((cmkpV[x] == 0xFF && (cmkpV[x-1] == 0xFF || cmkpV[x+1] == 0xFF ||
					 cmkppV[x-1] == 0xFF || cmkppV[x] == 0xFF || cmkppV[x+1] == 0xFF ||
					 cmkpnV[x-1] == 0xFF || cmkpnV[x] == 0xFF || cmkpnV[x+1] == 0xFF)) || 
					(cmkpU[x] == 0xFF && (cmkpU[x-1] == 0xFF || cmkpU[x+1] == 0xFF ||
					 cmkppU[x-1] == 0xFF || cmkppU[x] == 0xFF || cmkppU[x+1] == 0xFF ||
					 cmkpnU[x-1] == 0xFF || cmkpnU[x] == 0xFF || cmkpnU[x+1] == 0xFF)))
				{
					((unsigned short*)cmkp)[x] = (unsigned short) 0xFFFF;
					((unsigned short*)cmkpn)[x] = (unsigned short) 0xFFFF;
					if (y&1) ((unsigned short*)cmkpp)[x] = (unsigned short) 0xFFFF;
					else ((unsigned short*)cmkpnn)[x] = (unsigned short) 0xFFFF;
				}
			}
		}
	}
	const int cmk_pitch = cmask->GetPitch(0);
	const unsigned char *cmkp = cmask->GetPtr(0) + cmk_pitch;
	const unsigned char *cmkpp = cmkp - cmk_pitch;
	const unsigned char *cmkpn = cmkp + cmk_pitch;
	const int Width = cmask->GetWidth(0);
	const int Height = cmask->GetHeight(0);
	const int xblocks = ((Width+xhalf)>>xshift) + 1;
	const int xblocks4 = xblocks<<2;
	const int yblocks = ((Height+yhalf)>>yshift) + 1;
	const int arraysize = (xblocks*yblocks)<<2;
	memset(cArray,0,arraysize*sizeof(int));
	env->MakeWritable(&src);
	unsigned char *dstp = src->GetWritePtr(PLANAR_Y);
	const int dst_pitch = src->GetPitch(PLANAR_Y);
	dstp += dst_pitch;
	c_over = 0;
	for (int y=1; y<Height-1; ++y)
	{
		const int temp1 = (y>>yshift)*xblocks4;
		const int temp2 = ((y+yhalf)>>yshift)*xblocks4;
		for (int x=0; x<Width; ++x)
		{
			if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF)
			{
				const int box1 = (x>>xshift)<<2;
				const int box2 = ((x+xhalf)>>xshift)<<2;
				++cArray[temp1+box1+0];
				++cArray[temp1+box2+1];
				++cArray[temp2+box1+2];
				++cArray[temp2+box2+3];
				++c_over;
				if (display == 0 || (display > 2 && display != 5)) dstp[x] = 0xFF;
			}
		}
		cmkpp += cmk_pitch;
		cmkp += cmk_pitch;
		cmkpn += cmk_pitch;
		dstp += dst_pitch;
	}
	MICount = -1;
	b_over = 0;
	int high_block = 0;
	for (int x=0; x<arraysize; ++x)
	{
		if (cArray[x] > MICount) 
		{
			MICount = cArray[x];
			high_block = x;
		}
		if (cArray[x] > MI)
		{
			++b_over;
			if (display == 2 || display == 4)
			{
				if (fill) fillBox(src, x, xblocks4);
				else drawBox(src, x, xblocks4, vi.IsYV12() ? 3 : 1);
			}
		}
	}
	if (display == 1 || display == 3)
	{
		if (fill) fillBox(src, high_block, xblocks4);
		else drawBox(src, high_block, xblocks4, vi.IsYV12() ? 3 : 1);
	}
}

void ShowCombedTIVTC::fillBox(PVideoFrame &dst, int blockN, int xblocks)
{
	if (vi.IsYV12()) return fillBoxYV12(dst, blockN, xblocks);
	else return fillBoxYUY2(dst, blockN, xblocks);
}

void ShowCombedTIVTC::drawBox(PVideoFrame &dst, int blockN, int xblocks, int np)
{
	if (np == 3) drawBoxYV12(dst, blockN, xblocks);
	else drawBoxYUY2(dst, blockN, xblocks);
}

void ShowCombedTIVTC::Draw(PVideoFrame &dst, int x1, int y1, const char *s, int np)
{
	if (np == 3) DrawYV12(dst, x1, y1, s);
	else DrawYUY2(dst, x1, y1, s);
}

void ShowCombedTIVTC::fillBoxYUY2(PVideoFrame &dst, int blockN, int xblocks)
{
	unsigned char *dstp = dst->GetWritePtr();
	int pitch = dst->GetPitch();
	int width = dst->GetRowSize();
	int height = dst->GetHeight();
	int cordy, cordx, x, y, temp, xlim, ylim;
	cordy = blockN / xblocks;
	cordx = blockN - (cordy*xblocks);
	temp = cordx%4;
	cordx = (cordx>>2);
	cordy *= blocky;
	cordx *= (blockx<<1);
	if (temp == 1) cordx -= blockx;
	else if (temp == 2) cordy -= (blocky>>1);
	else if (temp == 3) { cordx -= blockx; cordy -= (blocky>>1); }
	xlim = cordx + 2*blockx;
	ylim = cordy + blocky;
	int ymid = max(min(cordy + ((ylim-cordy)>>1),height-1),0);
	int xmid = max(min(cordx + ((xlim-cordx)>>1),width-2),0);
	if (xlim > width) xlim = width;
	if (ylim > height) ylim = height;
	cordy = max(cordy,0);
	cordx = max(cordx,0);
	dstp = dstp+cordy*pitch;
	for (y=cordy; y<ylim; ++y)
	{
		for (x=cordx; x<xlim; x+=4)
		{
			if (y == ymid && (x == xmid || x+2 == xmid))
			{
				dstp[x] = 0;
				dstp[x+1] = 128;
				dstp[x+2] = 0;
				dstp[x+3] = 128;
			}
			else if (!(dstp[x] == 0 && dstp[x+2] == 0 && 
					(x <= 1 || dstp[x-2] == 255) && 
					(x >= width-4 || dstp[x+4] == 255) && 
					(y == 0 || dstp[x-pitch] == 255) &&
					(y == height-1 || dstp[x+pitch] == 255)))
			{
				dstp[x] = 255;
				dstp[x+1] = 128;
				dstp[x+2] = 255;
				dstp[x+3] = 128;
			}
		}
		dstp += pitch;
	}
}

void ShowCombedTIVTC::drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks)
{
	unsigned char *dstp = dst->GetWritePtr();
	int pitch = dst->GetPitch();
	int width = dst->GetRowSize();
	int height = dst->GetHeight();
	int cordy, cordx, x, y, temp, xlim, ylim;
	cordy = blockN / xblocks;
	cordx = blockN - (cordy*xblocks);
	temp = cordx%4;
	cordx = (cordx>>2);
	cordy *= blocky;
	cordx *= (blockx<<1);
	if (temp == 1) cordx -= blockx;
	else if (temp == 2) cordy -= (blocky>>1);
	else if (temp == 3) { cordx -= blockx; cordy -= (blocky>>1); }
	xlim = cordx + 2*blockx;
	if (xlim > width) xlim = width;
	ylim = cordy + blocky;
	if (ylim > height) ylim = height;
	for (y=max(cordy,0), temp=cordx+2*(blockx-1); y<ylim; ++y)
	{
		if (cordx >= 0) (dstp+y*pitch)[cordx] = (dstp+y*pitch)[cordx] <= 128 ? 255 : 0;
		if (temp < width) (dstp+y*pitch)[temp] = (dstp+y*pitch)[temp] <= 128 ? 255 : 0;
	}
	for (x=max(cordx,0), temp=cordy+blocky-1; x<xlim; x+=4)
	{
		if (cordy >= 0)
		{
			(dstp+cordy*pitch)[x] = (dstp+cordy*pitch)[x] <= 128 ? 255 : 0;
			(dstp+cordy*pitch)[x+1] = 128;
			(dstp+cordy*pitch)[x+2] = (dstp+cordy*pitch)[x+2] <= 128 ? 255 : 0;
			(dstp+cordy*pitch)[x+3] = 128;
		}
		if (temp < height)
		{
			(dstp+temp*pitch)[x] = (dstp+temp*pitch)[x] <= 128 ? 255 : 0;
			(dstp+temp*pitch)[x+1] = 128;
			(dstp+temp*pitch)[x+2] = (dstp+temp*pitch)[x+2] <= 128 ? 255 : 0;
			(dstp+temp*pitch)[x+3] = 128;
		}
	}
}

void ShowCombedTIVTC::DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s) 
{
	int x, y = y1 * 20, num, pitch = dst->GetPitch();
	unsigned char *dp;
	unsigned int width = dst->GetRowSize();
	int height = dst->GetHeight();
	if (y+20 >= height) return;
	for (int xx = 0; *s; ++s, ++xx)
	{
		x = (x1 + xx) * 10;
		if ((x+10)*2 >= (int)(width)) return;
		num = *s - ' ';
		for (int tx = 0; tx < 10; tx++) 
		{
			for (int ty = 0; ty < 20; ty++) 
			{
				dp = &dst->GetWritePtr()[(x + tx) * 2 + (y + ty) * pitch];
				if (font[num][ty] & (1 << (15 - tx))) 
				{
					if (tx & 1) 
					{
						dp[0] = 255;
						dp[-1] = 128;
						dp[1] = 128;
					} 
					else 
					{
						dp[0] = 255;
						dp[1] = 128;
						dp[3] = 128;
					}
				} 
				else 
				{
					if (tx & 1) 
					{
						dp[0] = (unsigned char) (dp[0] >> 1);
						dp[-1] = (unsigned char) ((dp[-1] + 128) >> 1);
						dp[1] = (unsigned char) ((dp[1] + 128) >> 1);
					} 
					else 
					{
						dp[0] = (unsigned char) (dp[0] >> 1);
						dp[1] = (unsigned char) ((dp[1] + 128) >> 1);
						dp[3] = (unsigned char) ((dp[3] + 128) >> 1);
					}
				}
			}
		}
	}
}

void ShowCombedTIVTC::fillBoxYV12(PVideoFrame &dst, int blockN, int xblocks)
{
	unsigned char *dstp = dst->GetWritePtr(PLANAR_Y);
	int width = dst->GetRowSize(PLANAR_Y);
	int height = dst->GetHeight(PLANAR_Y);
	int pitch = dst->GetPitch(PLANAR_Y);
	int cordy, cordx, x, y, temp, xlim, ylim;
	cordy = blockN / xblocks;
	cordx = blockN - (cordy*xblocks);
	temp = cordx%4;
	cordx = (cordx>>2);
	cordy *= blocky;
	cordx *= blockx;
	if (temp == 1) cordx -= (blockx>>1);
	else if (temp == 2) cordy -= (blocky>>1);
	else if (temp == 3) { cordx -= (blockx>>1); cordy -= (blocky>>1); }
	xlim = cordx + blockx;
	ylim = cordy + blocky;
	int ymid = max(min(cordy + ((ylim-cordy)>>1),height-1),0);
	int xmid = max(min(cordx + ((xlim-cordx)>>1),width-1),0);
	if (xlim > width) xlim = width;
	if (ylim > height) ylim = height;
	cordy = max(cordy,0);
	cordx = max(cordx,0);
	dstp = dstp+cordy*pitch;
	for (y=cordy; y<ylim; ++y)
	{
		for (x=cordx; x<xlim; ++x)
		{
			if (y == ymid && x == xmid) dstp[x] = 0;
			else if (!(dstp[x] == 0 && 
					(x == width-1 || dstp[x+1] == 255) &&
					(x == 0 || dstp[x-1] == 255) &&
					(y == height-1 || dstp[x+pitch] == 255) &&
					(y == 0 || dstp[x-pitch] == 255))) dstp[x] = 255;
		}
		dstp += pitch;
	}
}

void ShowCombedTIVTC::drawBoxYV12(PVideoFrame &dst, int blockN, int xblocks)
{
	unsigned char *dstp = dst->GetWritePtr(PLANAR_Y);
	int width = dst->GetRowSize(PLANAR_Y);
	int height = dst->GetHeight(PLANAR_Y);
	int pitch = dst->GetPitch(PLANAR_Y);
	int cordy, cordx, x, y, temp, xlim, ylim;
	cordy = blockN / xblocks;
	cordx = blockN - (cordy*xblocks);
	temp = cordx%4;
	cordx = (cordx>>2);
	cordy *= blocky;
	cordx *= blockx;
	if (temp == 1) cordx -= (blockx>>1);
	else if (temp == 2) cordy -= (blocky>>1);
	else if (temp == 3) { cordx -= (blockx>>1); cordy -= (blocky>>1); }
	xlim = cordx + blockx;
	if (xlim > width) xlim = width;
	ylim = cordy + blocky;
	if (ylim > height) ylim = height;
	for (y=max(cordy,0), temp=cordx+blockx-1; y<ylim; ++y)
	{
		if (cordx >= 0) (dstp+y*pitch)[cordx] = (dstp+y*pitch)[cordx] <= 128 ? 255 : 0;
		if (temp < width) (dstp+y*pitch)[temp] = (dstp+y*pitch)[temp] <= 128 ? 255 : 0;
	}
	for (x=max(cordx,0), temp=cordy+blocky-1; x<xlim; ++x)
	{
		if (cordy >= 0) (dstp+cordy*pitch)[x] = (dstp+cordy*pitch)[x] <= 128 ? 255 : 0;
		if (temp < height) (dstp+temp*pitch)[x] = (dstp+temp*pitch)[x] <= 128 ? 255 : 0;
	}
}

void ShowCombedTIVTC::DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s) 
{
	int x, y = y1 * 20, num, tx, ty;
	int pitchY = dst->GetPitch(PLANAR_Y), pitchUV = dst->GetPitch(PLANAR_V);
	unsigned char *dpY, *dpU, *dpV;
	unsigned int width = dst->GetRowSize(PLANAR_Y);
	int height = dst->GetHeight(PLANAR_Y);
	if (y+20 >= height) return;
	for (int xx = 0; *s; ++s, ++xx) 
	{
		x = (x1 + xx) * 10;
		if (x+10 >= (int)(width)) return;
		num = *s - ' ';
		for (tx = 0; tx < 10; tx++)
		{
			for (ty = 0; ty < 20; ty++)
			{
				dpY = &dst->GetWritePtr(PLANAR_Y)[(x + tx) + (y + ty) * pitchY];
				if (font[num][ty] & (1 << (15 - tx))) *dpY = 255;
				else *dpY = (unsigned char) (*dpY >> 1);
			}
		}
		for (tx = 0; tx < 10; tx++)
		{
			for (ty = 0; ty < 20; ty++)
			{
				dpU = &dst->GetWritePtr(PLANAR_U)[((x + tx)/2) + ((y + ty)/2) * pitchUV];
				dpV = &dst->GetWritePtr(PLANAR_V)[((x + tx)/2) + ((y + ty)/2) * pitchUV];
				if (font[num][ty] & (1 << (15 - tx)))
				{
					*dpU = 128;
					*dpV = 128;
				} 
				else
				{
					*dpU = (unsigned char) ((*dpU + 128) >> 1);
					*dpV = (unsigned char) ((*dpV + 128) >> 1);
				}
			}
		}
	}
}

AVSValue __cdecl Create_ShowCombedTIVTC(AVSValue args, void* user_data, IScriptEnvironment* env) 
{
	return new ShowCombedTIVTC(args[0].AsClip(),args[1].AsInt(9),args[2].AsBool(false),
		args[3].AsInt(80),args[4].AsInt(16),args[5].AsInt(16),args[6].AsInt(0),
		args[7].AsBool(false),args[8].AsInt(3),args[9].AsBool(false),args[10].AsInt(4),env);
}

// These are just copied from TFMASM.cpp.  One day I'll make it
// so I don't have duplicate code everywhere in this pos...

__declspec(align(16)) const __int64 lumaMask[2] = { 0x00FF00FF00FF00FF, 0x00FF00FF00FF00FF };
__declspec(align(16)) const __int64 threeMask[2] = { 0x0003000300030003, 0x0003000300030003 };
__declspec(align(16)) const __int64 ffMask[2] = { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF };

void ShowCombedTIVTC::check_combing_SSE2(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __m128 threshb, __m128 thresh6w)
{
	__asm
	{
		mov eax,srcp
		mov edx,dstp
		mov edi,src_pitch
		add eax,edi
		movdqa xmm6,threshb
		pcmpeqb xmm7,xmm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]	// next
		sub eax,edi
		movdqa xmm4,[eax+ecx]	// srcp
		movdqa xmm5,xmm0		// cpy next
		sub eax,edi
		movdqa xmm1,[eax+ecx]	// prev
		movdqa xmm2,xmm4		// cpy srcp
		movdqa xmm3,xmm1		// cpy prev
		psubusb xmm5,xmm2		// next-srcp
		psubusb xmm3,xmm4		// prev-srcp
		psubusb xmm2,xmm0		// srcp-next
		psubusb xmm4,xmm1		// srcp-prev 
		pminub xmm3,xmm5
		pminub xmm2,xmm4
		pmaxub xmm2,xmm3
		paddusb xmm2,xmm6
		pcmpeqb xmm2,xmm7
		movdqa xmm3,xmm2
		psrldq xmm2,4
		movd edi,xmm3
		movd esi,xmm2
		or edi,esi
		jnz output2
		movdqa xmm4,xmm2
		psrldq xmm2,4
		psrldq xmm4,8
		movd edi,xmm2
		movd esi,xmm4
		or edi,esi
		jnz output2
		mov edi,src_pitch
		add ecx,16
		lea eax,[eax+edi*2]
		cmp ecx,width
		jl xloop
		add eax,edi
		add edx,dst_pitch
		dec height
		jnz yloop
		jmp end
output2:
		mov esi,src_pitch2
		mov edi,src_pitch
		pxor xmm7,xmm7
		movdqa xmm2,xmm0
		movdqa xmm4,xmm1
		punpcklbw xmm2,xmm7
		punpckhbw xmm0,xmm7
		sub eax,edi
		punpcklbw xmm4,xmm7
		punpckhbw xmm1,xmm7
		paddusw xmm2,xmm4
		paddusw xmm0,xmm1
		pmullw xmm2,threeMask	// 3*(p+n)
		pmullw xmm0,threeMask	// 3*(p+n)
		movdqa xmm1,[eax+ecx]
		movdqa xmm4,xmm1
		punpcklbw xmm4,xmm7
		add eax,esi
		punpckhbw xmm1,xmm7
		movdqa xmm5,[eax+ecx]
		movdqa xmm6,xmm5
		punpcklbw xmm6,xmm7
		punpckhbw xmm5,xmm7
		psllw xmm6,2
		add eax,esi
		psllw xmm5,2
		paddusw xmm4,xmm6
		paddusw xmm1,xmm5
		movdqa xmm5,[eax+ecx]
		movdqa xmm6,xmm5
		punpcklbw xmm6,mm7
		punpckhbw xmm5,mm7
		paddusw xmm4,xmm6			// (pp+c*4+nn)
		paddusw xmm1,xmm5			// (pp+c*4+nn)
		movdqa xmm6,xmm4
		movdqa xmm5,xmm1
		psubusw xmm6,xmm2
		psubusw xmm5,xmm0
		psubusw xmm2,xmm4
		psubusw xmm0,xmm1
		pcmpeqb xmm7,xmm7
		pmaxsw xmm2,xmm6
		pmaxsw xmm0,xmm5
		paddusw xmm2,thresh6w
		paddusw xmm0,thresh6w
		pcmpeqw xmm2,xmm7
		pcmpeqw xmm0,xmm7
		sub eax,edi
		psrlw xmm2,8
		movdqa xmm6,threshb
		psrlw xmm0,8
		packuswb xmm2,xmm0
		pand xmm3,xmm2
		movdqa [edx+ecx],xmm3
		add ecx,16
		cmp ecx,width
		jl xloop
		add eax,edi
		add edx,dst_pitch
		dec height
		jnz yloop
end:
	}
}

void ShowCombedTIVTC::check_combing_iSSE(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w)
{
	__asm
	{
		mov eax,srcp
		mov ebx,dstp
		mov edx,width
		mov edi,src_pitch
		add eax,edi
		movq mm6,threshb
		pcmpeqb mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]	// next
		sub eax,edi
		movq mm4,[eax+ecx]	// srcp
		movq mm5,mm0		// cpy next
		sub eax,edi
		movq mm1,[eax+ecx]	// prev
		movq mm2,mm4		// cpy srcp
		movq mm3,mm1		// cpy prev
		psubusb mm5,mm2		// next-srcp
		psubusb mm3,mm4		// prev-srcp
		psubusb mm2,mm0		// srcp-next
		psubusb mm4,mm1		// srcp-prev 
		pminub mm3,mm5
		pminub mm2,mm4
		pmaxub mm2,mm3
		paddusb mm2,mm6
		pcmpeqb mm2,mm7
		movq mm3,mm2
		psrlq mm2,32
		movd edi,mm3
		movd esi,mm2
		or edi,esi
		jnz output2
		mov edi,src_pitch
		add ecx,8
		lea eax,[eax+edi*2]
		cmp ecx,edx
		jl xloop
		add eax,edi
		add ebx,dst_pitch
		dec height
		jnz yloop
		jmp end
output2:
		mov esi,src_pitch2
		mov edi,src_pitch
		pxor mm7,mm7
		movq mm2,mm0
		movq mm4,mm1
		punpcklbw mm2,mm7
		punpckhbw mm0,mm7
		sub eax,edi
		punpcklbw mm4,mm7
		punpckhbw mm1,mm7
		paddusw mm2,mm4
		paddusw mm0,mm1
		pmullw mm2,threeMask	// 3*(p+n)
		pmullw mm0,threeMask	// 3*(p+n)
		movq mm1,[eax+ecx]
		movq mm4,mm1
		punpcklbw mm4,mm7
		add eax,esi
		punpckhbw mm1,mm7
		movq mm5,[eax+ecx]
		movq mm6,mm5
		punpcklbw mm6,mm7
		punpckhbw mm5,mm7
		psllw mm6,2
		add eax,esi
		psllw mm5,2
		paddusw mm4,mm6
		paddusw mm1,mm5
		movq mm5,[eax+ecx]
		movq mm6,mm5
		punpcklbw mm6,mm7
		punpckhbw mm5,mm7
		paddusw mm4,mm6			// (pp+c*4+nn)
		paddusw mm1,mm5			// (pp+c*4+nn)
		movq mm6,mm4
		movq mm5,mm1
		psubusw mm6,mm2
		psubusw mm5,mm0
		psubusw mm2,mm4
		psubusw mm0,mm1
		pcmpeqb mm7,mm7
		pmaxsw mm2,mm6
		pmaxsw mm0,mm5
		paddusw mm2,thresh6w
		paddusw mm0,thresh6w
		pcmpeqw mm2,mm7
		pcmpeqw mm0,mm7
		sub eax,edi
		psrlw mm2,8
		movq mm6,threshb
		psrlw mm0,8
		packuswb mm2,mm0
		pand mm3,mm2
		movq [ebx+ecx],mm3
		add ecx,8
		cmp ecx,edx
		jl xloop
		add eax,edi
		add ebx,dst_pitch
		dec height
		jnz yloop
end:
		emms
	}
}

void ShowCombedTIVTC::check_combing_MMX(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w)
{
	__asm
	{
		mov eax,srcp
		mov ebx,dstp
		mov edx,width
		mov edi,src_pitch
		add eax,edi
		movq mm6,threshb
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]	// next
		sub eax,edi
		movq mm4,[eax+ecx]	// srcp
		movq mm5,mm0		// cpy next
		sub eax,edi
		movq mm1,[eax+ecx]	// prev
		movq mm2,mm4		// cpy srcp
		movq mm3,mm1		// cpy prev
		psubusb mm5,mm2		// next-srcp
		psubusb mm3,mm4		// prev-srcp
		psubusb mm2,mm0		// srcp-next
		psubusb mm4,mm1		// srcp-prev
		//pminub mm3,mm5
		//pminub mm2,mm4
		movq mm0,mm3
		movq mm1,mm2
		psubusb mm0,mm5
		psubusb mm1,mm4
		pcmpeqb mm0,mm7
		pcmpeqb mm1,mm7
		pand mm3,mm0
		pand mm2,mm1
		pxor mm0,ffMask
		pxor mm1,ffMask
		pand mm5,mm0
		pand mm4,mm1
		por mm3,mm5			// min(mm3,mm5)
		por mm2,mm4			// min(mm2,mm4)
		//pmaxub mm2,mm3
		movq mm0,mm2
		psubusb mm0,mm3
		pcmpeqb mm0,mm7
		pand mm3,mm0
		pxor mm0,ffMask
		pand mm2,mm0
		por mm2,mm3			// max(mm2,mm3)
		paddusb mm2,mm6
		pcmpeqb mm2,ffMask
		movq mm3,mm2
		psrlq mm2,32
		movd edi,mm3
		movd esi,mm2
		or edi,esi
		jnz output2
		mov edi,src_pitch
		add ecx,8
		lea eax,[eax+edi*2]
		cmp ecx,edx
		jl xloop
		add eax,edi
		add ebx,dst_pitch
		dec height
		jnz yloop
		jmp end
output2:
		movq mm1,[eax+ecx]	// prev
		mov esi,src_pitch2
		mov edi,src_pitch
		add eax,esi
		movq mm0,[eax+ecx]	// next
		movq mm2,mm0
		movq mm4,mm1
		sub eax,esi
		punpcklbw mm2,mm7
		punpckhbw mm0,mm7
		sub eax,edi
		punpcklbw mm4,mm7
		punpckhbw mm1,mm7
		paddusw mm2,mm4
		paddusw mm0,mm1
		pmullw mm2,threeMask	// 3*(p+n)
		pmullw mm0,threeMask	// 3*(p+n)
		movq mm1,[eax+ecx]
		movq mm4,mm1
		punpcklbw mm4,mm7
		add eax,esi
		punpckhbw mm1,mm7
		movq mm5,[eax+ecx]
		movq mm6,mm5
		punpcklbw mm6,mm7
		punpckhbw mm5,mm7
		psllw mm6,2
		add eax,esi
		psllw mm5,2
		paddusw mm4,mm6
		paddusw mm1,mm5
		movq mm5,[eax+ecx]
		movq mm6,mm5
		punpcklbw mm6,mm7
		punpckhbw mm5,mm7
		paddusw mm4,mm6			// (pp+c*4+nn)
		paddusw mm1,mm5			// (pp+c*4+nn)
		movq mm6,mm4
		movq mm5,mm1
		psubusw mm6,mm2
		psubusw mm5,mm0
		psubusw mm2,mm4
		psubusw mm0,mm1
		//pmaxsw mm2,mm6
		//pmaxsw mm0,mm5
		movq mm1,mm2
		movq mm4,mm0
		pcmpgtw mm1,mm6
		pcmpgtw mm4,mm5
		pand mm2,mm1
		pand mm0,mm4
		pxor mm1,ffMask
		pxor mm4,ffMask
		pand mm6,mm1
		pand mm5,mm4
		por mm2,mm6				// max(mm2,mm6)
		por mm0,mm5				// max(mm0,mm5)
		paddusw mm2,thresh6w
		paddusw mm0,thresh6w
		pcmpeqw mm2,ffMask
		pcmpeqw mm0,ffMask
		sub eax,edi
		psrlw mm2,8
		movq mm6,threshb
		psrlw mm0,8
		packuswb mm2,mm0
		pand mm3,mm2
		movq [ebx+ecx],mm3
		add ecx,8
		cmp ecx,edx
		jl xloop
		add eax,edi
		add ebx,dst_pitch
		dec height
		jnz yloop
end:
		emms
	}
}

void ShowCombedTIVTC::check_combing_SSE2_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __m128 threshb, __m128 thresh6w)
{
	__asm
	{
		mov eax,srcp
		mov edx,dstp
		mov edi,src_pitch
		add eax,edi
		movdqa xmm6,threshb
		pcmpeqb xmm7,xmm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]	// next
		sub eax,edi
		movdqa xmm4,[eax+ecx]	// srcp
		movdqa xmm5,xmm0		// cpy next
		sub eax,edi
		movdqa xmm1,[eax+ecx]	// prev
		movdqa xmm2,xmm4		// cpy srcp
		movdqa xmm3,xmm1		// cpy prev
		psubusb xmm5,xmm2		// next-srcp
		psubusb xmm3,xmm4		// prev-srcp
		psubusb xmm2,xmm0		// srcp-next
		psubusb xmm4,xmm1		// srcp-prev 
		pminub xmm3,xmm5
		pminub xmm2,xmm4
		pmaxub xmm2,xmm3
		paddusb xmm2,xmm6
		pcmpeqb xmm2,xmm7
		pand xmm2,lumaMask
		movdqa xmm3,xmm2
		psrldq xmm2,4
		movd edi,xmm3
		movd esi,xmm2
		or edi,esi
		jnz output2
		movdqa xmm4,xmm2
		psrldq xmm2,4
		psrldq xmm4,8
		movd edi,xmm2
		movd esi,xmm4
		or edi,esi
		jnz output2
		mov edi,src_pitch
		add ecx,16
		lea eax,[eax+edi*2]
		cmp ecx,width
		jl xloop
		add eax,edi
		add edx,dst_pitch
		dec height
		jnz yloop
		jmp end
output2:
		mov esi,src_pitch2
		mov edi,src_pitch
		pxor xmm7,mm7
		movdqa xmm2,xmm0
		movdqa xmm4,xmm1
		punpcklbw xmm2,xmm7
		punpckhbw xmm0,xmm7
		sub eax,edi
		punpcklbw xmm4,xmm7
		punpckhbw xmm1,xmm7
		paddusw xmm2,xmm4
		paddusw xmm0,xmm1
		pmullw xmm2,threeMask	// 3*(p+n)
		pmullw xmm0,threeMask	// 3*(p+n)
		movdqa xmm1,[eax+ecx]
		movdqa xmm4,xmm1
		punpcklbw xmm4,xmm7
		add eax,esi
		punpckhbw xmm1,xmm7
		movdqa xmm5,[eax+ecx]
		movdqa xmm6,xmm5
		punpcklbw xmm6,xmm7
		punpckhbw xmm5,xmm7
		psllw xmm6,2
		add eax,esi
		psllw xmm5,2
		paddusw xmm4,xmm6
		paddusw xmm1,xmm5
		movdqa xmm5,[eax+ecx]
		movdqa xmm6,xmm5
		punpcklbw xmm6,xmm7
		punpckhbw xmm5,xmm7
		paddusw xmm4,xmm6			// (pp+c*4+nn)
		paddusw xmm1,xmm5			// (pp+c*4+nn)
		movdqa xmm6,xmm4
		movdqa xmm5,xmm1
		psubusw xmm6,xmm2
		psubusw xmm5,xmm0
		psubusw xmm2,xmm4
		psubusw xmm0,xmm1
		pcmpeqb xmm7,xmm7
		pmaxsw xmm2,xmm6
		pmaxsw xmm0,xmm5
		paddusw xmm2,thresh6w
		paddusw xmm0,thresh6w
		pcmpeqw xmm2,xmm7
		pcmpeqw xmm0,xmm7
		sub eax,edi
		psrlw xmm2,8
		movdqa xmm6,threshb
		psrlw xmm0,8
		packuswb xmm2,xmm0
		pand xmm3,xmm2
		movdqa [edx+ecx],xmm3
		add ecx,16
		cmp ecx,width
		jl xloop
		add eax,edi
		add edx,dst_pitch
		dec height
		jnz yloop
end:
	}
}

void ShowCombedTIVTC::check_combing_iSSE_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w)
{
	__asm
	{
		mov eax,srcp
		mov ebx,dstp
		mov edx,width
		mov edi,src_pitch
		add eax,edi
		movq mm6,threshb
		pcmpeqb mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]	// next
		sub eax,edi
		movq mm4,[eax+ecx]	// srcp
		movq mm5,mm0		// cpy next
		sub eax,edi
		movq mm1,[eax+ecx]	// prev
		movq mm2,mm4		// cpy srcp
		movq mm3,mm1		// cpy prev
		psubusb mm5,mm2		// next-srcp
		psubusb mm3,mm4		// prev-srcp
		psubusb mm2,mm0		// srcp-next
		psubusb mm4,mm1		// srcp-prev 
		pminub mm3,mm5
		pminub mm2,mm4
		pmaxub mm2,mm3
		paddusb mm2,mm6
		pcmpeqb mm2,mm7
		pand mm2,lumaMask
		movq mm3,mm2
		psrlq mm2,32
		movd edi,mm3
		movd esi,mm2
		or edi,esi
		jnz output2
		mov edi,src_pitch
		add ecx,8
		lea eax,[eax+edi*2]
		cmp ecx,edx
		jl xloop
		add eax,edi
		add ebx,dst_pitch
		dec height
		jnz yloop
		jmp end
output2:
		mov esi,src_pitch2
		mov edi,src_pitch
		pxor mm7,mm7
		movq mm2,mm0
		movq mm4,mm1
		punpcklbw mm2,mm7
		punpckhbw mm0,mm7
		sub eax,edi
		punpcklbw mm4,mm7
		punpckhbw mm1,mm7
		paddusw mm2,mm4
		paddusw mm0,mm1
		pmullw mm2,threeMask	// 3*(p+n)
		pmullw mm0,threeMask	// 3*(p+n)
		movq mm1,[eax+ecx]
		movq mm4,mm1
		punpcklbw mm4,mm7
		add eax,esi
		punpckhbw mm1,mm7
		movq mm5,[eax+ecx]
		movq mm6,mm5
		punpcklbw mm6,mm7
		punpckhbw mm5,mm7
		psllw mm6,2
		add eax,esi
		psllw mm5,2
		paddusw mm4,mm6
		paddusw mm1,mm5
		movq mm5,[eax+ecx]
		movq mm6,mm5
		punpcklbw mm6,mm7
		punpckhbw mm5,mm7
		paddusw mm4,mm6			// (pp+c*4+nn)
		paddusw mm1,mm5			// (pp+c*4+nn)
		movq mm6,mm4
		movq mm5,mm1
		psubusw mm6,mm2
		psubusw mm5,mm0
		psubusw mm2,mm4
		psubusw mm0,mm1
		pcmpeqb mm7,mm7
		pmaxsw mm2,mm6
		pmaxsw mm0,mm5
		paddusw mm2,thresh6w
		paddusw mm0,thresh6w
		pcmpeqw mm2,mm7
		pcmpeqw mm0,mm7
		sub eax,edi
		psrlw mm2,8
		movq mm6,threshb
		psrlw mm0,8
		packuswb mm2,mm0
		pand mm3,mm2
		movq [ebx+ecx],mm3
		add ecx,8
		cmp ecx,edx
		jl xloop
		add eax,edi
		add ebx,dst_pitch
		dec height
		jnz yloop
end:
		emms
	}
}

void ShowCombedTIVTC::check_combing_MMX_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
		int height, int src_pitch, int src_pitch2, int dst_pitch, __int64 threshb, __int64 thresh6w)
{
	__asm
	{
		mov eax,srcp
		mov ebx,dstp
		mov edx,width
		mov edi,src_pitch
		add eax,edi
		movq mm6,threshb
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]	// next
		sub eax,edi
		movq mm4,[eax+ecx]	// srcp
		movq mm5,mm0		// cpy next
		sub eax,edi
		movq mm1,[eax+ecx]	// prev
		movq mm2,mm4		// cpy srcp
		movq mm3,mm1		// cpy prev
		psubusb mm5,mm2		// next-srcp
		psubusb mm3,mm4		// prev-srcp
		psubusb mm2,mm0		// srcp-next
		psubusb mm4,mm1		// srcp-prev
		//pminub mm3,mm5
		//pminub mm2,mm4
		movq mm0,mm3
		movq mm1,mm2
		psubusb mm0,mm5
		psubusb mm1,mm4
		pcmpeqb mm0,mm7
		pcmpeqb mm1,mm7
		pand mm3,mm0
		pand mm2,mm1
		pxor mm0,ffMask
		pxor mm1,ffMask
		pand mm5,mm0
		pand mm4,mm1
		por mm3,mm5			// min(mm3,mm5)
		por mm2,mm4			// min(mm2,mm4)
		//pmaxub mm2,mm3
		movq mm0,mm2
		psubusb mm0,mm3
		pcmpeqb mm0,mm7
		pand mm3,mm0
		pxor mm0,ffMask
		pand mm2,mm0
		por mm2,mm3			// max(mm2,mm3)
		paddusb mm2,mm6
		pcmpeqb mm2,ffMask
		pand mm2,lumaMask
		movq mm3,mm2
		psrlq mm2,32
		movd edi,mm3
		movd esi,mm2
		or edi,esi
		jnz output2
		mov edi,src_pitch
		add ecx,8
		lea eax,[eax+edi*2]
		cmp ecx,edx
		jl xloop
		add eax,edi
		add ebx,dst_pitch
		dec height
		jnz yloop
		jmp end
output2:
		movq mm1,[eax+ecx]	// prev
		mov esi,src_pitch2
		mov edi,src_pitch
		add eax,esi
		movq mm0,[eax+ecx]	// next
		movq mm2,mm0
		movq mm4,mm1
		sub eax,esi
		punpcklbw mm2,mm7
		punpckhbw mm0,mm7
		sub eax,edi
		punpcklbw mm4,mm7
		punpckhbw mm1,mm7
		paddusw mm2,mm4
		paddusw mm0,mm1
		pmullw mm2,threeMask	// 3*(p+n)
		pmullw mm0,threeMask	// 3*(p+n)
		movq mm1,[eax+ecx]
		movq mm4,mm1
		punpcklbw mm4,mm7
		add eax,esi
		punpckhbw mm1,mm7
		movq mm5,[eax+ecx]
		movq mm6,mm5
		punpcklbw mm6,mm7
		punpckhbw mm5,mm7
		psllw mm6,2
		add eax,esi
		psllw mm5,2
		paddusw mm4,mm6
		paddusw mm1,mm5
		movq mm5,[eax+ecx]
		movq mm6,mm5
		punpcklbw mm6,mm7
		punpckhbw mm5,mm7
		paddusw mm4,mm6			// (pp+c*4+nn)
		paddusw mm1,mm5			// (pp+c*4+nn)
		movq mm6,mm4
		movq mm5,mm1
		psubusw mm6,mm2
		psubusw mm5,mm0
		psubusw mm2,mm4
		psubusw mm0,mm1
		//pmaxsw mm2,mm6
		//pmaxsw mm0,mm5
		movq mm1,mm2
		movq mm4,mm0
		pcmpgtw mm1,mm6
		pcmpgtw mm4,mm5
		pand mm2,mm1
		pand mm0,mm4
		pxor mm1,ffMask
		pxor mm4,ffMask
		pand mm6,mm1
		pand mm5,mm4
		por mm2,mm6				// max(mm2,mm6)
		por mm0,mm5				// max(mm0,mm5)
		paddusw mm2,thresh6w
		paddusw mm0,thresh6w
		pcmpeqw mm2,ffMask
		pcmpeqw mm0,ffMask
		sub eax,edi
		psrlw mm2,8
		movq mm6,threshb
		psrlw mm0,8
		packuswb mm2,mm0
		pand mm3,mm2
		movq [ebx+ecx],mm3
		add ecx,8
		cmp ecx,edx
		jl xloop
		add eax,edi
		add ebx,dst_pitch
		dec height
		jnz yloop
end:
		emms
	}
}

void ShowCombedTIVTC::check_combing_MMX_M1(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int dst_pitch, __int64 thresh)
{
	__asm
	{
		mov ebx, srcp
		mov edi, dstp
		mov eax, ebx
		mov esi, ebx
		sub eax, src_pitch
		add esi, src_pitch
		mov edx, width
		movq mm6, thresh
		pxor mm7, mm7
yloop:
		xor ecx, ecx
		align 16
xloop:
		movq mm0, [eax+ecx]
		movq mm1, [ebx+ecx]
		movq mm2, [esi+ecx]
		movq mm3, mm0
		movq mm4, mm1
		movq mm5, mm2
		punpcklbw mm0, mm7
		punpcklbw mm1, mm7
		punpcklbw mm2, mm7
		punpckhbw mm3, mm7
		punpckhbw mm4, mm7
		punpckhbw mm5, mm7
		psubsw mm0, mm1
		psubsw mm2, mm1
		psubsw mm3, mm4
		psubsw mm5, mm4
		movq mm1, mm0
		movq mm4, mm2
		punpcklwd mm0, mm7
		punpckhwd mm1, mm7
 		punpcklwd mm2, mm7
		punpckhwd mm4, mm7
		pmaddwd mm0, mm2
		pmaddwd mm1, mm4
		movq mm2, mm3
		movq mm4, mm5
		punpcklwd mm2, mm7
		punpckhwd mm3, mm7
 		punpcklwd mm4, mm7
		punpckhwd mm5, mm7
		pmaddwd mm2, mm4
		pmaddwd mm3, mm5
		pcmpgtd mm0, mm6
		pcmpgtd mm1, mm6
		pcmpgtd mm2, mm6
		pcmpgtd mm3, mm6
		packssdw mm0, mm1
		packssdw mm2, mm3
		pand mm0,lumaMask
		pand mm2,lumaMask
		packuswb mm0, mm2
		movq [edi+ecx], mm0
		add ecx, 8
		cmp ecx, edx
		jl xloop
		add eax, src_pitch
		add ebx, src_pitch
		add esi, src_pitch
		add edi, dst_pitch
		dec height
		jnz yloop
		emms
	}
}

void ShowCombedTIVTC::check_combing_SSE2_M1(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int dst_pitch, __m128 thresh)
{
	__asm
	{
		movdqa xmm6, thresh
		mov edx, srcp
		mov edi, dstp
		mov eax, edx
		mov esi, edx
		sub eax, src_pitch
		add esi, src_pitch
		pxor xmm7, xmm7
yloop:
		xor ecx, ecx
		align 16
xloop:
		movdqa xmm0, [eax+ecx]
		movdqa xmm1, [edx+ecx]
		movdqa xmm2, [esi+ecx]
		movdqa xmm3, xmm0
		movdqa xmm4, xmm1
		movdqa xmm5, xmm2
		punpcklbw xmm0, xmm7
		punpcklbw xmm1, xmm7
		punpcklbw xmm2, xmm7
		punpckhbw xmm3, xmm7
		punpckhbw xmm4, xmm7
		punpckhbw xmm5, xmm7
		psubsw xmm0, xmm1
		psubsw xmm2, xmm1
		psubsw xmm3, xmm4
		psubsw xmm5, xmm4
		movdqa xmm1, xmm0
		movdqa xmm4, xmm2
		punpcklwd xmm0, xmm7
		punpckhwd xmm1, xmm7
 		punpcklwd xmm2, xmm7
		punpckhwd xmm4, xmm7
		pmaddwd xmm0, xmm2
		pmaddwd xmm1, xmm4
		movdqa xmm2, xmm3
		movdqa xmm4, xmm5
		punpcklwd xmm2, xmm7
		punpckhwd xmm3, xmm7
 		punpcklwd xmm4, xmm7
		punpckhwd xmm5, xmm7
		pmaddwd xmm2, xmm4
		pmaddwd xmm3, xmm5
		pcmpgtd xmm0, xmm6
		pcmpgtd xmm1, xmm6
		pcmpgtd xmm2, xmm6
		pcmpgtd xmm3, xmm6
		packssdw xmm0, xmm1
		packssdw xmm2, xmm3
		pand xmm0,lumaMask
		pand xmm2,lumaMask
		packuswb xmm0, xmm2
		movdqa [edi+ecx], xmm0
		add ecx, 16
		cmp ecx, width
		jl xloop
		add eax, src_pitch
		add edx, src_pitch
		add esi, src_pitch
		add edi, dst_pitch
		dec height
		jnz yloop
	}
}

void ShowCombedTIVTC::check_combing_MMX_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int dst_pitch, __int64 thresh)
{
	__asm
	{
		mov ebx, srcp
		mov edi, dstp
		mov eax, ebx
		mov esi, ebx
		sub eax, src_pitch
		add esi, src_pitch
		mov edx, width
		movq mm5, lumaMask
		movq mm6, thresh
		pxor mm7, mm7
yloop:
		xor ecx, ecx
		align 16
xloop:
		movq mm0, [eax+ecx]
		movq mm1, [ebx+ecx]
		movq mm2, [esi+ecx]
		pand mm0, mm5
		pand mm1, mm5
		pand mm2, mm5
		psubsw mm0, mm1
		psubsw mm2, mm1
		movq mm1, mm0
		movq mm4, mm2
		punpcklwd mm0, mm7
		punpckhwd mm1, mm7
 		punpcklwd mm2, mm7
		punpckhwd mm4, mm7
		pmaddwd mm0, mm2
		pmaddwd mm1, mm4
		pcmpgtd mm0, mm6
		pcmpgtd mm1, mm6
		packssdw mm0, mm1
		pand mm0, mm5
		movq [edi+ecx], mm0
		add ecx, 8
		cmp ecx, edx
		jl xloop
		add eax, src_pitch
		add ebx, src_pitch
		add esi, src_pitch
		add edi, dst_pitch
		dec height
		jnz yloop
		emms
	}
}

void ShowCombedTIVTC::check_combing_SSE2_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
		int width, int height, int src_pitch, int dst_pitch, __m128 thresh)
{
	__asm
	{
		movdqa xmm6, thresh
		mov edx, srcp
		mov edi, dstp
		mov eax, edx
		mov esi, edx
		sub eax, src_pitch
		add esi, src_pitch
		movdqa xmm5, lumaMask
		pxor xmm7, xmm7
yloop:
		xor ecx, ecx
		align 16
xloop:
		movdqa xmm0, [eax+ecx]
		movdqa xmm1, [edx+ecx]
		movdqa xmm2, [esi+ecx]
		pand xmm0, xmm5
		pand xmm1, xmm5
		pand xmm2, xmm5
		psubsw xmm0, xmm1
		psubsw xmm2, xmm1
		movdqa xmm1, xmm0
		movdqa xmm4, xmm2
		punpcklwd xmm0, xmm7
		punpckhwd xmm1, xmm7
 		punpcklwd xmm2, xmm7
		punpckhwd xmm4, xmm7
		pmaddwd xmm0, xmm2
		pmaddwd xmm1, xmm4
		pcmpgtd xmm0, xmm6
		pcmpgtd xmm1, xmm6
		packssdw xmm0, xmm1
		pand xmm0, xmm5
		movdqa [edi+ecx], xmm0
		add ecx, 16
		cmp ecx, width
		jl xloop
		add eax, src_pitch
		add edx, src_pitch
		add esi, src_pitch
		add edi, dst_pitch
		dec height
		jnz yloop
	}
}