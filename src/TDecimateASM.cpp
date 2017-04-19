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

#include "TDecimate.h"

__declspec(align(16)) const __int64 lumaMask[2] = { 0x00FF00FF00FF00FF, 0x00FF00FF00FF00FF };
__declspec(align(16)) const __int64 hdd_mask[2] = { 0x00000000FFFFFFFF, 0x00000000FFFFFFFF };

// Leak's mmx blend routine
void TDecimate::blend_MMX_8(unsigned char* dstp, const unsigned char* srcp,  
			const unsigned char* nxtp, int width, int height, int dst_pitch, 
			int src_pitch, int nxt_pitch, double w1, double w2)
{
	int iw1t=(int)(w1*65536.0); iw1t+=(iw1t << 16);
	__int64 iw1=(__int64)iw1t; iw1+=(iw1 << 32);
	int iw2t=(int)(w2*65536.0); iw2t+=(iw2t << 16);
	__int64 iw2=(__int64)iw2t; iw2+=(iw2 << 32);
	__asm
	{
 		mov ebx,height
		mov eax,width
 		mov esi,srcp
 		mov edi,dstp
 		mov edx,nxtp
		add esi,eax
		add edi,eax
		add edx,eax
		neg eax
 		movq mm6,iw1
 		movq mm7,iw2
yloop:
		mov ecx,eax
 		align 16
xloop:
		movq mm0,[esi+ecx]
		movq mm1,[edx+ecx]
		movq mm2,mm0
		movq mm3,mm1
		punpcklbw mm0,mm0
		punpcklbw mm1,mm1
		pmulhuw mm0,mm6
		punpckhbw mm2,mm2
		pmulhuw mm1,mm7
		punpckhbw mm3,mm3
		paddusw mm0,mm1
		pmulhuw mm2,mm6
		pmulhuw mm3,mm7
 		psrlw mm0,8
		paddusw mm2,mm3
		psrlw mm2,8
		add ecx,8
		packuswb mm0,mm2
		movq [edi+ecx-8],mm0
		jnz xloop
 		add edi,dst_pitch
 		add esi,src_pitch
 		add edx,nxt_pitch
 		dec ebx
 		jnz yloop
 		emms
	}
}

// fast blend routine for 50:50 case
void TDecimate::blend_iSSE_5050(unsigned char* dstp, const unsigned char* srcp,  
			const unsigned char* nxtp, int width, int height, int dst_pitch, 
			int src_pitch, int nxt_pitch)
{
	__asm
	{
 		mov ebx,height
		mov eax,width
 		mov esi,srcp
 		mov edi,dstp
 		mov edx,nxtp
yloop:
		xor ecx,ecx
 		align 16
xloop:
		movq mm0,[esi+ecx]
		pavgb mm0,[edx+ecx]
		movq [edi+ecx],mm0
		add ecx,8
		cmp ecx,eax
		jl xloop
 		add edi,dst_pitch
 		add esi,src_pitch
 		add edx,nxt_pitch
 		dec ebx
 		jnz yloop
 		emms
	}
}

// Leak's sse2 blend routine
void TDecimate::blend_SSE2_16(unsigned char* dstp, const unsigned char* srcp,  
			const unsigned char* nxtp, int width, int height, int dst_pitch, 
			int src_pitch, int nxt_pitch, double w1, double w2)
{
	int iw1t=(int)(w1*65536.0); iw1t+=(iw1t << 16);
	__int64 iw1t2=(__int64)iw1t; iw1t2+=(iw1t2 << 32);
	int iw2t=(int)(w2*65536.0); iw2t+=(iw2t << 16);
	__int64 iw2t2=(__int64)iw2t; iw2t2+=(iw2t2 << 32);
	__int64 iw1[]={ iw1t2, iw1t2 };
	__int64 iw2[]={ iw2t2, iw2t2 };
	__asm
	{
 		mov ebx,height
		mov eax,width
 		mov esi,srcp
 		mov edi,dstp
 		mov edx,nxtp
		add esi,eax
		add edi,eax
		add edx,eax
		neg eax
 		movdqu xmm6,iw1
 		movdqu xmm7,iw2
yloop:
		mov ecx,eax
 		align 16
xloop:
		movdqa xmm0,[esi+ecx]
		movdqa xmm1,[edx+ecx]
		movdqa xmm2,xmm0
		movdqa xmm3,xmm1
		punpcklbw xmm0,xmm0
		punpcklbw xmm1,xmm1
		pmulhuw xmm0,xmm6
		punpckhbw xmm2,xmm2
		pmulhuw xmm1,xmm7
		punpckhbw xmm3,xmm3
		paddusw xmm0,xmm1
		pmulhuw xmm2,xmm6
		pmulhuw xmm3,xmm7
 		psrlw xmm0,8
		paddusw xmm2,xmm3
		psrlw xmm2,8
		add ecx,16
		packuswb xmm0,xmm2
		movdqa [edi+ecx-16],xmm0
		jnz xloop
 		add edi,dst_pitch
 		add esi,src_pitch
 		add edx,nxt_pitch
 		dec ebx
 		jnz yloop
	}
}

// fast blend routine for 50:50 case
void TDecimate::blend_SSE2_5050(unsigned char* dstp, const unsigned char* srcp,  
			const unsigned char* nxtp, int width, int height, int dst_pitch, 
			int src_pitch, int nxt_pitch)
{
	__asm
	{
 		mov ebx,height
		mov eax,width
 		mov esi,srcp
 		mov edi,dstp
 		mov edx,nxtp
yloop:
		xor ecx,ecx
 		align 16
xloop:
		movdqa xmm0,[esi+ecx]
		pavgb xmm0,[edx+ecx]
		movdqa [edi+ecx],xmm0
		add ecx,16
		cmp ecx,eax
		jl xloop
 		add edi,dst_pitch
 		add esi,src_pitch
 		add edx,nxt_pitch
 		dec ebx
 		jnz yloop
	}
}

