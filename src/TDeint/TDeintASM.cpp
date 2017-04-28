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

#include "TDeinterlace.h"

void TDeinterlace::absDiff(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, int pos,
						   IScriptEnvironment *env)
{
	long cpu = env->GetCPUFlags();
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
	int stop = vi.IsYV12() ? 3 : 1;
	for (int b=0; b<stop; ++b)
	{
		const unsigned char *srcp1 = src1->GetReadPtr(plane[b]);
		const int src1_pitch = src1->GetPitch(plane[b]);
		const int height = src1->GetHeight(plane[b]);
		const int width = src1->GetRowSize(plane[b]);
		const unsigned char *srcp2 = src2->GetReadPtr(plane[b]);
		const int src2_pitch = src2->GetPitch(plane[b]);
		unsigned char *dstp = pos == -1 ? dst->GetWritePtr(plane[b]) : db->GetWritePtr(pos, b);
		const int dst_pitch = pos == -1 ? dst->GetPitch(plane[b]) : db->GetPitch(b);
		const int mthresh1 = b == 0 ? mthreshL : mthreshC;
		const int mthresh2 = stop == 3 ? (b == 0 ? mthreshL : mthreshC) : mthreshC;
		if ((cpu&CPUF_SSE2) && !((int(srcp1)|int(srcp2)|int(dstp)|src1_pitch|src2_pitch|dst_pitch)&15))
			absDiffSSE2(srcp1, srcp2, dstp, src1_pitch, src2_pitch, dst_pitch, width, height, mthresh1, mthresh2);
		else if (cpu&CPUF_MMX)
			absDiffMMX(srcp1, srcp2, dstp, src1_pitch, src2_pitch, dst_pitch, width, height, mthresh1, mthresh2);
		else
		{
			for (int y=0; y<height; ++y)
			{
				for (int x=0; x<width; ++x)
				{
					if (abs(srcp1[x]-srcp2[x]) < mthresh1) dstp[x] = 1;
					else dstp[x] = 0;
					++x;
					if (abs(srcp1[x]-srcp2[x]) < mthresh2) dstp[x] = 1;
					else dstp[x] = 0;
				}
				srcp1 += src1_pitch;
				srcp2 += src2_pitch;
				dstp += dst_pitch;
			}
		}
	}
}

__declspec(align(16)) const __int64 onesMask[2] = { 0x0101010101010101, 0x0101010101010101 };
__declspec(align(16)) const __int64 twosMask[2] = { 0x0202020202020202, 0x0202020202020202 };
__declspec(align(16)) const __int64 mask251[2] = { 0xFBFBFBFBFBFBFBFB, 0xFBFBFBFBFBFBFBFB };
__declspec(align(16)) const __int64 mask235[2] = { 0xEBEBEBEBEBEBEBEB, 0xEBEBEBEBEBEBEBEB };
__declspec(align(16)) const __int64 onesMaskLuma[2] = { 0x0001000100010001, 0x0001000100010001 };
__declspec(align(16)) const __int64 threeMask[2] = { 0x0003000300030003, 0x0003000300030003 };
__declspec(align(16)) const __int64 ffMask[2] = { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF };
__declspec(align(16)) const __int64 lumaMask[2] = { 0x00FF00FF00FF00FF, 0x00FF00FF00FF00FF };

void TDeinterlace::absDiffSSE2(const unsigned char *srcp1, const unsigned char *srcp2,
	   unsigned char *dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width, 
	   int height, int mthresh1, int mthresh2)
{
	mthresh1 = min(max(255-mthresh1,0),255);
	mthresh2 = min(max(255-mthresh2,0),255);
	__int64 sthresh[2];
	sthresh[0] = (mthresh2<<8)+mthresh1;
	sthresh[0] += (sthresh[0]<<48)+(sthresh[0]<<32)+(sthresh[0]<<16);
	sthresh[1] = sthresh[0];
	__asm
	{
		mov edi,srcp1
		mov edx,srcp2
		mov esi,dstp
		mov ecx,width
		movdqu xmm5,sthresh
		pcmpeqb xmm6,xmm6
		movdqa xmm7,onesMask
yloop:
		xor eax,eax
		align 16
xloop:
		movdqa xmm0,[edi+eax]
		movdqa xmm1,[edx+eax]
		movdqa xmm2,xmm0
		psubusb xmm0,xmm1
		psubusb xmm1,xmm2
		por xmm0,xmm1
		paddusb xmm0,xmm5
		pcmpeqb xmm0,xmm6
		pxor xmm0,xmm6
		pand xmm0,xmm7
		movdqa [esi+eax],xmm0
		add eax,16
		cmp eax,ecx
		jl xloop
		add edi,src1_pitch
		add edx,src2_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
	}
}

