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

__declspec(align(16)) const __int64 onesMask[2] = { 0x0101010101010101, 0x0101010101010101 };
__declspec(align(16)) const __int64 onesMaskLuma[2] = { 0x0001000100010001, 0x0001000100010001 };
__declspec(align(16)) const __int64 twosMask[2] = { 0x0202020202020202, 0x0202020202020202 };
__declspec(align(16)) const __int64 mask251[2] = { 0xFBFBFBFBFBFBFBFB, 0xFBFBFBFBFBFBFBFB };
__declspec(align(16)) const __int64 mask235[2] = { 0xEBEBEBEBEBEBEBEB, 0xEBEBEBEBEBEBEBEB };
__declspec(align(16)) const __int64 lumaMask[2] = { 0x00FF00FF00FF00FF, 0x00FF00FF00FF00FF };
__declspec(align(16)) const __int64 threeMask[2] = { 0x0003000300030003, 0x0003000300030003 };
__declspec(align(16)) const __int64 ffMask[2] = { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF };

void TFM::checkSceneChangeYV12_1_SSE2(const unsigned char *prvp, const unsigned char *srcp, 
	int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov esi,prv_pitch
		mov ebx,src_pitch
		mov edx,width
		pxor xmm7,xmm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]
		psadbw xmm0,[edi+ecx]
		add ecx,16
		paddd xmm7,xmm0
		cmp ecx,edx
		jl xloop
		add eax,esi
		add edi,ebx
		dec height
		jnz yloop
		mov eax,diffp
		movdqa xmm6,xmm7
		psrldq xmm7,8
		paddd xmm6,xmm7
		movd [eax],xmm6
	}
}

void TFM::checkSceneChangeYV12_1_ISSE(const unsigned char *prvp, const unsigned char *srcp, 
	int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov esi,prv_pitch
		mov ebx,src_pitch
		mov edx,width
		pxor mm6,mm6
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]
		movq mm1,[eax+ecx+8]
		psadbw mm0,[edi+ecx]
		psadbw mm1,[edi+ecx+8]
		add ecx,16
		paddd mm6,mm0
		paddd mm7,mm1
		cmp ecx,edx
		jl xloop
		add eax,esi
		add edi,ebx
		dec height
		jnz yloop
		paddd mm6,mm7
		mov eax,diffp
		movd [eax],mm6
		emms
	}
}

void TFM::checkSceneChangeYV12_1_MMX(const unsigned char *prvp, const unsigned char *srcp, 
	int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov esi,prv_pitch
		mov ebx,src_pitch
		mov edx,width
		pxor mm6,mm6
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]
		movq mm1,[eax+ecx+8]
		movq mm2,[edi+ecx]
		movq mm3,[edi+ecx+8]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		por mm0,mm2
		por mm1,mm3
		pxor mm4,mm4
		pxor mm5,mm5
		movq mm2,mm0
		movq mm3,mm1
		punpcklbw mm0,mm4
		punpcklbw mm1,mm5
		punpckhbw mm2,mm4
		punpckhbw mm3,mm5
		paddw mm0,mm1
		paddw mm2,mm3
		paddw mm0,mm2
		movq mm1,mm0
		punpcklwd mm0,mm4
		punpckhwd mm1,mm5
		add ecx,16
		paddd mm6,mm0
		paddd mm7,mm1
		cmp ecx,edx
		jl xloop
		add eax,esi
		add edi,ebx
		dec height
		jnz yloop
		paddd mm6,mm7
		movq mm5,mm6
		psrlq mm6,32
		paddd mm5,mm6
		mov eax,diffp
		movd [eax],mm5
		emms
	}
}

void TFM::checkSceneChangeYV12_2_SSE2(const unsigned char *prvp, const unsigned char *srcp, 
	const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
	int nxt_pitch, unsigned long &diffp, unsigned long &diffn)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov ebx,nxtp
		mov edx,width
		mov esi,height
		pxor xmm6,xmm6
		pxor xmm7,xmm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]
		movdqa xmm1,[edi+ecx]
		movdqa xmm2,[ebx+ecx]
		psadbw xmm0,xmm1
		add ecx,16
		psadbw xmm2,xmm1
		paddd xmm6,xmm0
		paddd xmm7,xmm2
		cmp ecx,edx
		jl xloop
		add eax,prv_pitch
		add edi,src_pitch
		add ebx,nxt_pitch
		dec esi
		jnz yloop
		mov eax,diffp
		mov ebx,diffn
		movdqa xmm4,xmm6
		movdqa xmm5,xmm7
		psrldq xmm6,8
		psrldq xmm7,8
		paddd xmm4,xmm6
		paddd xmm5,xmm7
		movd [eax],xmm4
		movd [ebx],xmm5
	}
}