void TDecimate::calcLumaDiffYUY2SAD_MMX_16(const unsigned char *prvp, const unsigned char *nxtp, 
	int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad)
{
	__asm
	{
		mov edi,prvp
 		mov esi,nxtp
 		mov ecx,width
yloop:
		pxor mm6,mm6
		pxor mm7,mm7
		xor eax,eax
 		align 16
xloop:
		movq mm0,[edi+eax]
		movq mm1,[edi+eax+8]
		movq mm2,[esi+eax]
		movq mm3,[esi+eax+8]
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
		paddd mm6,mm0
		add eax,16
		paddd mm7,mm1
		cmp eax,ecx
		jl xloop
		mov eax,sad
		paddd mm6,mm7
		movq mm7,mm6
		psrlq mm6,32
		paddd mm7,mm6
		movd ebx,mm7
		xor edx,edx
		add ebx,[eax]
		adc edx,[eax+4]
		mov [eax],ebx
		mov [eax+4],edx
 		add edi,prv_pitch
 		add esi,nxt_pitch
 		dec height
 		jnz yloop
 		emms
	}
}

void TDecimate::calcLumaDiffYUY2SAD_ISSE_16(const unsigned char *prvp, const unsigned char *nxtp, 
	int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad)
{
	__asm
	{
		mov edi,prvp
 		mov esi,nxtp
 		mov ecx,width
		movq mm4,lumaMask
		movq mm5,lumaMask
yloop:
		pxor mm6,mm6
		pxor mm7,mm7
		xor eax,eax
 		align 16
xloop:
		movq mm0,[edi+eax]
		movq mm1,[edi+eax+8]
		movq mm2,[esi+eax]
		movq mm3,[esi+eax+8]
		pand mm0,mm4
		pand mm1,mm5
		pand mm2,mm4
		pand mm3,mm5
		psadbw mm0,mm2
		psadbw mm1,mm3
		add eax,16
		paddd mm6,mm0
		paddd mm7,mm1
		cmp eax,ecx
		jl xloop
		mov eax,sad
		paddd mm6,mm7
		movq mm7,mm6
		psrlq mm6,32
		paddd mm7,mm6
		movd ebx,mm7
		xor edx,edx
		add ebx,[eax]
		adc edx,[eax+4]
		mov [eax],ebx
		mov [eax+4],edx
 		add edi,prv_pitch
 		add esi,nxt_pitch
 		dec height
 		jnz yloop
 		emms
	}
}

void TDecimate::calcLumaDiffYUY2SAD_SSE2_16(const unsigned char *prvp, const unsigned char *nxtp, 
	int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad)
{
	__asm
	{
		mov edi,prvp
 		mov esi,nxtp
 		mov ecx,width
		mov edx,prv_pitch
		mov ebx,nxt_pitch
		movdqa xmm5,lumaMask
		movdqa xmm6,lumaMask
		pxor xmm7,xmm7
yloop:
		xor eax,eax
 		align 16
xloop:
		movdqa xmm0,[edi+eax]
		movdqa xmm1,[esi+eax]
		pand xmm0,xmm5
		pand xmm1,xmm6
		psadbw xmm0,xmm1
		add eax,16
		paddq xmm7,xmm0
		cmp eax,ecx
		jl xloop
 		add edi,edx
 		add esi,ebx
 		dec height
 		jnz yloop
		mov eax,sad
		movdqa xmm6,xmm7
		psrldq xmm7,8
		paddq xmm6,xmm7
		movq qword ptr[eax],xmm6
	}
}

void TDecimate::calcLumaDiffYUY2SSD_MMX_16(const unsigned char *prvp, const unsigned char *nxtp, 
	int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &ssd)
{
	__asm
	{
		mov edi,prvp
 		mov esi,nxtp
 		mov ecx,width
yloop:
		pxor mm6,mm6
		pxor mm7,mm7
		xor eax,eax
 		align 16
xloop:
		movq mm0,[edi+eax]
		movq mm1,[edi+eax+8]
		movq mm2,[esi+eax]
		movq mm3,[esi+eax+8]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm4,mm2
		psubusb mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		por mm2,mm4
		por mm3,mm5
		pand mm2,lumaMask
		pand mm3,lumaMask
		pmaddwd mm2,mm2
		pmaddwd mm3,mm3
		paddd mm6,mm2
		add eax,16
		paddd mm7,mm3
		cmp eax,ecx
		jl xloop
		mov eax,ssd
		paddd mm6,mm7
		movq mm7,mm6
		psrlq mm6,32
		paddd mm7,mm6
		movd ebx,mm7
		xor edx,edx
		add ebx,[eax]
		adc edx,[eax+4]
		mov [eax],ebx
		mov [eax+4],edx
 		add edi,prv_pitch
 		add esi,nxt_pitch
 		dec height
 		jnz yloop
 		emms
	}
}

void TDecimate::calcLumaDiffYUY2SSD_SSE2_16(const unsigned char *prvp, const unsigned char *nxtp, 
	int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &ssd)
{
	__asm
	{
		mov edi,prvp
 		mov esi,nxtp
 		mov ecx,width
		mov eax,ssd
		mov edx,prv_pitch
		mov ebx,nxt_pitch
		movdqa xmm4,lumaMask
		pxor xmm5,xmm5
		pxor xmm7,xmm7
		movq qword ptr[eax],xmm7
yloop:
		xor eax,eax
		pxor xmm6,xmm6
 		align 16
xloop:
		movdqa xmm0,[edi+eax]
		movdqa xmm1,[esi+eax]
		movdqa xmm2,xmm0
		psubusb xmm0,xmm1
		psubusb xmm1,xmm2
		por xmm0,xmm1
		pand xmm0,xmm4
		pmaddwd xmm0,xmm0
		add eax,16
		paddd xmm6,xmm0
		cmp eax,ecx
		jl xloop
		movdqa xmm0,xmm6
		mov eax,ssd
		punpckldq xmm6,xmm5
		punpckhdq xmm0,xmm5
		paddq xmm6,xmm0
		movq xmm1,qword ptr[eax]
		movq xmm0,xmm6
		psrldq xmm6,8
		paddq xmm1,xmm0
		paddq xmm1,xmm6
		movq qword ptr[eax],xmm1
 		add edi,edx
 		add esi,ebx
 		dec height
 		jnz yloop
	}
}

