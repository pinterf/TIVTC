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

#include "FieldDiff.h"

FieldDiff::FieldDiff(PClip _child, int _nt, bool _chroma, bool _display, bool _debug,
	bool _sse, int _opt, IScriptEnvironment *env) : GenericVideoFilter(_child), nt(_nt), 
	chroma(_chroma), display(_display), debug(_debug), sse(_sse), opt(_opt)
{
	if (!vi.IsYV12() && !vi.IsYUY2())
		env->ThrowError("FieldDiff:  only YV12 and YUY2 input supported!");
	if (vi.height&1)
		env->ThrowError("FieldDiff:  height must be mod 2!");
	if (vi.height < 8)
		env->ThrowError("FieldDiff:  height must be at least 8!");
	if (opt < 0 || opt > 4)
		env->ThrowError("FieldDiff:  opt must be set to 0, 1, 2, 3, or 4!");
	nfrms = vi.num_frames-1;
	if (debug)
	{
		sprintf(buf,"FieldDiff:  %s by tritical\n", VERSION);
		OutputDebugString(buf);
	}
	child->SetCacheHints(CACHE_NOTHING, 0);
}

FieldDiff::~FieldDiff()
{
	/* nothing to free */
}

AVSValue FieldDiff::ConditionalFieldDiff(int n, IScriptEnvironment* env) 
{
	if (n < 0) n = 0;
	else if (n > nfrms) n = nfrms;
	__int64 diff = 0;
	if (sse) diff = getDiff_SSE(child->GetFrame(n, env), vi.IsYV12() ? 3 : 1, chroma, nt, opt, env);
	else diff = getDiff(child->GetFrame(n, env), vi.IsYV12() ? 3 : 1, chroma, nt, opt, env);
	if (debug)
	{
		if (sse) sprintf(buf,"FieldDiff:  Frame = %d  Diff = %I64d (sse)\n", n, diff);
		else sprintf(buf,"FieldDiff:  Frame = %d  Diff = %I64d (sad)\n", n, diff);
		OutputDebugString(buf);
	}
	return double(diff); // the value could be outside of int range and avsvalue doesn't
						 // support __int64... so convert it to float
}

PVideoFrame __stdcall FieldDiff::GetFrame(int n, IScriptEnvironment *env)
{
	if (n < 0) n = 0;
	else if (n > nfrms) n = nfrms;
	PVideoFrame src = child->GetFrame(n, env);
	__int64 diff = 0;
	if (sse) diff = getDiff_SSE(src, vi.IsYV12() ? 3 : 1, chroma, nt, opt, env);
	else diff = getDiff(src, vi.IsYV12() ? 3 : 1, chroma, nt, opt, env);
	if (debug)
	{
		if (sse) sprintf(buf,"FieldDiff:  Frame = %d  Diff = %I64d (sse)\n", n, diff);
		else sprintf(buf,"FieldDiff:  Frame = %d  Diff = %I64d (sad)\n", n, diff);
		OutputDebugString(buf);
	}
	if (display)
	{
		env->MakeWritable(&src);
		sprintf(buf,"FieldDiff %s by tritical", VERSION);
		if (vi.IsYV12()) TFM::DrawYV12(src, 0, 0, buf);
		else TFM::DrawYUY2(src, 0, 0, buf);
		if (sse) sprintf(buf,"Frame = %d  Diff = %I64d (sse)", n, diff);
		else sprintf(buf,"Frame = %d  Diff = %I64d (sad)", n, diff);
		if (vi.IsYV12()) TFM::DrawYV12(src, 0, 1, buf);
		else TFM::DrawYUY2(src, 0, 1, buf);
		return src;
	}
	return src;
}

