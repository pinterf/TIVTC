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

#include "THelper.h"

TDHelper::~TDHelper()
{
	// nothing to free
}

TDHelper::TDHelper(PClip _child, int _order, int _field, double _lim, bool _debug, 
	int _opt, int* _sa, int _slow, TDeinterlace * _tdptr, IScriptEnvironment *env) : 
	GenericVideoFilter(_child), order(_order), field(_field), debug(_debug), opt(_opt), 
	sa(_sa), slow(_slow), tdptr(_tdptr)
{
	if (!vi.IsYV12() && !vi.IsYUY2())
		env->ThrowError("TDHelper:  only YV12 and YUY2 input supported!");
	if (order != -1 && order != 0 && order != 1)
		env->ThrowError("TDHelper:  order must be set to -1, 0, or 1!");
	if (field != -1 && field != 0 && field != 1)
		env->ThrowError("TDHelper:  field must be set to -1, 0, or 1!");
	if (opt < 0 || opt > 4)
		env->ThrowError("TDHelper:  opt must be set to 0, 1, 2, 3, or 4!");
	if (slow < 0 || slow > 2)
		env->ThrowError("TDHelper:  slow must be set to 0, 1, or 2!");
	if (!tdptr)
		env->ThrowError("TDHelper:  tdptr not set!");
	if (order == -1) order = child->GetParity(0);
	if (field == -1) field = child->GetParity(0);
	nfrms = vi.num_frames;
	vi.num_frames >>= 1;
	vi.SetFPS(vi.fps_numerator, vi.fps_denominator*2);
	child->SetCacheHints(CACHE_RANGE, 3);
	if (_lim < 0.0) lim = ULONG_MAX;
	else
	{
		double dlim = _lim*vi.height*vi.width*219.0/100.0;
		lim = min(max(0,int(dlim)),ULONG_MAX);
	}
}

int TDHelper::mapn(int n)
{
	if (n == -1) n += 2;
	else if (n == nfrms) n -= 2;
	if (n < 0) return 0;
	if (n >= nfrms) return nfrms;
	return n;
}

PVideoFrame __stdcall TDHelper::GetFrame(int n, IScriptEnvironment *env)
{
	n *= 2;
	if (field != order) ++n;
	PVideoFrame prv = child->GetFrame(mapn(n-1), env);
	PVideoFrame src = child->GetFrame(mapn(n), env);
	PVideoFrame nxt = child->GetFrame(mapn(n+1), env);
	int norm1 = -1, norm2 = -1;
	int mtn1 = -1, mtn2 = -1;
	if (sa)
	{
		for (int i=0; i<500; ++i)
		{
			if (sa[i*5] == n)
			{
				norm1 = sa[i*5+1];
				norm2 = sa[i*5+2];
				mtn1  = sa[i*5+3];
				mtn2  = sa[i*5+4];
				break;
			}
		}
		if (norm1 == -1 || norm2 == -1 || mtn1 == -1 || mtn2 == -1)
			env->ThrowError("TDeint:  mode 2 internal communication problem!");
	}
	else
		tdptr->subtractFields(prv,src,nxt,vi,norm1,norm2,mtn1,
			mtn2,field,order,opt,true,slow,env);
	if (debug)
	{
		sprintf(buf,"TDeint:  frame %d:  n1 = %u  n2 = %u  m1 = %u  m2 = %u\n", 
			n>>1, norm1, norm2, mtn1, mtn2);
		OutputDebugString(buf);
	}
	if (lim != ULONG_MAX)
	{
		unsigned long d1 = subtractFrames(prv, src, env);
		unsigned long d2 = subtractFrames(src, nxt, env);
		if (debug)
		{
			sprintf(buf,"TDeint:  frame %d:  d1 = %u  d2 = %u  lim = %u\n", n>>1, d1, d2, lim);
			OutputDebugString(buf);
		}
		if (d1 > lim && d2 > lim)
		{
			if (debug)
			{
				sprintf(buf,"TDeint:  frame %d:  not blending (returning src)\n", n>>1);
				OutputDebugString(buf);
			}
			return src;
		}
	}
	PVideoFrame dst = env->NewVideoFrame(vi);
	int ret = -1;
	float c1 = float(max(norm1,norm2))/float(max(min(norm1,norm2),1));
	float c2 = float(max(mtn1,mtn2))/float(max(min(mtn1,mtn2),1));
	float mr = float(max(mtn1,mtn2))/float(max(max(norm1,norm2),1));
	if (((mtn1 >= 500  || mtn2 >= 500)  && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1)) ||
		((mtn1 >= 1000 || mtn2 >= 1000) && (mtn1*3 < mtn2*2 || mtn2*3 < mtn1*2)) ||
		((mtn1 >= 2000 || mtn2 >= 2000) && (mtn1*5 < mtn2*4 || mtn2*5 < mtn1*4)) ||
		((mtn1 >= 4000 || mtn2 >= 4000) && c2 > c1))
	{
		if (mtn1 > mtn2) ret = 1;
		else ret = 0;
	}
	else if (mr > 0.005 && max(mtn1,mtn2) > 150 && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1))
	{
		if (mtn1 > mtn2) ret = 1;
		else ret = 0;
	}
	else
	{
		if (norm1 > norm2) ret = 1;
		else ret = 0;
	}
	if (ret == 0) blendFrames(prv, src, dst, env);
	else if (ret == 1) blendFrames(src, nxt, dst, env);
	else env->ThrowError("TDeint:  mode 2 internal error!");
	if (debug)
	{
		sprintf(buf,"TDeint:  frame %d:  blending with %s\n", n>>1, ret ? "nxt" : "prv");
		OutputDebugString(buf);
	}
	return dst;
}