void TDecimate::calcLumaDiffYUY2SAD_MMX_8(const unsigned char *prvp, const unsigned char *nxtp, 
	int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad)
{
	__asm
	{
		mov edi,prvp
 		mov esi,nxtp
 		mov ecx,width
		movq mm3,hdd_mask
		pxor mm4,mm4
		pxor mm5,mm5
		movq mm6,lumaMask
yloop:
		pxor mm7,mm7
		xor eax,eax
 		align 16
xloop:
		movq mm0,[edi+eax]
		movq mm1,[esi+eax]
		movq mm2,mm0
		psubusb mm0,mm1
		psubusb mm1,mm2
		por mm0,mm1
		pand mm0,mm6
		movq mm1,mm0
		punpcklwd mm0,mm4
		punpckhwd mm1,mm5
		paddd mm7,mm0
		add eax,8
		paddd mm7,mm1
		cmp eax,ecx
		jl xloop
		mov eax,sad
		movq mm0,mm7
		psrlq mm7,32
		paddd mm0,mm7
		movd ebx,mm0
		xor edx,edx
		add ebx,[eax]
		adc edx,[eax+4]
		mov [eax],ebx
		mov [eax+4],edx
 		add edi,prv_pitch
 		add esi,nxt_pitch
 		dec height
 		jnz yloop
 		emms
	}
}

void TDecimate::calcLumaDiffYUY2SAD_ISSE_8(const unsigned char *prvp, const unsigned char *nxtp, 
	int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &sad)
{
	__asm
	{
		mov edi,prvp
 		mov esi,nxtp
 		mov ecx,width
		movq mm2,lumaMask
		movq mm3,lumaMask
yloop:
		pxor mm4,mm4
		xor eax,eax
 		align 16
xloop:
		movq mm0,[edi+eax]
		movq mm1,[esi+eax]
		pand mm0,mm2
		pand mm1,mm3
		psadbw mm0,mm1
		add eax,8
		paddd mm4,mm0
		cmp eax,ecx
		jl xloop
		mov eax,sad
		movq mm0,mm4
		psrlq mm4,32
		paddd mm0,mm4
		movd ebx,mm0
		xor edx,edx
		add ebx,[eax]
		adc edx,[eax+4]
		mov [eax],ebx
		mov [eax+4],edx
 		add edi,prv_pitch
 		add esi,nxt_pitch
 		dec height
 		jnz yloop
 		emms
	}
}

void TDecimate::calcLumaDiffYUY2SSD_MMX_8(const unsigned char *prvp, const unsigned char *nxtp, 
	int width, int height, int prv_pitch, int nxt_pitch, unsigned __int64 &ssd)
{
	__asm
	{
		mov edi,prvp
 		mov esi,nxtp
 		mov ecx,width
		movq mm4,lumaMask
yloop:
		pxor mm5,mm5
		xor eax,eax
 		align 16
xloop:
		movq mm0,[edi+eax]
		movq mm1,[esi+eax]
		pand mm0,mm4
		pand mm1,mm4
		psubw mm0,mm1
		pmaddwd mm0,mm0
		add eax,8
		paddd mm5,mm0
		cmp eax,ecx
		jl xloop
		mov eax,ssd
		movq mm0,mm5
		psrlq mm5,32
		paddd mm0,mm5
		movd ebx,mm0
		xor edx,edx
		add ebx,[eax]
		adc edx,[eax+4]
		mov [eax],ebx
		mov [eax+4],edx
 		add edi,prv_pitch
 		add esi,nxt_pitch
 		dec height
 		jnz yloop
 		emms
	}
}

void TDecimate::calcSAD_SSE2_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,2
		pxor xmm7,xmm7
		align 16
loopy:
		movdqa xmm0,[edi]
		movdqa xmm1,[edi+edx]
		psadbw xmm0,[esi]
		psadbw xmm1,[esi+ecx]
		lea edi,[edi+edx*2]
		paddd xmm0,xmm1
		lea esi,[esi+ecx*2]
		movdqa xmm1,[edi]
		movdqa xmm2,[edi+edx]
		psadbw xmm1,[esi]
		psadbw xmm2,[esi+ecx]
		lea edi,[edi+edx*2]
		paddd xmm1,xmm2
		lea esi,[esi+ecx*2]
		movdqa xmm2,[edi]
		movdqa xmm3,[edi+edx]
		psadbw xmm2,[esi]
		psadbw xmm3,[esi+ecx]
		lea edi,[edi+edx*2]
		paddd xmm2,xmm3
		lea esi,[esi+ecx*2]
		movdqa xmm3,[edi]
		movdqa xmm4,[edi+edx]
		psadbw xmm3,[esi]
		psadbw xmm4,[esi+ecx]
		paddd xmm3,xmm4
		paddd xmm0,xmm1
		paddd xmm2,xmm3
		lea edi,[edi+edx*2]
		paddd xmm7,xmm0
		lea esi,[esi+ecx*2]
		paddd xmm7,xmm2
		dec ebx
		jnz loopy
		mov eax,sad
		movdqa xmm6,xmm7
		psrldq xmm7,8
		paddd xmm6,xmm7
		movd [eax],xmm6
	}
}

void TDecimate::calcSAD_SSE2_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,4
		pxor xmm7,xmm7
		align 16
loopy:
		movdqa xmm0,[edi]
		movdqa xmm1,[edi+16]
		movdqa xmm2,[edi+edx]
		movdqa xmm3,[edi+edx+16]
		psadbw xmm0,[esi]
		psadbw xmm1,[esi+16]
		psadbw xmm2,[esi+ecx]
		psadbw xmm3,[esi+ecx+16]
		paddd xmm0,xmm2
		lea edi,[edi+edx*2]
		paddd xmm1,xmm3
		lea esi,[esi+ecx*2]
		movdqa xmm2,[edi]
		movdqa xmm3,[edi+16]
		movdqa xmm4,[edi+edx]
		movdqa xmm5,[edi+edx+16]
		psadbw xmm2,[esi]
		psadbw xmm3,[esi+16]
		psadbw xmm4,[esi+ecx]
		psadbw xmm5,[esi+ecx+16]
		paddd xmm2,xmm4
		paddd xmm3,xmm5
		paddd xmm0,xmm1
		paddd xmm2,xmm3
		lea edi,[edi+edx*2]
		paddd xmm7,xmm0
		lea esi,[esi+ecx*2]
		paddd xmm7,xmm2
		dec ebx
		jnz loopy
		mov eax,sad
		movdqa xmm6,xmm7
		psrldq xmm7,8
		paddd xmm6,xmm7
		movd [eax],xmm6
	}
}