void TDeinterlace::absDiffMMX(const unsigned char *srcp1, const unsigned char *srcp2,
	   unsigned char *dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width, 
	   int height, int mthresh1, int mthresh2)
{
	mthresh1 = min(max(255-mthresh1,0),255);
	mthresh2 = min(max(255-mthresh2,0),255);
	__int64 sthresh = (mthresh2<<8)+mthresh1;
	sthresh += (sthresh<<48)+(sthresh<<32)+(sthresh<<16);
	if (!(width&15))
	{
		__asm
		{
			mov edi,srcp1
			mov edx,srcp2
			mov esi,dstp
			mov ecx,width
			movq mm6,sthresh
			pcmpeqb mm7,mm7
	yloop16:
			xor eax,eax
			align 16
	xloop16:
			movq mm0,[edi+eax]
			movq mm1,[edi+eax+8]
			movq mm2,[edx+eax]
			movq mm3,[edx+eax+8]
			movq mm4,mm0
			movq mm5,mm1
			psubusb mm0,mm2
			psubusb mm1,mm3
			psubusb mm2,mm4
			psubusb mm3,mm5
			por mm0,mm2
			por mm1,mm3
			paddusb mm0,mm6
			paddusb mm1,mm6
			pcmpeqb mm0,mm7
			pcmpeqb mm1,mm7
			pxor mm0,mm7
			pxor mm1,mm7
			pand mm0,onesMask
			pand mm1,onesMask
			movq [esi+eax],mm0
			movq [esi+eax+8],mm1
			add eax,16
			cmp eax,ecx
			jl xloop16
			add edi,src1_pitch
			add edx,src2_pitch
			add esi,dst_pitch
			dec height
			jnz yloop16
			emms
		}
	}
	else
	{
		__asm
		{
			mov edi,srcp1
			mov edx,srcp2
			mov esi,dstp
			mov ecx,width
			movq mm5,sthresh
			pcmpeqb mm6,mm6
			movq mm7,onesMask
	yloop8:
			xor eax,eax
			align 16
	xloop8:
			movq mm0,[edi+eax]
			movq mm1,[edx+eax]
			movq mm2,mm0
			psubusb mm0,mm1
			psubusb mm1,mm2
			por mm0,mm1
			paddusb mm0,mm5
			pcmpeqb mm0,mm6
			pxor mm0,mm6
			pand mm0,mm7
			movq [esi+eax],mm0
			add eax,8
			cmp eax,ecx
			jl xloop8
			add edi,src1_pitch
			add edx,src2_pitch
			add esi,dst_pitch
			dec height
			jnz yloop8
			emms
		}
	}
}