__int64 FieldDiff::getDiff(PVideoFrame &src, int np, bool chromaIn, int ntIn, int opti, 
						   IScriptEnvironment *env)
{
	int b, x, y, plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
	const int stop = chromaIn ? np : 1;
	const int inc = (np == 1 && !chromaIn) ? 2 : 1;
	const unsigned char *srcp, *srcpp, *src2p, *srcpn, *src2n;
	int src_pitch, width, widtha, widtha1, widtha2, height, temp;
	__int64 diff = 0, nt64[2];
	if (ntIn > 255) ntIn = 255;
	else if (ntIn < 0) ntIn = 0;
	const int nt6 = ntIn*6;
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opti != 4)
	{
		if (opti == 0) cpu &= ~0x2C;
		else if (opti == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opti == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opti == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_MMX) || (cpu&CPUF_SSE2))
	{
		nt64[0] = (nt6<<16)+nt6;
		nt64[0] += (nt64[0]<<32);
		nt64[1] = nt64[0];
	}
	for (b=0; b<stop; ++b)
	{
		srcp = src->GetReadPtr(plane[b]);
		src_pitch = src->GetPitch(plane[b]);
		width = src->GetRowSize(plane[b]);
		widtha1 = (width>>3)<<3;
		widtha2 = (width>>4)<<4;
		height = src->GetHeight(plane[b]);
		src2p = srcp - src_pitch*2;
		srcpp = srcp - src_pitch;
		srcpn = srcp + src_pitch;
		src2n = srcp + src_pitch*2;
		for (x=0; x<width; x+=inc)
		{
			temp = abs((src2n[x]+(srcp[x]<<2)+src2n[x])-3*(srcpn[x]+srcpn[x]));
			if (temp > nt6) diff += temp;
		}
		src2p += src_pitch;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		src2n += src_pitch;
		for (x=0; x<width; x+=inc)
		{
			temp = abs((src2n[x]+(srcp[x]<<2)+src2n[x])-3*(srcpp[x]+srcpn[x]));
			if (temp > nt6) diff += temp;
		}
		src2p += src_pitch;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		src2n += src_pitch;
		if ((cpu&CPUF_SSE2) || (cpu&CPUF_MMX))
		{
			if ((cpu&CPUF_SSE2) && !((int(srcp)|src_pitch)&15) && widtha2 >= 16)
			{
				__m128 nt128;
				__asm
				{
					movups xmm1,xmmword ptr[nt64]
					movaps nt128,xmm1
				}
				if (inc == 1)
					calcFieldDiff_SAD_SSE2(src2p,src_pitch,widtha2,height-4,nt128,diff);
				else
					calcFieldDiff_SAD_SSE2_Luma(src2p,src_pitch,widtha2,height-4,nt128,diff);
				widtha = widtha2;
			}
			else if (cpu&CPUF_MMX)
			{
				if (inc == 1)
					calcFieldDiff_SAD_MMX(src2p,src_pitch,widtha1,height-4,nt64[0],diff);
				else
					calcFieldDiff_SAD_MMX_Luma(src2p,src_pitch,widtha1,height-4,nt64[0],diff);
				widtha = widtha1;
			}
			else env->ThrowError("FieldDiff:  internal error!");
			for (y=2; y<height-2; ++y)
			{
				for (x=widtha; x<width; x+=inc)
				{
					temp = abs((src2p[x]+(srcp[x]<<2)+src2n[x])-3*(srcpp[x]+srcpn[x]));
					if (temp > nt6) diff += temp;
				}
				src2p += src_pitch;
				srcpp += src_pitch;
				srcp += src_pitch;
				srcpn += src_pitch;
				src2n += src_pitch;
			}
		}
		else
		{
			for (y=2; y<height-2; ++y)
			{
				for (x=0; x<width; x+=inc)
				{
					temp = abs((src2p[x]+(srcp[x]<<2)+src2n[x])-3*(srcpp[x]+srcpn[x]));
					if (temp > nt6) diff += temp;
				}
				src2p += src_pitch;
				srcpp += src_pitch;
				srcp += src_pitch;
				srcpn += src_pitch;
				src2n += src_pitch;
			}
		}
		for (x=0; x<width; x+=inc)
		{
			temp = abs((src2p[x]+(srcp[x]<<2)+src2p[x])-3*(srcpp[x]+srcpn[x]));
			if (temp > nt6) diff += temp;
		}
		src2p += src_pitch;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		src2n += src_pitch;
		for (x=0; x<width; x+=inc)
		{
			temp = abs((src2p[x]+(srcp[x]<<2)+src2p[x])-3*(srcpp[x]+srcpp[x]));
			if (temp > nt6) diff += temp;
		}
	}
	return (diff/6);
}