void TDecimate::calcSAD_SSE2_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,8
		movdqa xmm5,lumaMask
		movdqa xmm6,lumaMask
		pxor xmm7,xmm7
		align 16
loopy:
		movdqa xmm0,[edi]
		movdqa xmm1,[edi+16]
		movdqa xmm2,[esi]
		movdqa xmm3,[esi+16]
		pand xmm0,xmm5
		pand xmm1,xmm6
		pand xmm2,xmm5
		pand xmm3,xmm6
		psadbw xmm0,xmm2
		psadbw xmm1,xmm3
		paddd xmm0,xmm1
		movdqa xmm1,[edi+edx]
		movdqa xmm2,[edi+edx+16]
		movdqa xmm3,[esi+ecx]
		movdqa xmm4,[esi+ecx+16]
		pand xmm1,xmm5
		pand xmm2,xmm6
		pand xmm3,xmm5
		pand xmm4,xmm6
		psadbw xmm1,xmm3
		psadbw xmm2,xmm4
		lea edi,[edi+edx*2]
		paddd xmm1,xmm2
		paddd xmm7,xmm0
		lea esi,[esi+ecx*2]
		paddd xmm7,xmm1
		dec ebx
		jnz loopy
		mov eax,sad
		movdqa xmm6,xmm7
		psrldq xmm7,8
		paddd xmm6,xmm7
		movd [eax],xmm6
	}
}

// There are no emms instructions at the end of these block sad/ssd 
// mmx/isse routines because it is called at the end of the routine 
// that calls these individual functions.

#pragma warning(push)
#pragma warning(disable:4799) // disable no emms warning message

void TDecimate::calcSAD_iSSE_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,4
		pxor mm7,mm7
		align 16
loopy:
		movq mm0,[edi]
		movq mm1,[edi+8]
		psadbw mm0,[esi]
		psadbw mm1,[esi+8]
		add edi,edx
		paddd mm0,mm1
		add esi,ecx
		movq mm1,[edi]
		movq mm2,[edi+8]
		psadbw mm1,[esi]
		psadbw mm2,[esi+8]
		add edi,edx
		paddd mm1,mm2
		add esi,ecx
		movq mm2,[edi]
		movq mm3,[edi+8]
		psadbw mm2,[esi]
		psadbw mm3,[esi+8]
		add edi,edx
		paddd mm2,mm3
		add esi,ecx
		movq mm3,[edi]
		movq mm4,[edi+8]
		psadbw mm3,[esi]
		psadbw mm4,[esi+8]
		paddd mm0,mm1
		paddd mm3,mm4
		add edi,edx
		paddd mm2,mm3
		paddd mm7,mm0
		add esi,ecx
		paddd mm7,mm2
		dec ebx
		jnz loopy
		mov eax,sad
		movd [eax],mm7
	}
}

void TDecimate::calcSAD_iSSE_8x8(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		align 16
		movq mm0,[edi]
		movq mm1,[edi+edx]
		psadbw mm0,[esi]
		psadbw mm1,[esi+ecx]
		lea edi,[edi+edx*2]
		paddd mm0,mm1
		lea esi,[esi+ecx*2]
		movq mm1,[edi]
		movq mm2,[edi+edx]
		psadbw mm1,[esi]
		psadbw mm2,[esi+ecx]
		lea edi,[edi+edx*2]
		paddd mm1,mm2
		lea esi,[esi+ecx*2]
		movq mm2,[edi]
		movq mm3,[edi+edx]
		psadbw mm2,[esi]
		psadbw mm3,[esi+ecx]
		lea edi,[edi+edx*2]
		paddd mm2,mm3
		lea esi,[esi+ecx*2]
		movq mm3,[edi]
		movq mm4,[edi+edx]
		psadbw mm3,[esi]
		psadbw mm4,[esi+ecx]
		paddd mm3,mm4
		paddd mm0,mm1
		paddd mm2,mm3
		paddd mm0,mm2
		mov eax,sad
		movd [eax],mm0
	}
}

void TDecimate::calcSAD_iSSE_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		movq mm6,lumaMask
		movq mm7,lumaMask
		align 16
		movq mm0,[edi]
		movq mm1,[edi+edx]
		movq mm2,[esi]
		movq mm3,[esi+ecx]
		pand mm0,mm6
		pand mm1,mm7
		pand mm2,mm6
		pand mm3,mm7
		psadbw mm0,mm2
		psadbw mm1,mm3
		lea edi,[edi+edx*2]
		paddd mm0,mm1
		lea esi,[esi+ecx*2]
		movq mm1,[edi]
		movq mm2,[edi+edx]
		movq mm3,[esi]
		movq mm4,[esi+ecx]
		pand mm1,mm6
		pand mm2,mm7
		pand mm3,mm6
		pand mm4,mm7
		psadbw mm1,mm3
		psadbw mm2,mm4
		lea edi,[edi+edx*2]
		paddd mm1,mm2
		lea esi,[esi+ecx*2]
		movq mm2,[edi]
		movq mm3,[edi+edx]
		movq mm4,[esi]
		movq mm5,[esi+ecx]
		pand mm2,mm6
		pand mm3,mm7
		pand mm4,mm6
		pand mm5,mm7
		psadbw mm2,mm4
		psadbw mm3,mm5
		lea edi,[edi+edx*2]
		paddd mm2,mm3
		lea esi,[esi+ecx*2]
		paddd mm0,mm1
		movq mm3,[edi]
		movq mm4,[edi+edx]
		movq mm5,[esi]
		movq mm1,[esi+ecx]
		pand mm3,mm6
		pand mm4,mm7
		pand mm5,mm6
		pand mm1,mm7
		psadbw mm3,mm5
		psadbw mm4,mm1
		paddd mm3,mm4
		paddd mm0,mm2
		paddd mm0,mm3
		mov eax,sad
		movd [eax],mm0
	}
}