void TDeinterlace::buildDiffMapPlane(const unsigned char *prvp, const unsigned char *nxtp, 
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height, 
		int Width, int optt, IScriptEnvironment *env)
{
	long cpu = env->GetCPUFlags();
	if (optt != 4)
	{
		if (optt == 0) cpu &= ~0x2C;
		else if (optt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (optt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (optt == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_SSE2) && !((int(prvp)|int(nxtp)|int(dstp)|prv_pitch|nxt_pitch|dst_pitch)&15))
	{
		buildABSDiffMask2_SSE2(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch,
			Width, Height);
	}
	else if (cpu&CPUF_MMX)
	{
		buildABSDiffMask2_MMX(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch,
			Width, Height);
	}
	else
	{
		for (int y=0; y<Height; ++y)
		{
			for (int x=0; x<Width; ++x)
			{
				const int diff = abs(prvp[x]-nxtp[x]);
				if (diff > 19) dstp[x] = 3;
				else if (diff > 3) dstp[x] = 1;
				else dstp[x] = 0;
			}
			prvp += prv_pitch;
			nxtp += nxt_pitch;
			dstp += dst_pitch;
		}
	}
}

void TDeinterlace::buildABSDiffMask2_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height)
{
	__asm
	{
		mov eax,prvp
		mov ebx,nxtp
		mov edx,dstp
		mov edi,width
		mov esi,height
		movdqa xmm3,onesMask
		movdqa xmm4,twosMask
		pcmpeqb xmm5,xmm5
		movdqa xmm6,mask251
		movdqa xmm7,mask235
yloop:
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]
		movdqa xmm1,[ebx+ecx]
		movdqa xmm2,xmm0
		psubusb xmm0,xmm1
		psubusb xmm1,xmm2
		por xmm0,xmm1
		movdqa xmm1,xmm0
		paddusb xmm0,xmm6
		paddusb xmm1,xmm7
		pcmpeqb xmm0,xmm5
		pcmpeqb xmm1,xmm5
		pand xmm0,xmm3
		pand xmm1,xmm4
		por xmm0,xmm1
		movdqa [edx+ecx],xmm0
		add ecx,16
		cmp ecx,edi
		jl xloop
		add eax,prv_pitch
		add ebx,nxt_pitch
		add edx,dst_pitch
		dec esi
		jnz yloop
	}
}

void TDeinterlace::buildABSDiffMask2_MMX(const unsigned char *prvp, const unsigned char *nxtp,
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width,
		int height)
{
	if (!(width&15))
	{
		__asm
		{
			mov eax,prvp
			mov ebx,nxtp
			mov edx,dstp
			mov edi,width
			mov esi,height
			movq mm6,mask251
			movq mm7,mask235
yloop:
			xor ecx,ecx
			align 16
xloop:
			movq mm0,[eax+ecx]
			movq mm1,[eax+ecx+8]
			movq mm2,[ebx+ecx]
			movq mm3,[ebx+ecx+8]
			movq mm4,mm0
			movq mm5,mm1
			psubusb mm4,mm2
			psubusb mm5,mm3
			psubusb mm2,mm0
			psubusb mm3,mm1
			por mm2,mm4
			por mm3,mm5
			pcmpeqb mm0,mm0
			pcmpeqb mm1,mm1
			movq mm4,mm2
			movq mm5,mm3
			paddusb mm4,mm6
			paddusb mm2,mm7
			paddusb mm5,mm6
			paddusb mm3,mm7
			pcmpeqb mm4,mm0
			pcmpeqb mm2,mm1
			pcmpeqb mm5,mm0
			pcmpeqb mm3,mm1
			pand mm4,onesMask
			pand mm5,onesMask
			pand mm2,twosMask
			pand mm3,twosMask
			por mm2,mm4
			por mm3,mm5
			movq [edx+ecx],mm2
			movq [edx+ecx+8],mm3
			add ecx,16
			cmp ecx,edi
			jl xloop
			add eax,prv_pitch
			add ebx,nxt_pitch
			add edx,dst_pitch
			dec esi
			jnz yloop
			emms
		}
	}
	else
	{
		width -= 8;
		__asm
		{
			mov eax,prvp
			mov ebx,nxtp
			mov edx,dstp
			mov edi,width
			mov esi,height
			movq mm6,mask251
			movq mm7,mask235
yloop2:
			xor ecx,ecx
			align 16
xloop2:
			movq mm0,[eax+ecx]
			movq mm1,[eax+ecx+8]
			movq mm2,[ebx+ecx]
			movq mm3,[ebx+ecx+8]
			movq mm4,mm0
			movq mm5,mm1
			psubusb mm4,mm2
			psubusb mm5,mm3
			psubusb mm2,mm0
			psubusb mm3,mm1
			por mm2,mm4
			por mm3,mm5
			pcmpeqb mm0,mm0
			pcmpeqb mm1,mm1
			movq mm4,mm2
			movq mm5,mm3
			paddusb mm4,mm6
			paddusb mm2,mm7
			paddusb mm5,mm6
			paddusb mm3,mm7
			pcmpeqb mm4,mm0
			pcmpeqb mm2,mm1
			pcmpeqb mm5,mm0
			pcmpeqb mm3,mm1
			pand mm4,onesMask
			pand mm5,onesMask
			pand mm2,twosMask
			pand mm3,twosMask
			por mm2,mm4
			por mm3,mm5
			movq [edx+ecx],mm2
			movq [edx+ecx+8],mm3
			add ecx,16
			cmp ecx,edi
			jl xloop2
			movq mm0,[eax+ecx]
			movq mm1,[ebx+ecx]
			movq mm2,mm0
			psubusb mm2,mm1
			psubusb mm1,mm0
			por mm1,mm2
			pcmpeqb mm0,mm0
			movq mm2,mm1
			paddusb mm1,mm6
			paddusb mm2,mm7
			pcmpeqb mm1,mm0
			pcmpeqb mm2,mm0
			pand mm1,onesMask
			pand mm2,twosMask
			por mm1,mm2
			movq [edx+ecx],mm1
			add eax,prv_pitch
			add ebx,nxt_pitch
			add edx,dst_pitch
			dec esi
			jnz yloop2
			emms
		}
	}
}