void TFM::checkSceneChangeYV12_2_ISSE(const unsigned char *prvp, const unsigned char *srcp, 
	const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
	int nxt_pitch, unsigned long &diffp, unsigned long &diffn)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov ebx,nxtp
		mov edx,width
		mov esi,height
		pxor mm6,mm6
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]
		movq mm1,[eax+ecx+8]
		movq mm2,[edi+ecx]
		movq mm3,[edi+ecx+8]
		movq mm4,[ebx+ecx]
		movq mm5,[ebx+ecx+8]
		psadbw mm0,mm2
		psadbw mm5,mm3
		psadbw mm4,mm2
		psadbw mm1,mm3
		paddd mm6,mm0
		paddd mm7,mm4
		add ecx,16
		paddd mm6,mm1
		paddd mm7,mm5
		cmp ecx,edx
		jl xloop
		add eax,prv_pitch
		add edi,src_pitch
		add ebx,nxt_pitch
		dec esi
		jnz yloop
		mov eax,diffp
		mov ebx,diffn
		movd [eax],mm6
		movd [ebx],mm7
		emms
	}
}

void TFM::checkSceneChangeYV12_2_MMX(const unsigned char *prvp, const unsigned char *srcp, 
	const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
	int nxt_pitch, unsigned long &diffp, unsigned long &diffn)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov ebx,nxtp
		mov edx,width
		mov esi,height
		pxor mm6,mm6
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]
		movq mm1,[eax+ecx+8]
		movq mm2,[edi+ecx]
		movq mm3,[edi+ecx+8]
		movq mm4,mm2
		movq mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		psubusb mm0,mm4
		psubusb mm1,mm5
		por mm0,mm2
		por mm1,mm3
		movq mm2,mm0
		pxor mm3,mm3
		punpcklbw mm0,mm3
		punpckhbw mm2,mm3
		paddw mm0,mm2
		movq mm2,mm1
		punpcklbw mm1,mm3
		punpckhbw mm2,mm3
		paddw mm1,mm2
		paddw mm0,mm1
		movq mm1,mm0
		punpcklwd mm0,mm3
		punpckhwd mm1,mm3
		paddd mm6,mm0
		paddd mm6,mm1
		movq mm0,[ebx+ecx]
		movq mm1,[ebx+ecx+8]
		movq mm2,mm4
		movq mm3,mm5
		psubusb mm2,mm0
		psubusb mm3,mm1
		psubusb mm0,mm4
		psubusb mm1,mm5
		por mm0,mm2
		por mm1,mm3
		movq mm2,mm0
		pxor mm3,mm3
		punpcklbw mm0,mm3
		punpckhbw mm2,mm3
		paddw mm0,mm2
		movq mm2,mm1
		punpcklbw mm1,mm3
		punpckhbw mm2,mm3
		paddw mm1,mm2
		paddw mm0,mm1
		movq mm1,mm0
		punpcklwd mm0,mm3
		punpckhwd mm1,mm3
		add ecx,16
		paddd mm7,mm0
		paddd mm7,mm1
		cmp ecx,edx
		jl xloop
		add eax,prv_pitch
		add edi,src_pitch
		add ebx,nxt_pitch
		dec esi
		jnz yloop
		mov eax,diffp
		mov ebx,diffn
		movq mm4,mm6
		movq mm5,mm7
		psrlq mm6,32
		psrlq mm7,32
		paddd mm4,mm6
		paddd mm5,mm7
		movd [eax],mm4
		movd [ebx],mm5
		emms
	}
}

void TFM::checkSceneChangeYUY2_1_SSE2(const unsigned char *prvp, const unsigned char *srcp, 
	int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov esi,prv_pitch
		mov ebx,src_pitch
		mov edx,width
		movdqa xmm5,lumaMask
		movdqa xmm6,lumaMask
		pxor xmm7,xmm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]
		movdqa xmm1,[edi+ecx]
		pand xmm0,xmm5
		pand xmm1,xmm6
		psadbw xmm0,xmm1
		add ecx,16
		paddd xmm7,xmm0
		cmp ecx,edx
		jl xloop
		add eax,esi
		add edi,ebx
		dec height
		jnz yloop
		mov eax,diffp
		movdqa xmm6,xmm7
		psrldq xmm7,8
		paddd xmm6,xmm7
		movd [eax],xmm6
	}
}