void TDecimate::calcSAD_iSSE_4x4(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		align 16
		movd mm0,[edi]
		movd mm1,[edi+edx]
		movd mm2,[esi]
		movd mm3,[esi+ecx]
		psadbw mm0,mm2
		psadbw mm1,mm3
		lea edi,[edi+edx*2]
		paddd mm0,mm1
		lea esi,[esi+ecx*2]
		movd mm1,[edi]
		movd mm2,[edi+edx]
		movd mm3,[esi]
		movd mm4,[esi+ecx]
		psadbw mm1,mm3
		psadbw mm2,mm4
		paddd mm1,mm2
		paddd mm1,mm0
		mov eax,sad
		movd [eax],mm1
	}
}

void TDecimate::calcSAD_iSSE_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,8
		pxor mm7,mm7
		align 16
loopy:
		movq mm0,[edi+0]
		movq mm1,[edi+8]
		psadbw mm0,[esi+0]
		psadbw mm1,[esi+8]
		paddd mm0,mm1
		movq mm1,[edi+16]
		movq mm2,[edi+24]
		psadbw mm1,[esi+16]
		psadbw mm2,[esi+24]
		paddd mm1,mm2
		movq mm2,[edi+edx+0]
		movq mm3,[edi+edx+8]
		psadbw mm2,[esi+ecx+0]
		psadbw mm3,[esi+ecx+8]
		paddd mm2,mm3
		movq mm3,[edi+edx+16]
		movq mm4,[edi+edx+24]
		psadbw mm3,[esi+ecx+16]
		psadbw mm4,[esi+ecx+24]
		paddd mm0,mm1
		paddd mm3,mm4
		lea edi,[edi+edx*2]
		paddd mm2,mm3
		paddd mm7,mm0
		lea esi,[esi+ecx*2]
		paddd mm7,mm2
		dec ebx
		jnz loopy
		mov eax,sad
		movd [eax],mm7
	}
}

void TDecimate::calcSAD_iSSE_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,16
		movq mm5,lumaMask
		movq mm6,lumaMask
		pxor mm7,mm7
		align 16
loopy:
		movq mm0,[edi+0]
		movq mm1,[edi+8]
		movq mm2,[esi+0]
		movq mm3,[esi+8]
		pand mm0,mm5
		pand mm1,mm6
		pand mm2,mm5
		pand mm3,mm6
		psadbw mm0,mm2
		psadbw mm1,mm3
		paddd mm0,mm1
		movq mm1,[edi+16]
		movq mm2,[edi+24]
		movq mm3,[esi+16]
		movq mm4,[esi+24]
		pand mm1,mm5
		pand mm2,mm6
		pand mm3,mm5
		pand mm4,mm6
		psadbw mm1,mm3
		psadbw mm2,mm4
		add edi,edx
		paddd mm1,mm2
		paddd mm7,mm0
		add esi,ecx
		paddd mm7,mm1
		dec ebx
		jnz loopy
		mov eax,sad
		movd [eax],mm7
	}
}

void TDecimate::calcSAD_MMX_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,16
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
loopy:
		movq mm0,[edi]
		movq mm1,[edi+8]
		movq mm2,[esi]
		movq mm3,[esi+8]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		por mm0,mm2
		por mm1,mm3
		movq mm2,mm0
		movq mm3,mm1
		punpcklbw mm0,mm6
		punpcklbw mm1,mm6
		punpckhbw mm2,mm6
		punpckhbw mm3,mm6
		paddw mm0,mm1
		paddw mm2,mm3
		paddw mm0,mm2
		movq mm2,mm0
		punpcklwd mm0,mm6
		punpckhwd mm2,mm6
		add edi,edx
		paddd mm7,mm0
		add esi,ecx
		paddd mm7,mm2
		dec ebx
		jnz loopy
		mov eax,sad
		movq mm6,mm7
		psrlq mm7,32
		paddd mm6,mm7
		movd [eax],mm6
	}
}

void TDecimate::calcSAD_MMX_8x8(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,4
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
loopy:
		movq mm0,[edi]
		movq mm1,[edi+edx]
		movq mm2,[esi]
		movq mm3,[esi+ecx]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		por mm0,mm2
		por mm1,mm3
		movq mm2,mm0
		movq mm3,mm1
		punpcklbw mm0,mm6
		punpcklbw mm1,mm6
		punpckhbw mm2,mm6
		punpckhbw mm3,mm6
		paddw mm0,mm1
		paddw mm2,mm3
		paddw mm0,mm2
		movq mm2,mm0
		punpcklwd mm0,mm6
		punpckhwd mm2,mm6
		lea edi,[edi+edx*2]
		paddd mm7,mm0
		lea esi,[esi+ecx*2]
		paddd mm7,mm2
		dec ebx
		jnz loopy
		mov eax,sad
		movq mm6,mm7
		psrlq mm7,32
		paddd mm6,mm7
		movd [eax],mm6
	}
}

void TDecimate::calcSAD_MMX_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,4
		movq mm6,lumaMask
		pxor mm7,mm7
		align 16
loopy:
		movq mm0,[edi]
		movq mm1,[edi+edx]
		movq mm2,[esi]
		movq mm3,[esi+ecx]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		por mm0,mm2
		por mm1,mm3
		pand mm0,mm6
		pand mm1,mm6
		paddw mm0,mm1
		pxor mm3,mm3
		movq mm2,mm0
		punpcklwd mm0,mm3
		punpckhwd mm2,mm3
		lea edi,[edi+edx*2]
		paddd mm7,mm0
		lea esi,[esi+ecx*2]
		paddd mm7,mm2
		dec ebx
		jnz loopy
		mov eax,sad
		movq mm6,mm7
		psrlq mm7,32
		paddd mm6,mm7
		movd [eax],mm6
	}
}