#pragma warning(push)
#pragma warning(disable:4799)	// disable no emms warning message

void TDeinterlace::compute_sum_8x8_mmx(const unsigned char *srcp, int pitch, int &sum)
{
	__asm
	{
		mov eax,srcp
		mov edi,pitch
		mov ecx,4
		movq mm0,[eax]
		movq mm1,[eax+edi]
		movq mm5,onesMask
		lea eax,[eax+edi*2]
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
loopy:
		movq mm2,[eax]
		movq mm3,[eax+edi]
		movq mm4,mm2
		pand mm0,mm1
		pand mm4,mm3
		pand mm0,mm2
		pand mm4,mm1
		pand mm0,mm5
		pand mm4,mm5
		paddusb mm7,mm0
		lea eax,[eax+edi*2]
		movq mm0,mm2
		movq mm1,mm3
		paddusb mm7,mm4
		dec ecx
		jnz loopy
		movq mm0,mm7
		mov eax,sum
		punpcklbw mm7,mm6
		punpckhbw mm0,mm6
		paddusw mm7,mm0
		movq mm0,mm7
		punpcklwd mm7,mm6
		punpckhwd mm0,mm6
		paddd mm7,mm0
		movq mm0,mm7
		psrlq mm7,32
		paddd mm0,mm7
		movd [eax],mm0
	}
}

void TDeinterlace::compute_sum_8x8_isse(const unsigned char *srcp, int pitch, int &sum)
{
	__asm
	{
		mov eax,srcp
		mov edi,pitch
		mov ecx,4
		movq mm0,[eax]
		movq mm1,[eax+edi]
		movq mm5,onesMask
		lea eax,[eax+edi*2]
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
loopy:
		movq mm2,[eax]
		movq mm3,[eax+edi]
		movq mm4,mm2
		pand mm0,mm1
		pand mm4,mm3
		pand mm0,mm2
		pand mm4,mm1
		pand mm0,mm5
		pand mm4,mm5
		paddusb mm7,mm0
		lea eax,[eax+edi*2]
		movq mm0,mm2
		movq mm1,mm3
		paddusb mm7,mm4
		dec ecx
		jnz loopy
		mov eax,sum
		psadbw mm7,mm6
		movd [eax],mm7
	}
}