void TFM::checkSceneChangeYUY2_1_ISSE(const unsigned char *prvp, const unsigned char *srcp, 
	int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov esi,prv_pitch
		mov ebx,src_pitch
		mov edx,width
		movq mm4,lumaMask
		movq mm5,lumaMask
		pxor mm6,mm6
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]
		movq mm1,[eax+ecx+8]
		movq mm2,[edi+ecx]
		movq mm3,[edi+ecx+8]
		pand mm0,mm4
		pand mm1,mm5
		pand mm2,mm4
		pand mm3,mm5
		psadbw mm0,mm2
		psadbw mm1,mm3
		add ecx,16
		paddd mm6,mm0
		paddd mm7,mm1
		cmp ecx,edx
		jl xloop
		add eax,esi
		add edi,ebx
		dec height
		jnz yloop
		paddd mm6,mm7
		mov eax,diffp
		movd [eax],mm6
		emms
	}
}

void TFM::checkSceneChangeYUY2_1_MMX(const unsigned char *prvp, const unsigned char *srcp, 
	int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov esi,prv_pitch
		mov ebx,src_pitch
		mov edx,width
		pxor mm6,mm6
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]
		movq mm1,[eax+ecx+8]
		movq mm2,[edi+ecx]
		movq mm3,[edi+ecx+8]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		por mm0,mm2
		por mm1,mm3
		pand mm0,lumaMask
		pand mm1,lumaMask
		pxor mm4,mm4
		paddw mm0,mm1
		pxor mm5,mm5
		movq mm1,mm0
		punpcklwd mm0,mm4
		punpckhwd mm1,mm5
		add ecx,16
		paddd mm6,mm0
		paddd mm7,mm1
		cmp ecx,edx
		jl xloop
		add eax,esi
		add edi,ebx
		dec height
		jnz yloop
		paddd mm6,mm7
		movq mm5,mm6
		psrlq mm6,32
		paddd mm5,mm6
		mov eax,diffp
		movd [eax],mm5
		emms
	}
}

void TFM::checkSceneChangeYUY2_2_SSE2(const unsigned char *prvp, const unsigned char *srcp, 
	const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
	int nxt_pitch, unsigned long &diffp, unsigned long &diffn)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov ebx,nxtp
		mov edx,width
		mov esi,height
		movdqa xmm4,lumaMask
		movdqa xmm5,lumaMask
		pxor xmm6,xmm6
		pxor xmm7,xmm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]
		movdqa xmm1,[edi+ecx]
		movdqa xmm2,[ebx+ecx]
		pand xmm0,xmm4
		pand xmm1,xmm5
		pand xmm2,xmm4
		psadbw xmm0,xmm1
		add ecx,16
		psadbw xmm2,xmm1
		paddd xmm6,xmm0
		paddd xmm7,xmm2
		cmp ecx,edx
		jl xloop
		add eax,prv_pitch
		add edi,src_pitch
		add ebx,nxt_pitch
		dec esi
		jnz yloop
		mov eax,diffp
		mov ebx,diffn
		movdqa xmm4,xmm6
		movdqa xmm5,xmm7
		psrldq xmm6,8
		psrldq xmm7,8
		paddd xmm4,xmm6
		paddd xmm5,xmm7
		movd [eax],xmm4
		movd [ebx],xmm5
	}
}

void TFM::checkSceneChangeYUY2_2_ISSE(const unsigned char *prvp, const unsigned char *srcp, 
	const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
	int nxt_pitch, unsigned long &diffp, unsigned long &diffn)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov ebx,nxtp
		mov edx,width
		mov esi,height
		movq mm4,lumaMask
		movq mm5,lumaMask
		pxor mm6,mm6
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]
		movq mm1,[eax+ecx+8]
		movq mm2,[edi+ecx]
		movq mm3,[edi+ecx+8]
		pand mm0,mm4
		pand mm1,mm5
		pand mm2,mm4
		pand mm3,mm5
		psadbw mm0,mm2
		psadbw mm1,mm3
		paddd mm6,mm0
		paddd mm6,mm1
		movq mm0,[ebx+ecx]
		movq mm1,[ebx+ecx+8]
		pand mm0,mm4
		pand mm1,mm5
		psadbw mm0,mm2
		psadbw mm1,mm3
		add ecx,16
		paddd mm7,mm0
		paddd mm7,mm1
		cmp ecx,edx
		jl xloop
		add eax,prv_pitch
		add edi,src_pitch
		add ebx,nxt_pitch
		dec esi
		jnz yloop
		mov eax,diffp
		mov ebx,diffn
		movd [eax],mm6
		movd [ebx],mm7
		emms
	}
}