__int64 FieldDiff::getDiff_SSE(PVideoFrame &src, int np, bool chromaIn, int ntIn, int opti,
							   IScriptEnvironment *env)
{
	int b, x, y, plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
	const int stop = chromaIn ? np : 1;
	const int inc = (np == 1 && !chromaIn) ? 2 : 1;
	const unsigned char *srcp, *srcpp, *src2p, *srcpn, *src2n;
	int src_pitch, width, widtha, widtha1, widtha2, height, temp;
	__int64 diff = 0, nt64[2];
	if (ntIn > 255) ntIn = 255;
	else if (ntIn < 0) ntIn = 0;
	const int nt6 = ntIn*6;
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opti != 4)
	{
		if (opti == 0) cpu &= ~0x2C;
		else if (opti == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opti == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opti == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_MMX) || (cpu&CPUF_SSE2))
	{
		nt64[0] = (nt6<<16)+nt6;
		nt64[0] += (nt64[0]<<32);
		nt64[1] = nt64[0];
	}
	for (b=0; b<stop; ++b)
	{
		srcp = src->GetReadPtr(plane[b]);
		src_pitch = src->GetPitch(plane[b]);
		width = src->GetRowSize(plane[b]);
		widtha1 = (width>>3)<<3;
		widtha2 = (width>>4)<<4;
		height = src->GetHeight(plane[b]);
		src2p = srcp - src_pitch*2;
		srcpp = srcp - src_pitch;
		srcpn = srcp + src_pitch;
		src2n = srcp + src_pitch*2;
		for (x=0; x<width; x+=inc)
		{
			temp = abs((src2n[x]+(srcp[x]<<2)+src2n[x])-3*(srcpn[x]+srcpn[x]));
			if (temp > nt6) diff += temp*temp;
		}
		src2p += src_pitch;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		src2n += src_pitch;
		for (x=0; x<width; x+=inc)
		{
			temp = abs((src2n[x]+(srcp[x]<<2)+src2n[x])-3*(srcpp[x]+srcpn[x]));
			if (temp > nt6) diff += temp*temp;
		}
		src2p += src_pitch;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		src2n += src_pitch;
		if ((cpu&CPUF_SSE2) || (cpu&CPUF_MMX))
		{
			if ((cpu&CPUF_SSE2) && !((int(srcp)|src_pitch)&15) && widtha2 >= 16)
			{
				__m128 nt128;
				__asm
				{
					movups xmm1,xmmword ptr[nt64]
					movaps nt128,xmm1
				}
				if (inc == 1) 
					calcFieldDiff_SSE_SSE2(src2p,src_pitch,widtha2,height-4,nt128,diff);
				else 
					calcFieldDiff_SSE_SSE2_Luma(src2p,src_pitch,widtha2,height-4,nt128,diff);
				widtha = widtha2;
			}
			else if (cpu&CPUF_MMX)
			{
				if (inc == 1)
					calcFieldDiff_SSE_MMX(src2p,src_pitch,widtha1,height-4,nt64[0],diff);
				else
					calcFieldDiff_SSE_MMX_Luma(src2p,src_pitch,widtha1,height-4,nt64[0],diff);
				widtha = widtha1;
			}
			else env->ThrowError("FieldDiff:  internal error!");
			for (y=2; y<height-2; ++y)
			{
				for (x=widtha; x<width; x+=inc)
				{
					temp = abs((src2p[x]+(srcp[x]<<2)+src2n[x])-3*(srcpp[x]+srcpn[x]));
					if (temp > nt6) diff += temp*temp;
				}
				src2p += src_pitch;
				srcpp += src_pitch;
				srcp += src_pitch;
				srcpn += src_pitch;
				src2n += src_pitch;
			}
		}
		else
		{
			for (y=2; y<height-2; ++y)
			{
				for (x=0; x<width; x+=inc)
				{
					temp = abs((src2p[x]+(srcp[x]<<2)+src2n[x])-3*(srcpp[x]+srcpn[x]));
					if (temp > nt6) diff += temp*temp;
				}
				src2p += src_pitch;
				srcpp += src_pitch;
				srcp += src_pitch;
				srcpn += src_pitch;
				src2n += src_pitch;
			}
		}
		for (x=0; x<width; x+=inc)
		{
			temp = abs((src2p[x]+(srcp[x]<<2)+src2p[x])-3*(srcpp[x]+srcpn[x]));
			if (temp > nt6) diff += temp*temp;
		}
		src2p += src_pitch;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		src2n += src_pitch;
		for (x=0; x<width; x+=inc)
		{
			temp = abs((src2p[x]+(srcp[x]<<2)+src2p[x])-3*(srcpp[x]+srcpp[x]));
			if (temp > nt6) diff += temp*temp;
		}
	}
	return (diff/6);
}

