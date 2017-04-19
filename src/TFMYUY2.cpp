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

bool TFM::checkCombedYUY2(PVideoFrame &src, int n, IScriptEnvironment *env, int match,
						  int *blockN, int &xblocksi, int *mics, bool ddebug) 
{
	if (mics[match] != -20)
	{
		if (mics[match] > MI) 
		{
			if (debug && !ddebug)
			{
				sprintf(buf,"TFM:  frame %d  - match %c:  Detected As Combed  (ReCheck - not processed)! (%d > %d)\n", 
					n, MTC(match), mics[match], MI);
				OutputDebugString(buf);
			}
			return true;
		}
		if (debug && !ddebug)
		{
			sprintf(buf,"TFM:  frame %d  - match %c:  Detected As NOT Combed  (ReCheck - not processed)! (%d <= %d)\n", 
				n, MTC(match), mics[match], MI);
			OutputDebugString(buf);
		}
		return false;
	}
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
	xblocksi = xblocks4;
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
				else env->ThrowError("TFM:  simd error (0)!");
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
				else env->ThrowError("TFM:  simd error (1)!");
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
	int Heighta = (Height>>(yshift-1))<<(yshift-1);
	if (Heighta == Height) Heighta = Height-yhalf;
	const int Widtha = (Width>>(xshift-1))<<(xshift-1);
	const bool use_sse2_sum = (use_sse2 && xhalf == 16 && yhalf == 8 && !((int(cmkp)|cmk_pitch)&15)) ? true : false;
	const bool use_isse_sum = (use_isse && xhalf == 16 && yhalf == 8) ? true : false;
	const bool use_mmx_sum = (use_mmx && xhalf == 16 && yhalf == 8) ? true : false;
	for (int y=1; y<yhalf; ++y)
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
			}
		}
		cmkpp += cmk_pitch;
		cmkp += cmk_pitch;
		cmkpn += cmk_pitch;
	}
	for (int y=yhalf; y<Heighta; y+=yhalf)
	{
		const int temp1 = (y>>yshift)*xblocks4;
		const int temp2 = ((y+yhalf)>>yshift)*xblocks4;
		if (use_sse2_sum)
		{
			for (int x=0; x<Widtha; x+=xhalf)
			{
				int sum = 0;
				compute_sum_8x16_sse2_luma(cmkpp+x,cmk_pitch,sum);
				if (sum)
				{
					const int box1 = (x>>xshift)<<2;
					const int box2 = ((x+xhalf)>>xshift)<<2;
					cArray[temp1+box1+0] += sum;
					cArray[temp1+box2+1] += sum;
					cArray[temp2+box1+2] += sum;
					cArray[temp2+box2+3] += sum;
				}
			}
		}
		else if (use_isse_sum)
		{
			for (int x=0; x<Widtha; x+=xhalf)
			{
				int sum = 0;
				compute_sum_8x16_isse_luma(cmkpp+x,cmk_pitch,sum);
				if (sum)
				{
					const int box1 = (x>>xshift)<<2;
					const int box2 = ((x+xhalf)>>xshift)<<2;
					cArray[temp1+box1+0] += sum;
					cArray[temp1+box2+1] += sum;
					cArray[temp2+box1+2] += sum;
					cArray[temp2+box2+3] += sum;
				}
			}
			__asm emms;
		}
		else if (use_mmx_sum)
		{
			for (int x=0; x<Widtha; x+=xhalf)
			{
				int sum = 0;
				compute_sum_8x16_mmx_luma(cmkpp+x,cmk_pitch,sum);
				if (sum)
				{
					const int box1 = (x>>xshift)<<2;
					const int box2 = ((x+xhalf)>>xshift)<<2;
					cArray[temp1+box1+0] += sum;
					cArray[temp1+box2+1] += sum;
					cArray[temp2+box1+2] += sum;
					cArray[temp2+box2+3] += sum;
				}
			}
			__asm emms;
		}
		else
		{
			for (int x=0; x<Widtha; x+=xhalf)
			{
				const unsigned char *cmkppT = cmkpp;
				const unsigned char *cmkpT = cmkp;
				const unsigned char *cmkpnT = cmkpn;
				int sum = 0;
				for (int u=0; u<yhalf; ++u)
				{
					for (int v=0; v<xhalf; v+=2)
					{
						if (cmkppT[x+v] == 0xFF && cmkpT[x+v] == 0xFF && 
							cmkpnT[x+v] == 0xFF) ++sum;
					}
					cmkppT += cmk_pitch;
					cmkpT += cmk_pitch;
					cmkpnT += cmk_pitch;
				}
				if (sum)
				{
					const int box1 = (x>>xshift)<<2;
					const int box2 = ((x+xhalf)>>xshift)<<2;
					cArray[temp1+box1+0] += sum;
					cArray[temp1+box2+1] += sum;
					cArray[temp2+box1+2] += sum;
					cArray[temp2+box2+3] += sum;
				}
			}
		}
		for (int x=Widtha; x<Width; x+=2)
		{
			const unsigned char *cmkppT = cmkpp;
			const unsigned char *cmkpT = cmkp;
			const unsigned char *cmkpnT = cmkpn;
			int sum = 0;
			for (int u=0; u<yhalf; ++u)
			{
				if (cmkppT[x] == 0xFF && cmkpT[x] == 0xFF && 
					cmkpnT[x] == 0xFF) ++sum;
				cmkppT += cmk_pitch;
				cmkpT += cmk_pitch;
				cmkpnT += cmk_pitch;
			}
			if (sum)
			{
				const int box1 = (x>>xshift)<<2;
				const int box2 = ((x+xhalf)>>xshift)<<2;
				cArray[temp1+box1+0] += sum;
				cArray[temp1+box2+1] += sum;
				cArray[temp2+box1+2] += sum;
				cArray[temp2+box2+3] += sum;
			}
		}
		cmkpp += cmk_pitch*yhalf;
		cmkp += cmk_pitch*yhalf;
		cmkpn += cmk_pitch*yhalf;
	}
	for (int y=Heighta; y<Height-1; ++y)
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
			}
		}
		cmkpp += cmk_pitch;
		cmkp += cmk_pitch;
		cmkpn += cmk_pitch;
	}
	for (int x=0; x<arraysize; ++x)
	{
		if (cArray[x] > mics[match])
		{
			mics[match] = cArray[x];
			blockN[match] = x;
		}
	}
	if (mics[match] > MI)
	{
		if (debug && !ddebug)
		{
			sprintf(buf,"TFM:  frame %d  - match %c:  Detected As Combed! (%d > %d)\n", 
				n, MTC(match), mics[match], MI);
			OutputDebugString(buf);
		}
		return true;
	}
	if (debug && !ddebug)
	{
		sprintf(buf,"TFM:  frame %d  - match %c:  Detected As NOT Combed! (%d <= %d)\n", 
			n, MTC(match), mics[match], MI);
		OutputDebugString(buf);
	}
	return false;
}