void TFM::checkSceneChangeYUY2_2_MMX(const unsigned char *prvp, const unsigned char *srcp, 
	const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch, 
	int nxt_pitch, unsigned long &diffp, unsigned long &diffn)
{
	__asm
	{
		mov eax,prvp
		mov edi,srcp
		mov ebx,nxtp
		mov edx,width
		mov esi,height
		pxor mm6,mm6
		pxor mm7,mm7
yloop:
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]
		movq mm1,[eax+ecx+8]
		movq mm2,[edi+ecx]
		movq mm3,[edi+ecx+8]
		movq mm4,mm2
		movq mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		psubusb mm0,mm4
		psubusb mm1,mm5
		por mm2,mm0
		por mm3,mm1
		pand mm2,lumaMask
		pand mm3,lumaMask
		paddw mm2,mm3
		pxor mm0,mm0
		movq mm3,mm2
		pxor mm1,mm1
		punpcklwd mm2,mm0
		punpckhwd mm3,mm1
		paddd mm6,mm2
		paddd mm6,mm3
		movq mm0,[ebx+ecx]
		movq mm1,[ebx+ecx+8]
		movq mm2,mm4
		movq mm3,mm5
		psubusb mm4,mm0
		psubusb mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		por mm0,mm4
		por mm1,mm5
		pand mm0,lumaMask
		pand mm1,lumaMask
		paddw mm0,mm1
		pxor mm2,mm2
		movq mm1,mm0
		pxor mm3,mm3
		punpcklwd mm0,mm2
		punpckhwd mm1,mm3
		add ecx,16
		paddd mm7,mm0
		paddd mm7,mm1
		cmp ecx,edx
		jl xloop
		add eax,prv_pitch
		add edi,src_pitch
		add ebx,nxt_pitch
		dec esi
		jnz yloop
		mov eax,diffp
		mov ebx,diffn
		movq mm4,mm6
		movq mm5,mm7
		psrlq mm6,32
		psrlq mm7,32
		paddd mm4,mm6
		paddd mm5,mm7
		movd [eax],mm4
		movd [ebx],mm5
		emms
	}
}

void TFM::buildABSDiffMask_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
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

void TFM::buildABSDiffMask_MMX(const unsigned char *prvp, const unsigned char *nxtp,
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

void TFM::buildABSDiffMask2_SSE2(const unsigned char *prvp, const unsigned char *nxtp,
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
			movdqa xmm3,onesMask
			movdqa xmm4,twosMask
			pcmpeqb xmm5,xmm5
			movdqa xmm6,mask251
			movdqa xmm7,mask235
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
			jl xloop2
			movq xmm0,qword ptr[eax+ecx]
			movq xmm1,qword ptr[ebx+ecx]
			movq xmm2,xmm0
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
			movq qword ptr[edx+ecx],xmm0
			add eax,prv_pitch
			add ebx,nxt_pitch
			add edx,dst_pitch
			dec esi
			jnz yloop2
		}
	}
}

void TFM::buildABSDiffMask2_MMX(const unsigned char *prvp, const unsigned char *nxtp,
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

void TFM::check_combing_SSE2(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TFM::check_combing_iSSE(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TFM::check_combing_MMX(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TFM::check_combing_SSE2_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TFM::check_combing_iSSE_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TFM::check_combing_MMX_Luma(const unsigned char *srcp, unsigned char *dstp, int width, 
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

void TFM::check_combing_MMX_M1(const unsigned char *srcp, unsigned char *dstp, 
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

void TFM::check_combing_SSE2_M1(const unsigned char *srcp, unsigned char *dstp, 
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

void TFM::check_combing_MMX_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
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

void TFM::check_combing_SSE2_Luma_M1(const unsigned char *srcp, unsigned char *dstp, 
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

// There are no emms instructions at the end of these compute_sum 
// mmx/isse routines because it is called at the end of the routine 
// that calls these individual functions.

#pragma warning(push)
#pragma warning(disable:4799)	// disable no emms warning message

void TFM::compute_sum_8x8_mmx(const unsigned char *srcp, int pitch, int &sum)
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

void TFM::compute_sum_8x8_isse(const unsigned char *srcp, int pitch, int &sum)
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

void TFM::compute_sum_8x16_mmx_luma(const unsigned char *srcp, int pitch, int &sum)
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

void TFM::compute_sum_8x16_isse_luma(const unsigned char *srcp, int pitch, int &sum)
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

void TFM::compute_sum_8x16_sse2_luma(const unsigned char *srcp, int pitch, int &sum)
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