unsigned long TDHelper::subtractFrames(PVideoFrame &src1, PVideoFrame &src2, IScriptEnvironment *env)
{
	unsigned long diff = 0;
	const unsigned char *srcp1 = src1->GetReadPtr();
	const int src1_pitch = src1->GetPitch();
	const int height = src1->GetHeight();
	const int width = (src1->GetRowSize()>>4)<<4;
	const unsigned char *srcp2 = src2->GetReadPtr();
	const int src2_pitch = src2->GetPitch();
	const int inc = vi.IsYV12() ? 1 : 2;
	long cpu = env->GetCPUFlags();
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_SSE2) && !((int(srcp1)|int(srcp2)|src1_pitch|src2_pitch)&15))
		subtractFramesSSE2(srcp1, src1_pitch, srcp2, src2_pitch, height, width, inc, diff);
	else if (cpu&CPUF_INTEGER_SSE)
		subtractFramesISSE(srcp1, src1_pitch, srcp2, src2_pitch, height, width, inc, diff);
	else if (cpu&CPUF_MMX)
		subtractFramesMMX(srcp1, src1_pitch, srcp2, src2_pitch, height, width, inc, diff);
	else
	{
		for (int y=0; y<height; ++y)
		{
			for (int x=0; x<width; x+=inc)
				diff += abs(srcp1[x]-srcp2[x]);
			srcp1 += src1_pitch;
			srcp2 += src2_pitch;
		}
	}
	return diff;
}

void TDHelper::blendFrames(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, IScriptEnvironment *env)
{
	int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
	int stop = vi.IsYV12() ? 3 : 1;
	long cpu = env->GetCPUFlags();
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	for (int b=0; b<stop; ++b)
	{
		const unsigned char *srcp1 = src1->GetReadPtr(plane[b]);
		const int src1_pitch = src1->GetPitch(plane[b]);
		const int height = src1->GetHeight(plane[b]);
		const int width = src1->GetRowSize(plane[b]);
		const unsigned char *srcp2 = src2->GetReadPtr(plane[b]);
		const int src2_pitch = src2->GetPitch(plane[b]);
		unsigned char *dstp = dst->GetWritePtr(plane[b]);
		const int dst_pitch = dst->GetPitch(plane[b]);
		if ((cpu&CPUF_SSE2) && !((int(srcp1)|int(srcp2)|int(dstp)|src1_pitch|src2_pitch|dst_pitch)&15))
			blendFramesSSE2(srcp1, src1_pitch, srcp2, src2_pitch, dstp, dst_pitch, height, width);
		else if (cpu&CPUF_INTEGER_SSE)
			blendFramesISSE(srcp1, src1_pitch, srcp2, src2_pitch, dstp, dst_pitch, height, width);
		else if (cpu&CPUF_MMX)
			blendFramesMMX(srcp1, src1_pitch, srcp2, src2_pitch, dstp, dst_pitch, height, width);
		else
		{
			for (int y=0; y<height; ++y)
			{
				for (int x=0; x<width; ++x)
					dstp[x] = (srcp1[x]+srcp2[x]+1)>>1;
				srcp1 += src1_pitch;
				srcp2 += src2_pitch;
				dstp += dst_pitch;
			}
		}
	}
}

__declspec(align(16)) const __int64 lumaMask[2] = { 0x00FF00FF00FF00FF, 0x00FF00FF00FF00FF };
__declspec(align(16)) const __int64 onesMask[2] = { 0x0001000100010001, 0x0001000100010001 };