AVSValue __cdecl Create_CFieldDiff(AVSValue args, void* user_data, IScriptEnvironment* env) 
{
	AVSValue cnt = env->GetVar("current_frame");
	if (!cnt.IsInt())
		env->ThrowError("CFieldDiff:  This filter can only be used within ConditionalFilter!");
	int n = cnt.AsInt();
	FieldDiff *f = new FieldDiff(args[0].AsClip(),args[1].AsInt(3),args[2].AsBool(true),
		false,args[3].AsBool(false),args[4].AsBool(false),args[5].AsInt(4),env);
	AVSValue CFieldDiff = f->ConditionalFieldDiff(n, env);	
	delete f;
	return CFieldDiff;
}

AVSValue __cdecl Create_FieldDiff(AVSValue args, void* user_data, IScriptEnvironment* env) 
{
	return new FieldDiff(args[0].AsClip(),args[1].AsInt(3),args[2].AsBool(true),
		args[3].AsBool(false),args[4].AsBool(false),args[5].AsBool(false),
		args[6].AsInt(4),env);
}

__declspec(align(16)) const __int64 threeMask[2] = { 0x0003000300030003, 0x0003000300030003 };
__declspec(align(16)) const __int64 hdd_Mask[2] = { 0x00000000FFFFFFFF, 0x00000000FFFFFFFF };
__declspec(align(16)) const __int64 lumaWordMask[2] = { 0x0000FFFF0000FFFF, 0x0000FFFF0000FFFF };

void FieldDiff::calcFieldDiff_SAD_SSE2(const unsigned char *src2p, int src_pitch,
		int width, int height, __m128 nt, __int64 &diff)
{
	__asm
	{
		mov eax,src2p
		mov edx,src_pitch
		mov esi,eax
		add esi,edx
		lea edi,[esi+edx*2]
		pxor xmm7,xmm7
yloop:
		pxor xmm6,xmm6
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]	// src2p
		lea eax,[eax+edx*2]
		movdqa xmm1,[eax+ecx]	// srcp
		lea eax,[eax+edx*2]
		movdqa xmm2,[eax+ecx]	// src2n
		movdqa xmm3,xmm0
		movdqa xmm4,xmm1
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm1,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm4,xmm7
		punpckhbw xmm5,xmm7
		paddusw xmm0,xmm2
		paddusw xmm3,xmm5
		psllw xmm1,2
		psllw xmm4,2
		paddusw xmm1,xmm0
		paddusw xmm4,xmm3
		movdqa xmm0,[esi+ecx]	// srcpp
		movdqa xmm2,[edi+ecx]	// srcpn
		movdqa xmm3,xmm0
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm5,xmm7
		paddusw xmm0,xmm2
		paddusw xmm3,xmm5
		pmullw xmm0,threeMask
		pmullw xmm3,threeMask
		movdqa xmm2,xmm1
		movdqa xmm5,xmm4
		psubusw xmm1,xmm0
		psubusw xmm4,xmm3
		psubusw xmm0,xmm2
		psubusw xmm3,xmm5
		por xmm1,xmm0
		por xmm4,xmm3
		movdqa xmm0,xmm1
		movdqa xmm2,xmm4
		pcmpgtw xmm1,nt
		pcmpgtw xmm4,nt
		pand xmm0,xmm1
		pand xmm2,xmm4
		mov eax,esi
		paddusw xmm0,xmm2
		sub eax,edx
		movdqa xmm2,xmm0
		punpcklwd xmm0,xmm7
		punpckhwd xmm2,xmm7
		paddd xmm6,xmm0
		add ecx,16
		paddd xmm6,xmm2
		cmp ecx,width
		jl xloop
		mov ecx,diff
		movdqa xmm5,xmm6
		movq xmm4,qword ptr[ecx]
		punpckldq xmm6,xmm7
		punpckhdq xmm5,xmm7
		paddq xmm6,xmm5
		add eax,edx
		movdqa xmm5,xmm6
		add esi,edx
		psrldq xmm6,8
		add edi,edx
		paddq xmm5,xmm6
		paddq xmm5,xmm4
		movq qword ptr[ecx],xmm5
		dec height
		jnz yloop
	}
}