void TDecimate::calcSAD_MMX_4x4(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		align 16
		movd mm0,[edi]
		movd mm1,[edi+edx]
		movd mm2,[esi]
		movd mm3,[esi+ecx]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		lea edi,[edi+edx*2]
		lea esi,[esi+ecx*2]
		por mm0,mm2
		por mm1,mm3
		movd mm2,[edi]
		movd mm3,[edi+edx]
		movd mm4,[esi]
		movd mm5,[esi+ecx]
		movq mm6,mm2
		movq mm7,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		psubusb mm4,mm6
		psubusb mm5,mm7
		por mm2,mm4
		por mm3,mm5
		pxor mm6,mm6
		pxor mm7,mm7
		movq mm4,mm0
		movq mm5,mm1
		punpcklbw mm0,mm6
		punpcklbw mm1,mm7
		punpckhbw mm4,mm6
		punpckhbw mm5,mm7
		paddw mm0,mm1
		paddw mm4,mm5
		movq mm1,mm2
		movq mm5,mm3
		punpcklbw mm2,mm6
		punpcklbw mm3,mm7
		punpckhbw mm1,mm6
		punpckhbw mm5,mm7
		paddw mm2,mm3
		paddw mm1,mm5
		paddw mm0,mm4
		paddw mm2,mm1
		paddw mm0,mm2
		movq mm1,mm0
		punpcklwd mm0,mm6
		punpckhwd mm1,mm7
		paddd mm0,mm1
		mov eax,sad
		movq mm2,mm0
		psrlq mm0,32
		paddd mm0,mm2
		movd [eax],mm0
	}
}

void TDecimate::calcSAD_MMX_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,16
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
loopy:
		movq mm0,[edi+0]
		movq mm1,[edi+8]
		movq mm2,[esi+0]
		movq mm3,[esi+8]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		por mm0,mm2
		por mm1,mm3
		movq mm2,mm0
		movq mm3,mm1
		punpcklbw mm0,mm6
		punpcklbw mm1,mm6
		punpckhbw mm2,mm6
		punpckhbw mm3,mm6
		paddw mm0,mm1
		paddw mm2,mm3
		paddw mm0,mm2
		movq mm2,mm0
		punpcklwd mm0,mm6
		punpckhwd mm2,mm6
		paddd mm7,mm0
		paddd mm7,mm2
		movq mm0,[edi+16]
		movq mm1,[edi+24]
		movq mm2,[esi+16]
		movq mm3,[esi+24]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		por mm0,mm2
		por mm1,mm3
		movq mm2,mm0
		movq mm3,mm1
		punpcklbw mm0,mm6
		punpcklbw mm1,mm6
		punpckhbw mm2,mm6
		punpckhbw mm3,mm6
		paddw mm0,mm1
		paddw mm2,mm3
		paddw mm0,mm2
		movq mm2,mm0
		punpcklwd mm0,mm6
		punpckhwd mm2,mm6
		paddd mm7,mm0
		add edi,edx
		paddd mm7,mm2
		add esi,ecx
		dec ebx
		jnz loopy
		mov eax,sad
		movq mm6,mm7
		psrlq mm7,32
		paddd mm6,mm7
		movd [eax],mm6
	}
}

void TDecimate::calcSAD_MMX_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &sad)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov ebx,16
		movq mm6,lumaMask
		pxor mm7,mm7
		align 16
loopy:
		movq mm0,[edi+0]
		movq mm1,[edi+8]
		movq mm2,[esi+0]
		movq mm3,[esi+8]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		por mm0,mm2
		por mm1,mm3
		pand mm0,mm6
		pand mm1,mm6
		paddw mm0,mm1
		pxor mm3,mm3
		movq mm2,mm0
		punpcklwd mm0,mm3
		punpckhwd mm2,mm3
		paddd mm7,mm0
		paddd mm7,mm2
		movq mm0,[edi+16]
		movq mm1,[edi+24]
		movq mm2,[esi+16]
		movq mm3,[esi+24]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm0,mm2
		psubusb mm1,mm3
		psubusb mm2,mm4
		psubusb mm3,mm5
		por mm0,mm2
		por mm1,mm3
		pand mm0,mm6
		pand mm1,mm6
		paddw mm0,mm1
		pxor mm3,mm3
		movq mm2,mm0
		punpcklwd mm0,mm3
		punpckhwd mm2,mm3
		paddd mm7,mm0
		add edi,edx
		paddd mm7,mm2
		add esi,ecx
		dec ebx
		jnz loopy
		mov eax,sad
		movq mm6,mm7
		psrlq mm7,32
		paddd mm6,mm7
		movd [eax],mm6
	}
}

void TDecimate::calcSSD_SSE2_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov eax,8
		pxor xmm6,xmm6
		pxor xmm7,xmm7
		align 16
yloop:
		movdqa xmm0,[edi]
		movdqa xmm1,[edi+edx]
		movdqa xmm2,[esi]
		movdqa xmm3,[esi+ecx]
		movdqa xmm4,xmm0
		movdqa xmm5,xmm1
		psubusb xmm4,xmm2
		psubusb xmm5,xmm3
		psubusb xmm2,xmm0
		psubusb xmm3,xmm1
		por xmm2,xmm4
		por xmm3,xmm5
		movdqa xmm0,xmm2
		movdqa xmm1,xmm3
		punpcklbw xmm0,xmm7
		punpckhbw xmm2,xmm7
		pmaddwd xmm0,xmm0
		pmaddwd xmm2,xmm2
		paddd xmm6,xmm0
		punpcklbw xmm1,xmm7
		paddd xmm6,xmm2
		punpckhbw xmm3,xmm7
		pmaddwd xmm1,xmm1
		pmaddwd xmm3,xmm3
		lea edi,[edi+edx*2]
		paddd xmm6,xmm1
		lea esi,[esi+ecx*2]
		paddd xmm6,xmm3
		dec eax
		jnz yloop
		movdqa xmm5,xmm6
		punpckldq xmm6,xmm7
		punpckhdq xmm5,xmm7
		paddq xmm6,xmm5
		mov eax,ssd
		movdqa xmm5,xmm6
		psrldq xmm6,8
		paddq xmm5,xmm6
		movd [eax],xmm5
	}
}