void TDHelper::subtractFramesSSE2(const unsigned char *srcp1, int src1_pitch, 
	const unsigned char *srcp2, int src2_pitch, int height, int width, int inc, 
	unsigned long &diff)
{
	if (inc == 1)
	{
		__asm
		{
			mov ebx,srcp1
			mov edx,srcp2
			mov ecx,width
			mov esi,height
			pxor xmm1,xmm1
yloopyv12:
			xor eax,eax
			align 16
xloopyv12:
			movdqa xmm0,[ebx+eax]
			psadbw xmm0,[edx+eax]
			add eax,16
			paddq xmm1,xmm0
			cmp eax,ecx
			jl xloopyv12
			add ebx,src1_pitch
			add edx,src2_pitch
			dec esi
			jnz yloopyv12
			movdqa xmm0,xmm1
			psrldq xmm1,8
			paddq xmm0,xmm1
			mov eax,diff
			movd [eax],xmm0
		}
	}
	else
	{
		__asm
		{
			mov ebx,srcp1
			mov edx,srcp2
			mov ecx,width
			mov esi,height
			movdqa xmm2,lumaMask
			movdqa xmm3,lumaMask
			pxor xmm4,xmm4
yloopyuy2:
			xor eax,eax
			align 16
xloopyuy2:
			movdqa xmm0,[ebx+eax]
			movdqa xmm1,[edx+eax]
			pand xmm0,xmm2
			pand xmm1,xmm3
			add eax,16
			psadbw xmm0,xmm1
			cmp eax,ecx
			paddq xmm4,xmm0
			jl xloopyuy2
			add ebx,src1_pitch
			add edx,src2_pitch
			dec esi
			jnz yloopyuy2
			movdqa xmm0,xmm4
			psrldq xmm4,8
			paddq xmm0,xmm4
			mov eax,diff
			movd [eax],xmm0
		}
	}
}

void TDHelper::subtractFramesISSE(const unsigned char *srcp1, int src1_pitch, 
	const unsigned char *srcp2, int src2_pitch, int height, int width, int inc, 
	unsigned long &diff)
{
	if (inc == 1)
	{
		__asm
		{
			mov ebx,srcp1
			mov edx,srcp2
			mov ecx,width
			mov esi,height
			pxor mm2,mm2
			pxor mm3,mm3
yloopyv12:
			xor eax,eax
			align 16
xloopyv12:
			movq mm0,[ebx+eax]
			movq mm1,[ebx+eax+8]
			psadbw mm0,[edx+eax]
			psadbw mm1,[edx+eax+8]
			add eax,16
			paddd mm2,mm0
			paddd mm3,mm1
			cmp eax,ecx
			jl xloopyv12
			add ebx,src1_pitch
			add edx,src2_pitch
			dec esi
			jnz yloopyv12
			paddd mm2,mm3
			mov eax,diff
			movd [eax],mm2
			emms
		}
	}
	else
	{
		__asm
		{
			mov ebx,srcp1
			mov edx,srcp2
			mov ecx,width
			mov esi,height
			pxor mm4,mm4
			pxor mm5,mm5
			movq mm6,lumaMask
			movq mm7,lumaMask
yloopyuy2:
			xor eax,eax
			align 16
xloopyuy2:
			movq mm0,[ebx+eax]
			movq mm1,[ebx+eax+8]
			movq mm2,[edx+eax]
			movq mm3,[edx+eax+8]
			pand mm0,mm6
			pand mm1,mm7
			pand mm2,mm6
			pand mm3,mm7
			psadbw mm0,mm2
			psadbw mm1,mm3
			add eax,16
			paddd mm4,mm0
			paddd mm5,mm1
			cmp eax,ecx
			jl xloopyuy2
			add ebx,src1_pitch
			add edx,src2_pitch
			dec esi
			jnz yloopyuy2
			paddd mm4,mm5
			mov eax,diff
			movd [eax],mm4
			emms
		}
	}
}