void FieldDiff::calcFieldDiff_SAD_MMX(const unsigned char *src2p, int src_pitch,
		int width, int height, __int64 nt, __int64 &diff)
{
	__asm
	{
		mov eax,src2p
		mov edx,src_pitch
		mov ebx,width
		mov esi,eax
		add esi,edx
		lea edi,[esi+edx*2]
		pxor mm7,mm7
yloop:
		pxor mm6,mm6
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]	// src2p
		lea eax,[eax+edx*2]
		movq mm1,[eax+ecx]	// srcp
		lea eax,[eax+edx*2]
		movq mm2,[eax+ecx]	// src2n
		movq mm3,mm0
		movq mm4,mm1
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm1,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm4,mm7
		punpckhbw mm5,mm7
		paddusw mm0,mm2
		paddusw mm3,mm5
		psllw mm1,2
		psllw mm4,2
		paddusw mm1,mm0
		paddusw mm4,mm3
		movq mm0,[esi+ecx]	// srcpp
		movq mm2,[edi+ecx]	// srcpn
		movq mm3,mm0
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm5,mm7
		paddusw mm0,mm2
		paddusw mm3,mm5
		pmullw mm0,threeMask
		pmullw mm3,threeMask
		movq mm2,mm1
		movq mm5,mm4
		psubusw mm1,mm0
		psubusw mm4,mm3
		psubusw mm0,mm2
		psubusw mm3,mm5
		por mm1,mm0
		por mm4,mm3
		movq mm0,mm1
		movq mm2,mm4
		pcmpgtw mm1,nt
		pcmpgtw mm4,nt
		pand mm0,mm1
		pand mm2,mm4
		mov eax,esi
		paddusw mm0,mm2
		sub eax,edx
		movq mm2,mm0
		punpcklwd mm0,mm7
		punpckhwd mm2,mm7
		paddd mm6,mm0
		add ecx,8
		paddd mm6,mm2
		cmp ecx,ebx
		jl xloop
		mov ecx,diff
		movq mm5,mm6
		psrlq mm6,32
		paddd mm5,mm6
		movd ebx,mm5
		xor edx,edx
		add ebx,[ecx]
		adc edx,[ecx+4]
		mov [ecx],ebx
		mov [ecx+4],edx
		mov ebx,width
		mov edx,src_pitch
		add eax,edx
		add esi,edx
		add edi,edx
		dec height
		jnz yloop
		emms
	}
}

void FieldDiff::calcFieldDiff_SAD_SSE2_Luma(const unsigned char *src2p, int src_pitch,
		int width, int height, __m128 nt, __int64 &diff)
{
	__asm
	{
		mov eax,src2p
		mov edx,src_pitch
		mov esi,eax
		add esi,edx
		lea edi,[esi+edx*2]
		pxor xmm7,xmm7
yloop:
		pxor xmm6,xmm6
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]	// src2p
		lea eax,[eax+edx*2]
		movdqa xmm1,[eax+ecx]	// srcp
		lea eax,[eax+edx*2]
		movdqa xmm2,[eax+ecx]	// src2n
		movdqa xmm3,xmm0
		movdqa xmm4,xmm1
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm1,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm4,xmm7
		punpckhbw xmm5,xmm7
		paddusw xmm0,xmm2
		paddusw xmm3,xmm5
		psllw xmm1,2
		psllw xmm4,2
		paddusw xmm1,xmm0
		paddusw xmm4,xmm3
		movdqa xmm0,[esi+ecx]	// srcpp
		movdqa xmm2,[edi+ecx]	// srcpn
		movdqa xmm3,xmm0
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm5,xmm7
		paddusw xmm0,xmm2
		paddusw xmm3,xmm5
		pmullw xmm0,threeMask
		pmullw xmm3,threeMask
		movdqa xmm2,xmm1
		movdqa xmm5,xmm4
		psubusw xmm1,xmm0
		psubusw xmm4,xmm3
		psubusw xmm0,xmm2
		psubusw xmm3,xmm5
		por xmm1,xmm0
		por xmm4,xmm3
		movdqa xmm0,xmm1
		movdqa xmm2,xmm4
		pcmpgtw xmm1,nt
		pcmpgtw xmm4,nt
		pand xmm0,xmm1
		pand xmm2,xmm4
		pand xmm0,lumaWordMask
		pand xmm2,lumaWordMask
		mov eax,esi
		paddusw xmm0,xmm2
		sub eax,edx
		movdqa xmm2,xmm0
		punpcklwd xmm0,xmm7
		punpckhwd xmm2,xmm7
		paddd xmm6,xmm0
		add ecx,16
		paddd xmm6,xmm2
		cmp ecx,width
		jl xloop
		mov ecx,diff
		movdqa xmm5,xmm6
		movq xmm4,qword ptr[ecx]
		punpckldq xmm6,xmm7
		punpckhdq xmm5,xmm7
		paddq xmm6,xmm5
		add eax,edx
		movdqa xmm5,xmm6
		add esi,edx
		psrldq xmm6,8
		add edi,edx
		paddq xmm5,xmm6
		paddq xmm5,xmm4
		movq qword ptr[ecx],xmm5
		dec height
		jnz yloop
	}
}