void TDeinterlace::compute_sum_8x16_mmx_luma(const unsigned char *srcp, int pitch, int &sum)
{
	__asm
	{
		mov eax,srcp
		mov edi,pitch
		mov ecx,4
		xor edx,edx
		movq mm0,[eax]
		movq mm1,[eax+edi]
		movq mm5,onesMaskLuma
		lea eax,[eax+edi*2]
		pxor mm6,mm6
		pxor mm7,mm7
		jmp xskip
loopx:
		mov eax,srcp
		mov ecx,4
		add eax,8
		inc edx
		movq mm0,[eax]
		movq mm1,[eax+edi]
		lea eax,[eax+edi*2]
xskip:
		align 16
loopy:
		movq mm2,[eax]
		movq mm3,[eax+edi]
		movq mm4,mm2
		pand mm0,mm1
		pand mm4,mm3
		pand mm0,mm2
		pand mm4,mm1
		pand mm0,mm5
		pand mm4,mm5
		paddusb mm7,mm0
		lea eax,[eax+edi*2]
		movq mm0,mm2
		movq mm1,mm3
		paddusb mm7,mm4
		dec ecx
		jnz loopy
		or edx,edx
		jz loopx
		movq mm0,mm7
		mov eax,sum
		punpcklwd mm7,mm6
		punpckhwd mm0,mm6
		paddd mm7,mm0
		movq mm0,mm7
		psrlq mm7,32
		paddd mm0,mm7
		movd [eax],mm0
	}
}

void TDeinterlace::compute_sum_8x16_isse_luma(const unsigned char *srcp, int pitch, int &sum)
{
	__asm
	{
		mov eax,srcp
		mov edi,pitch
		mov ecx,4
		xor edx,edx
		movq mm0,[eax]
		movq mm1,[eax+edi]
		movq mm5,onesMaskLuma
		lea eax,[eax+edi*2]
		pxor mm6,mm6
		pxor mm7,mm7
		jmp xskip
loopx:
		mov eax,srcp
		add ecx,4
		add eax,8
		inc edx
		movq mm0,[eax]
		movq mm1,[eax+edi]
		lea eax,[eax+edi*2]
xskip:
		align 16
loopy:
		movq mm2,[eax]
		movq mm3,[eax+edi]
		movq mm4,mm2
		pand mm0,mm1
		pand mm4,mm3
		pand mm0,mm2
		pand mm4,mm1
		pand mm0,mm5
		pand mm4,mm5
		paddusb mm7,mm0
		lea eax,[eax+edi*2]
		movq mm0,mm2
		movq mm1,mm3
		paddusb mm7,mm4
		dec ecx
		jnz loopy
		or edx,edx
		jz loopx
		mov eax,sum
		psadbw mm7,mm6
		movd [eax],mm7
	}
}

void TDeinterlace::compute_sum_8x16_sse2_luma(const unsigned char *srcp, int pitch, int &sum)
{
	__asm
	{
		mov eax,srcp
		mov edi,pitch
		mov ecx,4
		movdqa xmm0,[eax]
		movdqa xmm1,[eax+edi]
		movdqa xmm5,onesMaskLuma
		lea eax,[eax+edi*2]
		pxor xmm6,xmm6
		pxor xmm7,xmm7
		align 16
loopy:
		movdqa xmm2,[eax]
		movdqa xmm3,[eax+edi]
		movdqa xmm4,xmm2
		pand xmm0,xmm1
		pand xmm4,xmm3
		pand xmm0,xmm2
		pand xmm4,xmm1
		pand xmm0,xmm5
		pand xmm4,xmm5
		paddusb xmm7,xmm0
		lea eax,[eax+edi*2]
		movdqa xmm0,xmm2
		movdqa xmm1,xmm3
		paddusb xmm7,xmm4
		dec ecx
		jnz loopy
		mov eax,sum
		psadbw xmm7,xmm6
		movdqa xmm4,xmm7
		psrldq xmm7,8
		paddq xmm4,xmm7
		movd [eax],xmm4
	}
}

#pragma warning(pop)	// reenable no emms warning