void TDHelper::subtractFramesMMX(const unsigned char *srcp1, int src1_pitch, 
	const unsigned char *srcp2, int src2_pitch, int height, int width, int inc, 
	unsigned long &diff)
{
	if (inc == 1)
	{
		__asm
		{
			mov ebx,srcp1
			mov edx,srcp2
			mov ecx,width
			mov esi,height
			pxor mm6,mm6
			pxor mm7,mm7
yloopyv12:
			xor eax,eax
			align 16
xloopyv12:
			movq mm0,[ebx+eax]
			movq mm1,[ebx+eax+8]
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
			add eax,16
			paddd mm6,mm0
			paddd mm7,mm1
			cmp eax,ecx
			jl xloopyv12
			add ebx,src1_pitch
			add edx,src2_pitch
			dec esi
			jnz yloopyv12
			paddd mm6,mm7
			movq mm5,mm6
			psrlq mm6,32
			paddd mm5,mm6
			mov eax,diff
			movd [eax],mm5
			emms
		}
	}
	else
	{
		__asm
		{
			mov ebx,srcp1
			mov edx,srcp2
			mov ecx,width
			mov esi,height
			pxor mm6,mm6
			pxor mm7,mm7
yloopyuy2:
			xor eax,eax
			align 16
xloopyuy2:
			movq mm0,[ebx+eax]
			movq mm1,[ebx+eax+8]
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
			pand mm0,lumaMask
			pand mm1,lumaMask
			pxor mm4,mm4
			paddw mm0,mm1
			pxor mm5,mm5
			movq mm1,mm0
			punpcklwd mm0,mm4
			punpckhwd mm1,mm5
			add eax,16
			paddd mm6,mm0
			paddd mm7,mm1
			cmp eax,ecx
			jl xloopyuy2
			add ebx,src1_pitch
			add edx,src2_pitch
			dec esi
			jnz yloopyuy2
			paddd mm6,mm7
			movq mm5,mm6
			psrlq mm6,32
			paddd mm5,mm6
			mov eax,diff
			movd [eax],mm5
			emms
		}
	}
}

void TDHelper::blendFramesSSE2(const unsigned char *srcp1, int src1_pitch,
   const unsigned char *srcp2, int src2_pitch, unsigned char *dstp, int dst_pitch,
   int height, int width)
{
	__asm
	{
		mov ebx,srcp1
		mov edx,srcp2
		mov esi,dstp
		mov edi,height
		mov ecx,width
yloop:
		xor eax,eax
		align 16
xloop:
		movdqa xmm0,[ebx+eax]
		pavgb xmm0,[edx+eax]
		movdqa [esi+eax],xmm0
		add eax,16
		cmp eax,ecx
		jl xloop
		add ebx,src1_pitch
		add edx,src2_pitch
		add esi,dst_pitch
		dec edi
		jnz yloop
	}
}

void TDHelper::blendFramesISSE(const unsigned char *srcp1, int src1_pitch,
   const unsigned char *srcp2, int src2_pitch, unsigned char *dstp, int dst_pitch,
   int height, int width)
{
	if (width&15)
	{
		__asm
		{
			mov ebx,srcp1
			mov edx,srcp2
			mov esi,dstp
			mov edi,height
			mov ecx,width
	yloop8:
			xor eax,eax
			align 16
	xloop8:
			movq mm0,[ebx+eax]
			pavgb mm0,[edx+eax]
			movq [esi+eax],mm0
			add eax,8
			cmp eax,ecx
			jl xloop8
			add ebx,src1_pitch
			add edx,src2_pitch
			add esi,dst_pitch
			dec edi
			jnz yloop8
			emms
		}
	}
	else
	{
		__asm
		{
			mov ebx,srcp1
			mov edx,srcp2
			mov esi,dstp
			mov edi,height
			mov ecx,width
	yloop16:
			xor eax,eax
			align 16
	xloop16:
			movq mm0,[ebx+eax]
			movq mm1,[ebx+eax+8]
			pavgb mm0,[edx+eax]
			pavgb mm1,[edx+eax+8]
			movq [esi+eax],mm0
			movq [esi+eax+8],mm1
			add eax,16
			cmp eax,ecx
			jl xloop16
			add ebx,src1_pitch
			add edx,src2_pitch
			add esi,dst_pitch
			dec edi
			jnz yloop16
			emms
		}
	}
}

void TDHelper::blendFramesMMX(const unsigned char *srcp1, int src1_pitch,
   const unsigned char *srcp2, int src2_pitch, unsigned char *dstp, int dst_pitch,
   int height, int width)
{
	__asm
	{
		mov ebx,srcp1
		mov edx,srcp2
		mov esi,dstp
		mov edi,height
		mov ecx,width
		movq mm6,onesMask
		pxor mm7,mm7
yloop:
		xor eax,eax
		align 16
xloop:
		movq mm0,[ebx+eax]
		movq mm1,[edx+eax]
		movq mm2,mm0
		movq mm3,mm1
		punpcklbw mm0,mm7
		punpcklbw mm1,mm7
		punpckhbw mm2,mm7
		punpckhbw mm3,mm7
		paddusw mm0,mm1
		paddusw mm2,mm3
		paddusw mm0,mm6
		paddusw mm2,mm6
		psrlw mm0,1
		psrlw mm2,1
		packuswb mm0,mm2
		movq [esi+eax],mm0
		add eax,8
		cmp eax,ecx
		jl xloop
		add ebx,src1_pitch
		add edx,src2_pitch
		add esi,dst_pitch
		dec edi
		jnz yloop
		emms
	}
}