void FieldDiff::calcFieldDiff_SAD_MMX_Luma(const unsigned char *src2p, int src_pitch,
		int width, int height, __int64 nt, __int64 &diff)
{
	__asm
	{
		mov eax,src2p
		mov edx,src_pitch
		mov ebx,width
		mov esi,eax
		add esi,edx
		lea edi,[esi+edx*2]
		pxor mm7,mm7
yloop:
		pxor mm6,mm6
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]	// src2p
		lea eax,[eax+edx*2]
		movq mm1,[eax+ecx]	// srcp
		lea eax,[eax+edx*2]
		movq mm2,[eax+ecx]	// src2n
		movq mm3,mm0
		movq mm4,mm1
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm1,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm4,mm7
		punpckhbw mm5,mm7
		paddusw mm0,mm2
		paddusw mm3,mm5
		psllw mm1,2
		psllw mm4,2
		paddusw mm1,mm0
		paddusw mm4,mm3
		movq mm0,[esi+ecx]	// srcpp
		movq mm2,[edi+ecx]	// srcpn
		movq mm3,mm0
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm5,mm7
		paddusw mm0,mm2
		paddusw mm3,mm5
		pmullw mm0,threeMask
		pmullw mm3,threeMask
		movq mm2,mm1
		movq mm5,mm4
		psubusw mm1,mm0
		psubusw mm4,mm3
		psubusw mm0,mm2
		psubusw mm3,mm5
		por mm1,mm0
		por mm4,mm3
		movq mm0,mm1
		movq mm2,mm4
		pcmpgtw mm1,nt
		pcmpgtw mm4,nt
		pand mm0,mm1
		pand mm2,mm4
		pand mm0,lumaWordMask
		pand mm2,lumaWordMask
		mov eax,esi
		paddusw mm0,mm2
		sub eax,edx
		movq mm2,mm0
		punpcklwd mm0,mm7
		punpckhwd mm2,mm7
		paddd mm6,mm0
		add ecx,8
		paddd mm6,mm2
		cmp ecx,ebx
		jl xloop
		mov ecx,diff
		movq mm5,mm6
		psrlq mm6,32
		paddd mm5,mm6
		movd ebx,mm5
		xor edx,edx
		add ebx,[ecx]
		adc edx,[ecx+4]
		mov [ecx],ebx
		mov [ecx+4],edx
		mov ebx,width
		mov edx,src_pitch
		add eax,edx
		add esi,edx
		add edi,edx
		dec height
		jnz yloop
		emms
	}
}

void FieldDiff::calcFieldDiff_SSE_SSE2(const unsigned char *src2p, int src_pitch,
		int width, int height, __m128 nt, __int64 &diff)
{
	__asm
	{
		mov eax,src2p
		mov edx,src_pitch
		mov esi,eax
		add esi,edx
		lea edi,[esi+edx*2]
		pxor xmm7,xmm7
yloop:
		pxor xmm6,xmm6
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]	// src2p
		lea eax,[eax+edx*2]
		movdqa xmm1,[eax+ecx]	// srcp
		lea eax,[eax+edx*2]
		movdqa xmm2,[eax+ecx]	// src2n
		movdqa xmm3,xmm0
		movdqa xmm4,xmm1
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm1,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm4,xmm7
		punpckhbw xmm5,xmm7
		paddusw xmm0,xmm2
		paddusw xmm3,xmm5
		psllw xmm1,2
		psllw xmm4,2
		paddusw xmm1,xmm0
		paddusw xmm4,xmm3
		movdqa xmm0,[esi+ecx]	// srcpp
		movdqa xmm2,[edi+ecx]	// srcpn
		movdqa xmm3,xmm0
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm5,xmm7
		paddusw xmm0,xmm2
		paddusw xmm3,xmm5
		pmullw xmm0,threeMask
		pmullw xmm3,threeMask
		movdqa xmm2,xmm1
		movdqa xmm5,xmm4
		psubusw xmm1,xmm0
		psubusw xmm4,xmm3
		psubusw xmm0,xmm2
		psubusw xmm3,xmm5
		por xmm1,xmm0
		por xmm4,xmm3
		movdqa xmm0,xmm1
		movdqa xmm2,xmm4
		pcmpgtw xmm1,nt
		pcmpgtw xmm4,nt
		pand xmm0,xmm1
		pand xmm2,xmm4
		mov eax,esi
		pmaddwd xmm0,xmm0
		pmaddwd xmm2,xmm2
		sub eax,edx
		paddd xmm6,xmm0
		add ecx,16
		paddd xmm6,xmm2
		cmp ecx,width
		jl xloop
		mov ecx,diff
		movdqa xmm5,xmm6
		movq xmm4,qword ptr[ecx]
		punpckldq xmm6,xmm7
		punpckhdq xmm5,xmm7
		paddq xmm6,xmm5
		add eax,edx
		movdqa xmm5,xmm6
		add esi,edx
		psrldq xmm6,8
		add edi,edx
		paddq xmm5,xmm6
		paddq xmm5,xmm4
		movq qword ptr[ecx],xmm5
		dec height
		jnz yloop
	}
}