void TDeinterlace::check_combing_SSE2(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TDeinterlace::check_combing_iSSE(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TDeinterlace::check_combing_MMX(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TDeinterlace::check_combing_SSE2_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TDeinterlace::check_combing_iSSE_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TDeinterlace::check_combing_MMX_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TDeinterlace::check_combing_MMX_M1(const unsigned char *srcp, unsigned char *dstp, 
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

void TDeinterlace::check_combing_SSE2_M1(const unsigned char *srcp, unsigned char *dstp, 
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

void TDeinterlace::check_combing_MMX_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
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

void TDeinterlace::check_combing_SSE2_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
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

void TDeinterlace::buildABSDiffMask(const unsigned char *prvp, const unsigned char *nxtp, 
	int prv_pitch, int nxt_pitch, int tpitch, int width, int height, IScriptEnvironment *env)
{
	long cpu = env->GetCPUFlags();
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_SSE2) && !((int(prvp)|int(nxtp)|prv_pitch|nxt_pitch|tpitch)&15))
	{
		buildABSDiffMask_SSE2(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch,
			width, height);
	}
	else if (cpu&CPUF_MMX)
	{
		buildABSDiffMask_MMX(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch,
			width, height);
	}
	else
	{
		unsigned char *dstp = tbuffer;
		for (int y=0; y<height; ++y)
		{
			for (int x=0; x<width; x+=4)
			{
				dstp[x+0] = abs(prvp[x+0]-nxtp[x+0]);
				dstp[x+1] = abs(prvp[x+1]-nxtp[x+1]);
				dstp[x+2] = abs(prvp[x+2]-nxtp[x+2]);
				dstp[x+3] = abs(prvp[x+3]-nxtp[x+3]);
			}
			prvp += prv_pitch;
			nxtp += nxt_pitch;
			dstp += tpitch;
		}
	}
}

void TDeinterlace::buildABSDiffMask_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
	unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height)
{
	if (!(width&15))
	{
		__asm
		{
			mov eax,prvp
			mov ebx,nxtp
			mov edx,dstp
			mov edi,width
			mov esi,height
yloop:
			xor ecx,ecx
			align 16
xloop:
			movdqa xmm0,[eax+ecx]
			movdqa xmm1,[ebx+ecx]
			movdqa xmm2,xmm0
			psubusb xmm0,xmm1
			psubusb xmm1,xmm2
			por xmm0,xmm1
			movdqa [edx+ecx],xmm0
			add ecx,16
			cmp ecx,edi
			jl xloop
			add eax,prv_pitch
			add ebx,nxt_pitch
			add edx,dst_pitch
			dec esi
			jnz yloop
		}
	}
	else
	{
		width -= 8;
		__asm
		{
			mov eax,prvp
			mov ebx,nxtp
			mov edx,dstp
			mov edi,width
			mov esi,height
yloop2:
			xor ecx,ecx
			align 16
xloop2:
			movdqa xmm0,[eax+ecx]
			movdqa xmm1,[ebx+ecx]
			movdqa xmm2,xmm0
			psubusb xmm0,xmm1
			psubusb xmm1,xmm2
			por xmm0,xmm1
			movdqa [edx+ecx],xmm0
			add ecx,16
			cmp ecx,edi
			jl xloop2
			movq xmm0,qword ptr[eax+ecx]
			movq xmm1,qword ptr[ebx+ecx]
			movq xmm2,xmm0
			psubusb xmm0,xmm1
			psubusb xmm1,xmm2
			por xmm0,xmm1
			movq qword ptr[edx+ecx],xmm0
			add eax,prv_pitch
			add ebx,nxt_pitch
			add edx,dst_pitch
			dec esi
			jnz yloop2
		}
	}
}

void TDeinterlace::buildABSDiffMask_MMX(const unsigned char *prvp, const unsigned char *nxtp,
	unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height)
{
	if (!(width&15))
	{
		__asm
		{
			mov eax,prvp
			mov ebx,nxtp
			mov edx,dstp
			mov edi,width
			mov esi,height
yloop:
			xor ecx,ecx
			align 16
xloop:
			movq mm0,[eax+ecx]
			movq mm1,[eax+ecx+8]
			movq mm2,[ebx+ecx]
			movq mm3,[ebx+ecx+8]
			movq mm4,mm0
			movq mm5,mm1
			psubusb mm4,mm2
			psubusb mm5,mm3
			psubusb mm2,mm0
			psubusb mm3,mm1
			por mm2,mm4
			por mm3,mm5
			movq [edx+ecx],mm2
			movq [edx+ecx+8],mm3
			add ecx,16
			cmp ecx,edi
			jl xloop
			add eax,prv_pitch
			add ebx,nxt_pitch
			add edx,dst_pitch
			dec esi
			jnz yloop
			emms
		}
	}
	else
	{
		width -= 8;
		__asm
		{
			mov eax,prvp
			mov ebx,nxtp
			mov edx,dstp
			mov edi,width
			mov esi,height
yloop2:
			xor ecx,ecx
			align 16
xloop2:
			movq mm0,[eax+ecx]
			movq mm1,[eax+ecx+8]
			movq mm2,[ebx+ecx]
			movq mm3,[ebx+ecx+8]
			movq mm4,mm0
			movq mm5,mm1
			psubusb mm4,mm2
			psubusb mm5,mm3
			psubusb mm2,mm0
			psubusb mm3,mm1
			por mm2,mm4
			por mm3,mm5
			movq [edx+ecx],mm2
			movq [edx+ecx+8],mm3
			add ecx,16
			cmp ecx,edi
			jl xloop2
			movq mm0,[eax+ecx]
			movq mm1,[ebx+ecx]
			movq mm2,mm0
			psubusb mm2,mm1
			psubusb mm1,mm0
			por mm1,mm2
			movq [edx+ecx],mm1
			add eax,prv_pitch
			add ebx,nxt_pitch
			add edx,dst_pitch
			dec esi
			jnz yloop2
			emms
		}
	}
}

void TDeinterlace::buildDiffMapPlaneYV12(const unsigned char *prvp, const unsigned char *nxtp, 
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
	__asm
	{
		mov y, 2
yloop:
		mov edi, Width
		mov eax, dpp
		mov ecx, dp
		mov edx, dpn
		mov ebx, 1
		mov esi, dstp
		dec edi
xloop:
		cmp BYTE PTR [ecx+ebx], 3
		ja b1
		inc ebx
		cmp ebx, edi
		jl xloop
		jmp end_yloop
b1:
		cmp BYTE PTR [ecx+ebx-1], 3
		ja p1
		cmp BYTE PTR [ecx+ebx+1], 3
		ja p1
		cmp BYTE PTR [eax+ebx-1], 3
		ja p1
		cmp BYTE PTR [eax+ebx], 3
		ja p1
		cmp BYTE PTR [eax+ebx+1], 3
		ja p1
		cmp BYTE PTR [edx+ebx-1], 3
		ja p1
		cmp BYTE PTR [edx+ebx], 3
		ja p1
		cmp BYTE PTR [edx+ebx+1], 3
		ja p1
		inc ebx
		cmp ebx, edi
		jl xloop
		jmp end_yloop
p1:
		inc BYTE PTR [esi+ebx]
		cmp BYTE PTR [ecx+ebx], 19
		ja b2
		inc ebx
		cmp ebx, edi
		jl xloop
		jmp end_yloop
b2:
		xor edi,edi
		cmp BYTE PTR [eax+ebx-1], 19
		mov lower, 0
		mov upper, 0
		jbe b3
		inc edi
b3:
		cmp BYTE PTR [eax+ebx], 19
		jbe b4
		inc edi
b4:
		cmp BYTE PTR [eax+ebx+1], 19
		jbe b5
		inc edi
b5:
		or edi,edi
		jz p2
		mov upper, 1
p2:
		cmp BYTE PTR [ecx+ebx-1], 19
		jbe b6
		inc edi
b6:
		cmp BYTE PTR [ecx+ebx+1], 19
		jbe b7
		inc edi
b7:
		mov esi, edi
		cmp BYTE PTR [edx+ebx-1], 19
		jbe b8
		inc edi
b8:
		cmp BYTE PTR [edx+ebx], 19
		jbe b9
		inc edi
b9:
		cmp BYTE PTR [edx+ebx+1], 19
		jbe b10
		inc edi
b10:
		cmp edi, 2
		jg c1
c2:
		mov edi, Width
		inc ebx
		dec edi
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
		add eax, -4
		jge p3
		xor eax, eax
p3:
		mov edx, ebx
		mov ecx, Width
		mov lower2, 0
		add edx, 5
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
		inc esi
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
		inc esi
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
		inc esi
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
		inc ebx
		dec edi
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

void TDeinterlace::buildDiffMapPlaneYUY2(const unsigned char *prvp, const unsigned char *nxtp, 
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