void TDecimate::calcSSD_SSE2_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov eax,8
		pxor xmm6,xmm6
		pxor xmm7,xmm7
		align 16
yloop:
		movdqa xmm0,[edi]
		movdqa xmm1,[edi+16]
		movdqa xmm2,[esi]
		movdqa xmm3,[esi+16]
		movdqa xmm4,xmm0
		movdqa xmm5,xmm1
		psubusb xmm4,xmm2
		psubusb xmm5,xmm3
		psubusb xmm2,xmm0
		psubusb xmm3,xmm1
		por xmm2,xmm4
		por xmm3,xmm5
		movdqa xmm0,xmm2
		movdqa xmm1,xmm3
		punpcklbw xmm0,xmm7
		punpckhbw xmm2,xmm7
		pmaddwd xmm0,xmm0
		pmaddwd xmm2,xmm2
		paddd xmm6,xmm0
		punpcklbw xmm1,xmm7
		paddd xmm6,xmm2
		punpckhbw xmm3,xmm7
		pmaddwd xmm1,xmm1
		pmaddwd xmm3,xmm3
		paddd xmm6,xmm1
		movdqa xmm0,[edi+edx]
		movdqa xmm1,[edi+edx+16]
		paddd xmm6,xmm3
		movdqa xmm2,[esi+ecx]
		movdqa xmm3,[esi+ecx+16]
		movdqa xmm4,xmm0
		movdqa xmm5,xmm1
		psubusb xmm4,xmm2
		psubusb xmm5,xmm3
		psubusb xmm2,xmm0
		psubusb xmm3,xmm1
		por xmm2,xmm4
		por xmm3,xmm5
		movdqa xmm0,xmm2
		movdqa xmm1,xmm3
		punpcklbw xmm0,xmm7
		punpckhbw xmm2,xmm7
		pmaddwd xmm0,xmm0
		pmaddwd xmm2,xmm2
		paddd xmm6,xmm0
		punpcklbw xmm1,xmm7
		paddd xmm6,xmm2
		punpckhbw xmm3,xmm7
		pmaddwd xmm1,xmm1
		pmaddwd xmm3,xmm3
		lea edi,[edi+edx*2]
		paddd xmm6,xmm1
		lea esi,[esi+ecx*2]
		paddd xmm6,xmm3
		dec eax
		jnz yloop
		movdqa xmm5,xmm6
		punpckldq xmm6,xmm7
		punpckhdq xmm5,xmm7
		paddq xmm6,xmm5
		mov eax,ssd
		movdqa xmm5,xmm6
		psrldq xmm6,8
		paddq xmm5,xmm6
		movd [eax],xmm5
	}
}

void TDecimate::calcSSD_SSE2_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov eax,8
		pxor xmm6,xmm6
		pxor xmm7,xmm7
		align 16
yloop:
		movdqa xmm0,[edi]
		movdqa xmm1,[edi+16]
		movdqa xmm2,[esi]
		movdqa xmm3,[esi+16]
		movdqa xmm4,xmm0
		movdqa xmm5,xmm1
		psubusb xmm4,xmm2
		psubusb xmm5,xmm3
		psubusb xmm2,xmm0
		psubusb xmm3,xmm1
		por xmm2,xmm4
		por xmm3,xmm5
		pand xmm2,lumaMask
		pand xmm3,lumaMask
		pmaddwd xmm2,xmm2
		pmaddwd xmm3,xmm3
		paddd xmm6,xmm2
		movdqa xmm0,[edi+edx]
		movdqa xmm1,[edi+edx+16]
		paddd xmm6,xmm3
		movdqa xmm2,[esi+ecx]
		movdqa xmm3,[esi+ecx+16]
		movdqa xmm4,xmm0
		movdqa xmm5,xmm1
		psubusb xmm4,xmm2
		psubusb xmm5,xmm3
		psubusb xmm2,xmm0
		psubusb xmm3,xmm1
		por xmm2,xmm4
		por xmm3,xmm5
		pand xmm2,lumaMask
		pand xmm3,lumaMask
		pmaddwd xmm2,xmm2
		pmaddwd xmm3,xmm3
		lea edi,[edi+edx*2]
		paddd xmm6,xmm2
		lea esi,[esi+ecx*2]
		paddd xmm6,xmm3
		dec eax
		jnz yloop
		movdqa xmm5,xmm6
		punpckldq xmm6,xmm7
		punpckhdq xmm5,xmm7
		paddq xmm6,xmm5
		mov eax,ssd
		movdqa xmm5,xmm6
		psrldq xmm6,8
		paddq xmm5,xmm6
		movd [eax],xmm5
	}
}

void TDecimate::calcSSD_MMX_16x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov eax,16
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
yloop:
		movq mm0,[edi]
		movq mm1,[edi+8]
		movq mm2,[esi]
		movq mm3,[esi+8]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm4,mm2
		psubusb mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		por mm2,mm4
		por mm3,mm5
		movq mm0,mm2
		movq mm1,mm3
		punpcklbw mm0,mm7
		punpckhbw mm2,mm7
		pmaddwd mm0,mm0
		pmaddwd mm2,mm2
		paddd mm6,mm0
		punpcklbw mm1,mm7
		paddd mm6,mm2
		punpckhbw mm3,mm7
		pmaddwd mm1,mm1
		pmaddwd mm3,mm3
		paddd mm6,mm1
		add edi,edx
		add esi,ecx
		paddd mm6,mm3
		dec eax
		jnz yloop
		movq mm5,mm6
		psrlq mm6,32
		mov eax,ssd
		paddd mm6,mm5
		movd [eax],mm6
	}
}