void FieldDiff::calcFieldDiff_SSE_MMX(const unsigned char *src2p, int src_pitch,
		int width, int height, __int64 nt, __int64 &diff)
{
	__asm
	{
		mov eax,src2p
		mov edx,src_pitch
		mov ebx,width
		mov esi,eax
		add esi,edx
		lea edi,[esi+edx*2]
		pxor mm7,mm7
yloop:
		pxor mm6,mm6
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]	// src2p
		lea eax,[eax+edx*2]
		movq mm1,[eax+ecx]	// srcp
		lea eax,[eax+edx*2]
		movq mm2,[eax+ecx]	// src2n
		movq mm3,mm0
		movq mm4,mm1
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm1,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm4,mm7
		punpckhbw mm5,mm7
		paddusw mm0,mm2
		paddusw mm3,mm5
		psllw mm1,2
		psllw mm4,2
		paddusw mm1,mm0
		paddusw mm4,mm3
		movq mm0,[esi+ecx]	// srcpp
		movq mm2,[edi+ecx]	// srcpn
		movq mm3,mm0
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm5,mm7
		paddusw mm0,mm2
		paddusw mm3,mm5
		pmullw mm0,threeMask
		pmullw mm3,threeMask
		movq mm2,mm1
		movq mm5,mm4
		psubusw mm1,mm0
		psubusw mm4,mm3
		psubusw mm0,mm2
		psubusw mm3,mm5
		por mm1,mm0
		por mm4,mm3
		movq mm0,mm1
		movq mm2,mm4
		pcmpgtw mm1,nt
		pcmpgtw mm4,nt
		pand mm0,mm1
		pand mm2,mm4
		mov eax,esi
		pmaddwd mm0,mm0
		pmaddwd mm2,mm2
		sub eax,edx
		paddd mm6,mm0
		add ecx,8
		paddd mm6,mm2
		cmp ecx,ebx
		jl xloop
		mov ecx,diff
		movq mm5,mm6
		psrlq mm6,32
		paddd mm5,mm6
		movd ebx,mm5
		xor edx,edx
		add ebx,[ecx]
		adc edx,[ecx+4]
		mov [ecx],ebx
		mov [ecx+4],edx
		mov ebx,width
		mov edx,src_pitch
		add eax,edx
		add esi,edx
		add edi,edx
		dec height
		jnz yloop
		emms
	}
}