void TFM::buildDiffMapPlaneYUY2(const unsigned char *prvp, const unsigned char *nxtp, 
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height, 
		int Width, int tpitch, IScriptEnvironment *env)
{
	buildABSDiffMask(prvp-prv_pitch, nxtp-nxt_pitch, prv_pitch, nxt_pitch, 
		tpitch, Width, Height>>1, env);
	const unsigned char *dppp = tbuffer-tpitch;
	const unsigned char *dpp = tbuffer;
	const unsigned char *dp = tbuffer+tpitch;
	const unsigned char *dpn = tbuffer+tpitch*2;
	const unsigned char *dpnn = tbuffer+tpitch*3;
	int y, count;
	bool upper, lower, upper2, lower2;
	if (mChroma)
	{
		__asm
		{
			mov y, 2
	yloopl:
			mov edi, Width
			mov eax, dpp
			mov ecx, dp
			mov edx, dpn
			mov ebx, 4
			mov esi, dstp
			sub edi, 4
	xloopl:
			cmp BYTE PTR [ecx+ebx], 3
			ja b1l
			inc ebx
			jmp chroma_sec
	b1l:
			cmp BYTE PTR [ecx+ebx-2], 3
			ja p1l
			cmp BYTE PTR [ecx+ebx+2], 3
			ja p1l
			cmp BYTE PTR [eax+ebx-2], 3
			ja p1l
			cmp BYTE PTR [eax+ebx], 3
			ja p1l
			cmp BYTE PTR [eax+ebx+2], 3
			ja p1l
			cmp BYTE PTR [edx+ebx-2], 3
			ja p1l
			cmp BYTE PTR [edx+ebx], 3
			ja p1l
			cmp BYTE PTR [edx+ebx+2], 3
			ja p1l
			inc ebx
			jmp chroma_sec
	p1l:
			inc BYTE PTR [esi+ebx]
			cmp BYTE PTR [ecx+ebx], 19
			ja b2l
			inc ebx
			jmp chroma_sec
	b2l:
			xor edi,edi
			cmp BYTE PTR [eax+ebx-2], 19
			mov lower, 0
			mov upper, 0
			jbe b3l
			inc edi
	b3l:
			cmp BYTE PTR [eax+ebx], 19
			jbe b4l
			inc edi
	b4l:
			cmp BYTE PTR [eax+ebx+2], 19
			jbe b5l
			inc edi
	b5l:
			or edi,edi
			jz p2l
			mov upper, 1
	p2l:
			cmp BYTE PTR [ecx+ebx-2], 19
			jbe b6l
			inc edi
	b6l:
			cmp BYTE PTR [ecx+ebx+2], 19
			jbe b7l
			inc edi
	b7l:
			mov esi, edi
			cmp BYTE PTR [edx+ebx-2], 19
			jbe b8l
			inc edi
	b8l:
			cmp BYTE PTR [edx+ebx], 19
			jbe b9l
			inc edi
	b9l:
			cmp BYTE PTR [edx+ebx+2], 19
			jbe b10l
			inc edi
	b10l:
			cmp edi, 2
			jg c1l
	c2l:
			mov edi, Width
			inc ebx
			mov esi, dstp
			sub edi, 4
			jmp chroma_sec
	c1l:
			cmp edi, esi
			mov count, edi
			je b11l
			mov lower, 1
			cmp upper, 0
			je b11l
			mov esi, dstp
			add BYTE PTR [esi+ebx], 2
			jmp c2l
	b11l:
			mov eax, ebx
			add eax, -8
			jge p3l
			xor eax, eax
	p3l:
			mov edx, ebx
			mov ecx, Width
			mov lower2, 0
			add edx, 10
			mov upper2, 0
			cmp edx, ecx
			jle p4l
			mov edx, ecx
	p4l:
			cmp y, 2
			je p5l
			mov esi,eax
			mov ecx,dppp
	p6l:
			cmp BYTE PTR [ecx+esi], 19
			ja p7l
			add esi, 2
			cmp esi, edx
			jl p6l
			jmp p5l
	p7l:
			mov upper2, 1
	p5l:
			mov esi, eax
			mov ecx, dpp
			mov edi, dpn
	p11l:
			cmp BYTE PTR [ecx+esi], 19
			jbe p8l
			mov upper, 1
	p8l:
			cmp BYTE PTR [edi+esi], 19
			jbe p9l
			mov lower, 1
	p9l:
			cmp upper, 0
			je p10l
			cmp lower, 0
			jne p12l
	p10l:
			add esi, 2
			cmp esi, edx
			jl p11l
	p12l:
			mov esi, Height
			add esi, -4
			cmp y, esi
			je p13l
			mov esi, eax
			mov ecx, dpnn
	p14l:
			cmp BYTE PTR [ecx+esi], 19
			ja p15l
			add esi, 2
			cmp esi, edx
			jl p14l
			jmp p13l
	p15l:
			mov lower2, 1
	p13l:
			cmp upper, 0
			jne p16l
			cmp lower, 0
			je p17l
			cmp lower2, 0
			je p17l
			jmp p18l
	p16l:
			cmp lower, 0
			jne p18l
			cmp upper2, 0
			jne p18l
			jmp p17l
	p18l:
			mov esi, dstp
			add BYTE PTR [esi+ebx], 2
			jmp end_xloopl
	p17l:
			cmp count, 4
			jle end_xloopl
			mov esi, dstp
			add BYTE PTR [esi+ebx], 4
	end_xloopl:
			mov edi, Width
			inc ebx
			sub edi, 4
			mov eax, dpp
			mov ecx, dp
			mov edx, dpn
			mov esi, dstp
	chroma_sec:
			cmp BYTE PTR [ecx+ebx], 3
			ja b1c
			inc ebx
			cmp ebx, edi
			jl xloopl
			jmp end_yloopl
	b1c:
			cmp BYTE PTR [ecx+ebx-4], 3
			ja p1c
			cmp BYTE PTR [ecx+ebx+4], 3
			ja p1c
			cmp BYTE PTR [eax+ebx-4], 3
			ja p1c
			cmp BYTE PTR [eax+ebx], 3
			ja p1c
			cmp BYTE PTR [eax+ebx+4], 3
			ja p1c
			cmp BYTE PTR [edx+ebx-4], 3
			ja p1c
			cmp BYTE PTR [edx+ebx], 3
			ja p1c
			cmp BYTE PTR [edx+ebx+4], 3
			ja p1c
			inc ebx
			cmp ebx, edi
			jl xloopl
			jmp end_yloopl
	p1c:
			inc BYTE PTR [esi+ebx]
			cmp BYTE PTR [ecx+ebx], 19
			ja b2c
			inc ebx
			cmp ebx, edi
			jl xloopl
			jmp end_yloopl
	b2c:
			xor edi,edi
			cmp BYTE PTR [eax+ebx-4], 19
			mov lower, 0
			mov upper, 0
			jbe b3c
			inc edi
	b3c:
			cmp BYTE PTR [eax+ebx], 19
			jbe b4c
			inc edi
	b4c:
			cmp BYTE PTR [eax+ebx+4], 19
			jbe b5c
			inc edi
	b5c:
			or edi,edi
			jz p2c
			mov upper, 1
	p2c:
			cmp BYTE PTR [ecx+ebx-4], 19
			jbe b6c
			inc edi
	b6c:
			cmp BYTE PTR [ecx+ebx+4], 19
			jbe b7c
			inc edi
	b7c:
			mov esi, edi
			cmp BYTE PTR [edx+ebx-4], 19
			jbe b8c
			inc edi
	b8c:
			cmp BYTE PTR [edx+ebx], 19
			jbe b9c
			inc edi
	b9c:
			cmp BYTE PTR [edx+ebx+4], 19
			jbe b10c
			inc edi
	b10c:
			cmp edi, 2
			jg c1c
	c2c:
			mov edi, Width
			inc ebx
			sub edi, 4
			cmp ebx, edi
			jge end_yloopl
			mov esi, dstp
			jmp xloopl
	c1c:
			cmp edi, esi
			mov count, edi
			je b11c
			mov lower, 1
			cmp upper, 0
			je b11c
			mov esi, dstp
			add BYTE PTR [esi+ebx], 2
			jmp c2c
	b11c:
			mov eax, ebx
			add eax, -16
			mov edx, ebx
			and edx, 2
			inc edx
			cmp eax, edx
			jge p3c
			mov eax, edx
	p3c:
			mov edx, ebx
			mov ecx, Width
			mov lower2, 0
			add edx, 18
			mov upper2, 0
			cmp edx, ecx
			jle p4c
			mov edx, ecx
	p4c:
			cmp y, 2
			je p5c
			mov esi,eax
			mov ecx,dppp
	p6c:
			cmp BYTE PTR [ecx+esi], 19
			ja p7c
			add esi, 4
			cmp esi, edx
			jl p6c
			jmp p5c
	p7c:
			mov upper2, 1
	p5c:
			mov esi, eax
			mov ecx, dpp
			mov edi, dpn
	p11c:
			cmp BYTE PTR [ecx+esi], 19
			jbe p8c
			mov upper, 1
	p8c:
			cmp BYTE PTR [edi+esi], 19
			jbe p9c
			mov lower, 1
	p9c:
			cmp upper, 0
			je p10c
			cmp lower, 0
			jne p12c
	p10c:
			add esi, 4
			cmp esi, edx
			jl p11c
	p12c:
			mov esi, Height
			add esi, -4
			cmp y, esi
			je p13c
			mov esi, eax
			mov ecx, dpnn
	p14c:
			cmp BYTE PTR [ecx+esi], 19
			ja p15c
			add esi, 4
			cmp esi, edx
			jl p14c
			jmp p13c
	p15c:
			mov lower2, 1
	p13c:
			cmp upper, 0
			jne p16c
			cmp lower, 0
			je p17c
			cmp lower2, 0
			je p17c
			jmp p18c
	p16c:
			cmp lower, 0
			jne p18c
			cmp upper2, 0
			jne p18c
			jmp p17c
	p18c:
			mov esi, dstp
			add BYTE PTR [esi+ebx], 2
			jmp end_xloopc
	p17c:
			cmp count, 4
			jle end_xloopc
			mov esi, dstp
			add BYTE PTR [esi+ebx], 4
	end_xloopc:
			mov edi, Width
			inc ebx
			sub edi, 4
			cmp ebx, edi
			jge end_yloopl
			mov eax, dpp
			mov ecx, dp
			mov edx, dpn
			mov esi, dstp
			jmp xloopl
	end_yloopl:
			mov edi, tpitch
			mov eax, dst_pitch
			mov ecx, Height
			add y, 2
			sub ecx, 2
			add dppp, edi
			add dpp, edi
			add dp, edi
			add dpn, edi
			add dpnn, edi
			add dstp, eax
			cmp y, ecx
			jl yloopl
		}
	}
	else
	{
		__asm
		{
			mov y, 2
	yloop:
			mov edi, Width
			mov eax, dpp
			mov ecx, dp
			mov edx, dpn
			mov ebx, 4
			mov esi, dstp
			sub edi, 4
	xloop:
			cmp BYTE PTR [ecx+ebx], 3
			ja b1
			add ebx, 2
			cmp ebx, edi
			jl xloop
			jmp end_yloop
	b1:
			cmp BYTE PTR [ecx+ebx-2], 3
			ja p1
			cmp BYTE PTR [ecx+ebx+2], 3
			ja p1
			cmp BYTE PTR [eax+ebx-2], 3
			ja p1
			cmp BYTE PTR [eax+ebx], 3
			ja p1
			cmp BYTE PTR [eax+ebx+2], 3
			ja p1
			cmp BYTE PTR [edx+ebx-2], 3
			ja p1
			cmp BYTE PTR [edx+ebx], 3
			ja p1
			cmp BYTE PTR [edx+ebx+2], 3
			ja p1
			add ebx, 2
			cmp ebx, edi
			jl xloop
			jmp end_yloop
	p1:
			inc BYTE PTR [esi+ebx]
			cmp BYTE PTR [ecx+ebx], 19
			ja b2
			add ebx, 2
			cmp ebx, edi
			jl xloop
			jmp end_yloop
	b2:
			xor edi,edi
			cmp BYTE PTR [eax+ebx-2], 19
			mov lower, 0
			mov upper, 0
			jbe b3
			inc edi
	b3:
			cmp BYTE PTR [eax+ebx], 19
			jbe b4
			inc edi
	b4:
			cmp BYTE PTR [eax+ebx+2], 19
			jbe b5
			inc edi
	b5:
			or edi,edi
			jz p2
			mov upper, 1
	p2:
			cmp BYTE PTR [ecx+ebx-2], 19
			jbe b6
			inc edi
	b6:
			cmp BYTE PTR [ecx+ebx+2], 19
			jbe b7
			inc edi
	b7:
			mov esi, edi
			cmp BYTE PTR [edx+ebx-2], 19
			jbe b8
			inc edi
	b8:
			cmp BYTE PTR [edx+ebx], 19
			jbe b9
			inc edi
	b9:
			cmp BYTE PTR [edx+ebx+2], 19
			jbe b10
			inc edi
	b10:
			cmp edi, 2
			jg c1
	c2:
			mov edi, Width
			add ebx, 2
			sub edi, 4
			cmp ebx, edi
			jge end_yloop
			mov esi, dstp
			jmp xloop
	c1:
			cmp edi, esi
			mov count, edi
			je b11
			mov lower, 1
			cmp upper, 0
			je b11
			mov esi, dstp
			add BYTE PTR [esi+ebx], 2
			jmp c2
	b11:
			mov eax, ebx
			add eax, -8
			jge p3
			xor eax, eax
	p3:
			mov edx, ebx
			mov ecx, Width
			mov lower2, 0
			add edx, 10
			mov upper2, 0
			cmp edx, ecx
			jle p4
			mov edx, ecx
	p4:
			cmp y, 2
			je p5
			mov esi,eax
			mov ecx,dppp
	p6:
			cmp BYTE PTR [ecx+esi], 19
			ja p7
			add esi, 2
			cmp esi, edx
			jl p6
			jmp p5
	p7:
			mov upper2, 1
	p5:
			mov esi, eax
			mov ecx, dpp
			mov edi, dpn
	p11:
			cmp BYTE PTR [ecx+esi], 19
			jbe p8
			mov upper, 1
	p8:
			cmp BYTE PTR [edi+esi], 19
			jbe p9
			mov lower, 1
	p9:
			cmp upper, 0
			je p10
			cmp lower, 0
			jne p12
	p10:
			add esi, 2
			cmp esi, edx
			jl p11
	p12:
			mov esi, Height
			add esi, -4
			cmp y, esi
			je p13
			mov esi, eax
			mov ecx, dpnn
	p14:
			cmp BYTE PTR [ecx+esi], 19
			ja p15
			add esi, 2
			cmp esi, edx
			jl p14
			jmp p13
	p15:
			mov lower2, 1
	p13:
			cmp upper, 0
			jne p16
			cmp lower, 0
			je p17
			cmp lower2, 0
			je p17
			jmp p18
	p16:
			cmp lower, 0
			jne p18
			cmp upper2, 0
			jne p18
			jmp p17
	p18:
			mov esi, dstp
			add BYTE PTR [esi+ebx], 2
			jmp end_xloop
	p17:
			cmp count, 4
			jle end_xloop
			mov esi, dstp
			add BYTE PTR [esi+ebx], 4
	end_xloop:
			mov edi, Width
			add ebx, 2
			sub edi, 4
			cmp ebx, edi
			jge end_yloop
			mov eax, dpp
			mov ecx, dp
			mov edx, dpn
			mov esi, dstp
			jmp xloop
	end_yloop:
			mov edi, tpitch
			mov eax, dst_pitch
			mov ecx, Height
			add y, 2
			sub ecx, 2
			add dppp, edi
			add dpp, edi
			add dp, edi
			add dpn, edi
			add dpnn, edi
			add dstp, eax
			cmp y, ecx
			jl yloop
		}
	}
}

void TFM::DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s) 
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

void TFM::drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks)
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
		(dstp+y*pitch)[cordx] = (dstp+y*pitch)[cordx] <= 128 ? 255 : 0;
		if (temp < width) (dstp+y*pitch)[temp] = (dstp+y*pitch)[temp] <= 128 ? 255 : 0;
	}
	for (x=max(cordx,0), temp=cordy+blocky-1; x<xlim; x+=4)
	{
		(dstp+cordy*pitch)[x] = (dstp+cordy*pitch)[x] <= 128 ? 255 : 0;
		(dstp+cordy*pitch)[x+1] = 128;
		(dstp+cordy*pitch)[x+2] = (dstp+cordy*pitch)[x+2] <= 128 ? 255 : 0;
		(dstp+cordy*pitch)[x+3] = 128;
		if (temp < height)
		{
			(dstp+temp*pitch)[x] = (dstp+temp*pitch)[x] <= 128 ? 255 : 0;
			(dstp+temp*pitch)[x+1] = 128;
			(dstp+temp*pitch)[x+2] = (dstp+temp*pitch)[x+2] <= 128 ? 255 : 0;
			(dstp+temp*pitch)[x+3] = 128;
		}
	}
}