void TDecimate::calcSSD_MMX_8x8(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov eax,4
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
yloop:
		movq mm0,[edi]
		movq mm1,[edi+edx]
		movq mm2,[esi]
		movq mm3,[esi+ecx]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm4,mm2
		psubusb mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		por mm2,mm4
		por mm3,mm5
		movq mm0,mm2
		movq mm1,mm3
		punpcklbw mm0,mm7
		punpckhbw mm2,mm7
		pmaddwd mm0,mm0
		pmaddwd mm2,mm2
		paddd mm6,mm0
		punpcklbw mm1,mm7
		paddd mm6,mm2
		punpckhbw mm3,mm7
		pmaddwd mm1,mm1
		pmaddwd mm3,mm3
		paddd mm6,mm1
		lea edi,[edi+edx*2]
		lea esi,[esi+ecx*2]
		paddd mm6,mm3
		dec eax
		jnz yloop
		movq mm5,mm6
		psrlq mm6,32
		mov eax,ssd
		paddd mm6,mm5
		movd [eax],mm6
	}
}

void TDecimate::calcSSD_MMX_8x8_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov eax,4
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
yloop:
		movq mm0,[edi]
		movq mm1,[edi+edx]
		movq mm2,[esi]
		movq mm3,[esi+ecx]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm4,mm2
		psubusb mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		por mm2,mm4
		por mm3,mm5
		pand mm2,lumaMask
		pand mm3,lumaMask
		pmaddwd mm2,mm2
		pmaddwd mm3,mm3
		paddd mm6,mm2
		lea edi,[edi+edx*2]
		lea esi,[esi+ecx*2]
		paddd mm6,mm3
		dec eax
		jnz yloop
		movq mm5,mm6
		psrlq mm6,32
		mov eax,ssd
		paddd mm6,mm5
		movd [eax],mm6
	}
}

void TDecimate::calcSSD_MMX_4x4(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		pxor mm4,mm4
		pxor mm5,mm5
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
		movd mm0,[edi]
		movd mm1,[edi+edx]
		movd mm2,[esi]
		movd mm3,[esi+ecx]
		punpcklbw mm0,mm6
		punpcklbw mm1,mm7
		punpcklbw mm2,mm6
		punpcklbw mm3,mm7
		psubw mm0,mm1
		psubw mm2,mm3
		pmaddwd mm0,mm0
		pmaddwd mm2,mm2
		lea edi,[edi+edx*2]
		lea esi,[esi+ecx*2]
		paddd mm4,mm0
		paddd mm5,mm2
		movd mm0,[edi]
		movd mm1,[edi+edx]
		movd mm2,[esi]
		movd mm3,[esi+ecx]
		punpcklbw mm0,mm6
		punpcklbw mm1,mm7
		punpcklbw mm2,mm6
		punpcklbw mm3,mm7
		psubw mm0,mm1
		psubw mm2,mm3
		pmaddwd mm0,mm0
		pmaddwd mm2,mm2
		paddd mm4,mm0
		paddd mm5,mm2
		paddd mm4,mm5
		mov eax,ssd
		movq mm5,mm4
		psrlq mm4,32
		paddd mm5,mm4
		movd [eax],mm5
	}
}

void TDecimate::calcSSD_MMX_32x16(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov eax,16
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
yloop:
		movq mm0,[edi]
		movq mm1,[edi+8]
		movq mm2,[esi]
		movq mm3,[esi+8]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm4,mm2
		psubusb mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		por mm2,mm4
		por mm3,mm5
		movq mm0,mm2
		movq mm1,mm3
		punpcklbw mm0,mm7
		punpckhbw mm2,mm7
		pmaddwd mm0,mm0
		pmaddwd mm2,mm2
		paddd mm6,mm0
		punpcklbw mm1,mm7
		paddd mm6,mm2
		punpckhbw mm3,mm7
		pmaddwd mm1,mm1
		pmaddwd mm3,mm3
		paddd mm6,mm1
		movq mm0,[edi+16]
		movq mm1,[edi+24]
		paddd mm6,mm3
		movq mm2,[esi+16]
		movq mm3,[esi+24]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm4,mm2
		psubusb mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		por mm2,mm4
		por mm3,mm5
		movq mm0,mm2
		movq mm1,mm3
		punpcklbw mm0,mm7
		punpckhbw mm2,mm7
		pmaddwd mm0,mm0
		pmaddwd mm2,mm2
		paddd mm6,mm0
		punpcklbw mm1,mm7
		paddd mm6,mm2
		punpckhbw mm3,mm7
		pmaddwd mm1,mm1
		pmaddwd mm3,mm3
		paddd mm6,mm1
		add edi,edx
		add esi,ecx
		paddd mm6,mm3
		dec eax
		jnz yloop
		movq mm5,mm6
		psrlq mm6,32
		mov eax,ssd
		paddd mm6,mm5
		movd [eax],mm6
	}
}

void TDecimate::calcSSD_MMX_32x16_luma(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int &ssd)
{
	__asm
	{
		mov edi,ptr1
		mov esi,ptr2
		mov edx,pitch1
		mov ecx,pitch2
		mov eax,16
		pxor mm6,mm6
		pxor mm7,mm7
		align 16
yloop:
		movq mm0,[edi]
		movq mm1,[edi+8]
		movq mm2,[esi]
		movq mm3,[esi+8]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm4,mm2
		psubusb mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		por mm2,mm4
		por mm3,mm5
		pand mm2,lumaMask
		pand mm3,lumaMask
		pmaddwd mm2,mm2
		pmaddwd mm3,mm3
		paddd mm6,mm2
		movq mm0,[edi+16]
		movq mm1,[edi+24]
		paddd mm6,mm3
		movq mm2,[esi+16]
		movq mm3,[esi+24]
		movq mm4,mm0
		movq mm5,mm1
		psubusb mm4,mm2
		psubusb mm5,mm3
		psubusb mm2,mm0
		psubusb mm3,mm1
		por mm2,mm4
		por mm3,mm5
		pand mm2,lumaMask
		pand mm3,lumaMask
		pmaddwd mm2,mm2
		pmaddwd mm3,mm3
		paddd mm6,mm2
		add edi,edx
		add esi,ecx
		paddd mm6,mm3
		dec eax
		jnz yloop
		movq mm5,mm6
		psrlq mm6,32
		mov eax,ssd
		paddd mm6,mm5
		movd [eax],mm6
	}
}

#pragma warning(pop)	// reenable no emms warning