void FieldDiff::calcFieldDiff_SSE_SSE2_Luma(const unsigned char *src2p, int src_pitch,
		int width, int height, __m128 nt, __int64 &diff)
{
	__asm
	{
		mov eax,src2p
		mov edx,src_pitch
		mov esi,eax
		add esi,edx
		lea edi,[esi+edx*2]
		pxor xmm7,xmm7
yloop:
		pxor xmm6,xmm6
		xor ecx,ecx
		align 16
xloop:
		movdqa xmm0,[eax+ecx]	// src2p
		lea eax,[eax+edx*2]
		movdqa xmm1,[eax+ecx]	// srcp
		lea eax,[eax+edx*2]
		movdqa xmm2,[eax+ecx]	// src2n
		movdqa xmm3,xmm0
		movdqa xmm4,xmm1
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm1,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm4,xmm7
		punpckhbw xmm5,xmm7
		paddusw xmm0,xmm2
		paddusw xmm3,xmm5
		psllw xmm1,2
		psllw xmm4,2
		paddusw xmm1,xmm0
		paddusw xmm4,xmm3
		movdqa xmm0,[esi+ecx]	// srcpp
		movdqa xmm2,[edi+ecx]	// srcpn
		movdqa xmm3,xmm0
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm5,xmm7
		paddusw xmm0,xmm2
		paddusw xmm3,xmm5
		pmullw xmm0,threeMask
		pmullw xmm3,threeMask
		movdqa xmm2,xmm1
		movdqa xmm5,xmm4
		psubusw xmm1,xmm0
		psubusw xmm4,xmm3
		psubusw xmm0,xmm2
		psubusw xmm3,xmm5
		por xmm1,xmm0
		por xmm4,xmm3
		movdqa xmm0,xmm1
		movdqa xmm2,xmm4
		pcmpgtw xmm1,nt
		pcmpgtw xmm4,nt
		pand xmm0,xmm1
		pand xmm2,xmm4
		pand xmm0,lumaWordMask
		pand xmm2,lumaWordMask
		mov eax,esi
		pmaddwd xmm0,xmm0
		pmaddwd xmm2,xmm2
		sub eax,edx
		paddd xmm6,xmm0
		add ecx,16
		paddd xmm6,xmm2
		cmp ecx,width
		jl xloop
		mov ecx,diff
		movdqa xmm5,xmm6
		movq xmm4,qword ptr[ecx]
		punpckldq xmm6,xmm7
		punpckhdq xmm5,xmm7
		paddq xmm6,xmm5
		add eax,edx
		movdqa xmm5,xmm6
		add esi,edx
		psrldq xmm6,8
		add edi,edx
		paddq xmm5,xmm6
		paddq xmm5,xmm4
		movq qword ptr[ecx],xmm5
		dec height
		jnz yloop
	}
}

void FieldDiff::calcFieldDiff_SSE_MMX_Luma(const unsigned char *src2p, int src_pitch,
		int width, int height, __int64 nt, __int64 &diff)
{
	__asm
	{
		mov eax,src2p
		mov edx,src_pitch
		mov ebx,width
		mov esi,eax
		add esi,edx
		lea edi,[esi+edx*2]
		pxor mm7,mm7
yloop:
		pxor mm6,mm6
		xor ecx,ecx
		align 16
xloop:
		movq mm0,[eax+ecx]	// src2p
		lea eax,[eax+edx*2]
		movq mm1,[eax+ecx]	// srcp
		lea eax,[eax+edx*2]
		movq mm2,[eax+ecx]	// src2n
		movq mm3,mm0
		movq mm4,mm1
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm1,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm4,mm7
		punpckhbw mm5,mm7
		paddusw mm0,mm2
		paddusw mm3,mm5
		psllw mm1,2
		psllw mm4,2
		paddusw mm1,mm0
		paddusw mm4,mm3
		movq mm0,[esi+ecx]	// srcpp
		movq mm2,[edi+ecx]	// srcpn
		movq mm3,mm0
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm5,mm7
		paddusw mm0,mm2
		paddusw mm3,mm5
		pmullw mm0,threeMask
		pmullw mm3,threeMask
		movq mm2,mm1
		movq mm5,mm4
		psubusw mm1,mm0
		psubusw mm4,mm3
		psubusw mm0,mm2
		psubusw mm3,mm5
		por mm1,mm0
		por mm4,mm3
		movq mm0,mm1
		movq mm2,mm4
		pcmpgtw mm1,nt
		pcmpgtw mm4,nt
		pand mm0,mm1
		pand mm2,mm4
		pand mm0,lumaWordMask
		pand mm2,lumaWordMask
		mov eax,esi
		pmaddwd mm0,mm0
		pmaddwd mm2,mm2
		sub eax,edx
		paddd mm6,mm0
		add ecx,8
		paddd mm6,mm2
		cmp ecx,ebx
		jl xloop
		mov ecx,diff
		movq mm5,mm6
		psrlq mm6,32
		paddd mm5,mm6
		movd ebx,mm5
		xor edx,edx
		add ebx,[ecx]
		adc edx,[ecx+4]
		mov [ecx],ebx
		mov [ecx+4],edx
		mov ebx,width
		mov edx,src_pitch
		add eax,edx
		add esi,edx
		add edi,edx
		dec height
		jnz yloop
		emms
	}
}