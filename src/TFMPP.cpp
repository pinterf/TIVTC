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

#include "TFMPP.h"

__declspec(align(16)) const __int64 onesMask[2] = { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF };
__declspec(align(16)) const __int64 onesByte[2] = { 0x0101010101010101, 0x0101010101010101 };
__declspec(align(16)) const __int64 twosByte[2] = { 0x0202020202020202, 0x0202020202020202 };
__declspec(align(16)) const __int64 twosWord[2] = { 0x0002000200020002, 0x0002000200020002 };
__declspec(align(16)) const __int64 foursByte[2] = { 0x0404040404040404, 0x0404040404040404 };
__declspec(align(16)) const __int64 eightsByte[2] = { 0x0808080808080808, 0x0808080808080808 };
__declspec(align(16)) const __int64 sixteensByte[2] = { 0x1010101010101010, 0x1010101010101010 };
__declspec(align(16)) const __int64 thirtytwosByte[2] = { 0x2020202020202020, 0x2020202020202020 };
__declspec(align(16)) const __int64 threeWord[2] = { 0x0003000300030003, 0x0003000300030003 };
__declspec(align(16)) const __int64 sixteenWord[2] = { 0x0010001000100010, 0x0010001000100010 };
__declspec(align(16)) const __int64 nineteenWord[2] = { 0x0013001300130013, 0x0013001300130013 };

PVideoFrame __stdcall TFMPP::GetFrame(int n, IScriptEnvironment *env)
{
	if (n < 0) n = 0;
	else if (n > nfrms) n = nfrms;
	bool combed;
	int fieldSrc, field;
	unsigned int hint;
	PVideoFrame src = child->GetFrame(n, env);
	bool res = getHint(src, fieldSrc, combed, hint);
	if (!combed) 
	{
		if (usehints || !res) return src;
		env->MakeWritable(&src);
		destroyHint(src, hint);
		return src;
	}
	int np = vi.IsYV12() ? 3 : 1;
	getSetOvr(n);
	PVideoFrame dst;
	if (PP > 4)
	{
		int use = 0;
		unsigned int hintt;
		PVideoFrame prv = child->GetFrame(n>0?n-1:0, env);
		getHint(prv, field, combed, hintt);
		if (!combed && field != -1 && n != 0) ++use;
		PVideoFrame nxt = child->GetFrame(n<nfrms?n+1:nfrms, env);
		getHint(nxt, field, combed, hintt);
		if (!combed && field != -1 && n != nfrms) use += 2;
		if (use > 0)
		{
			dst = env->NewVideoFrame(vi);
			buildMotionMask(prv, src, nxt, mmask, use, np, env);
			if (uC2) maskClip2(src, clip2->GetFrame(n, env), mmask, dst, np, env);
			else
			{
				if (PP == 5) BlendDeint(src, mmask, dst, false, np, env);
				else
				{
					if (PP == 6) 
					{
						copyField(dst, src, env, np, fieldSrc);
						CubicDeint(src, mmask, dst, false, fieldSrc, np, env);
					}
					else 
					{
						copyFrame(dst, src, env, np);
						elaDeint(dst, mmask, src, false, fieldSrc, np);
					}
				}
			}
		}
		else
		{
			if (uC2)
			{
				dst = clip2->GetFrame(n, env);
				env->MakeWritable(&dst);
			}
			else
			{
				dst = env->NewVideoFrame(vi);
				if (PP == 5) BlendDeint(src, mmask, dst, true, np, env);
				else
				{
					if (PP == 6) 
					{
						copyField(dst, src, env, np, fieldSrc);
						CubicDeint(src, mmask, dst, true, fieldSrc, np, env);
					}
					else 
					{
						copyFrame(dst, src, env, np);
						elaDeint(dst, mmask, src, true, fieldSrc, np);
					}
				}
			}
		}
	}
	else
	{
		if (uC2) 
		{
			dst = clip2->GetFrame(n, env);
			env->MakeWritable(&dst);
		}
		else
		{
			dst = env->NewVideoFrame(vi);
			if (PP == 2) BlendDeint(src, mmask, dst, true, np, env);
			else 
			{
				if (PP == 3) 
				{
					copyField(dst, src, env, np, fieldSrc);
					CubicDeint(src, mmask, dst, true, fieldSrc, np, env);
				}
				else 
				{	
					copyFrame(dst, src, env, np);
					elaDeint(dst, mmask, src, true, fieldSrc, np);
				}
			}
		}
	}
	if (display) writeDisplay(dst, np, n, fieldSrc);
	if (usehints) putHint(dst, fieldSrc, hint);
	else destroyHint(dst, hint);
	return dst;
}

void TFMPP::buildMotionMask(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, 
		PlanarFrame *mask, int use, int np, IScriptEnvironment *env)
{
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	for (int b=0; b<np; ++b)
	{
		int plane;
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_U;
		else plane = PLANAR_V;
		const unsigned char *prvpp = prv->GetReadPtr(plane);
		const int prv_pitch = prv->GetPitch(plane);
		const unsigned char *prvp = prvpp + prv_pitch;
		const unsigned char *prvpn = prvp + prv_pitch;
		const unsigned char *srcpp = src->GetReadPtr(plane);
		const int src_pitch = src->GetPitch(plane);
		const int width = src->GetRowSize(plane);
		const int height = src->GetHeight(plane);
		const unsigned char *srcp = srcpp + src_pitch;
		const unsigned char *srcpn = srcp + src_pitch;
		const unsigned char *nxtpp = nxt->GetReadPtr(plane);
		const int nxt_pitch = nxt->GetPitch(plane);
		const unsigned char *nxtp = nxtpp + nxt_pitch;
		const unsigned char *nxtpn = nxtp + nxt_pitch;
		unsigned char *maskw = mask->GetPtr(b);
		const int msk_pitch = mask->GetPitch(b);
		maskw += msk_pitch;
		if (use == 1)
		{
			if ((cpu&CPUF_SSE2) && !((int(srcp)|int(prvp)|int(maskw)|src_pitch|prv_pitch|msk_pitch)&15))
				buildMotionMask1_SSE2(srcp,prvp,maskw,src_pitch,prv_pitch,msk_pitch,width,height-2,cpu);
			else if (cpu&CPUF_MMX)
				buildMotionMask1_MMX(srcp,prvp,maskw,src_pitch,prv_pitch,msk_pitch,width,height-2,cpu);
			else
			{
				fmemset(env->GetCPUFlags(),maskw-msk_pitch,msk_pitch*height,opt,0xFF);
				for (int y=1; y<height-1; ++y)
				{
					for (int x=0; x<width; ++x) 
					{
						if (!(abs(prvpp[x] - srcpp[x]) > mthresh || abs(prvp[x] - srcp[x]) > mthresh || 
							abs(prvpn[x] - srcpn[x]) > mthresh)) maskw[x] = 0;
					}
					prvpp += prv_pitch;
					prvp += prv_pitch;
					prvpn += prv_pitch;
					srcpp += src_pitch;
					srcp += src_pitch;
					srcpn += src_pitch;
					maskw += msk_pitch;
				}
			}
		}
		else if (use == 2)
		{
			if ((cpu&CPUF_SSE2) && !((int(srcp)|int(nxtp)|int(maskw)|src_pitch|nxt_pitch|msk_pitch)&15))
				buildMotionMask1_SSE2(srcp,nxtp,maskw,src_pitch,nxt_pitch,msk_pitch,width,height-2,cpu);
			else if (cpu&CPUF_MMX)
				buildMotionMask1_MMX(srcp,nxtp,maskw,src_pitch,nxt_pitch,msk_pitch,width,height-2,cpu);
			else
			{
				fmemset(env->GetCPUFlags(),maskw-msk_pitch,msk_pitch*height,opt,0xFF);
				for (int y=1; y<height-1; ++y)
				{
					for (int x=0; x<width; ++x) 
					{
						if (!(abs(nxtpp[x] - srcpp[x]) > mthresh || abs(nxtp[x] - srcp[x]) > mthresh || 
							abs(nxtpn[x] - srcpn[x]) > mthresh)) maskw[x] = 0;
					}
					srcpp += src_pitch;
					srcp += src_pitch;
					srcpn += src_pitch;
					nxtpp += nxt_pitch;
					nxtp += nxt_pitch;
					nxtpn += nxt_pitch;
					maskw += msk_pitch;
				}
			}
		}
		else
		{
			if ((cpu&CPUF_SSE2) && !((int(prvp)|int(srcp)|int(nxtp)|int(maskw)|prv_pitch|src_pitch|nxt_pitch|msk_pitch)&15))
			{
				buildMotionMask2_SSE2(prvp,srcp,nxtp,maskw,prv_pitch,src_pitch,nxt_pitch,msk_pitch,width,height-2,cpu);
				for (int y=1; y<height; ++y)
				{
					for (int x=0; x<width; ++x)
					{
						if (!maskw[x]) continue;
						if (((maskw[x]&0x8) && (maskw[x]&0x15)) ||
							((maskw[x]&0x4) && (maskw[x]&0x2A)) ||
							((maskw[x]&0x22) && ((maskw[x]&0x11) == 0x11)) ||
							((maskw[x]&0x11) && ((maskw[x]&0x22) == 0x22)))
							maskw[x] = 0xFF;
						else maskw[x] = 0;
					}
					maskw += msk_pitch;
				}
			}
			else if (cpu&CPUF_MMX)
			{
				buildMotionMask2_MMX(prvp,srcp,nxtp,maskw,prv_pitch,src_pitch,nxt_pitch,msk_pitch,width,height-2,cpu);
				for (int y=1; y<height; ++y)
				{
					for (int x=0; x<width; ++x)
					{
						if (!maskw[x]) continue;
						if (((maskw[x]&0x8) && (maskw[x]&0x15)) ||
							((maskw[x]&0x4) && (maskw[x]&0x2A)) ||
							((maskw[x]&0x22) && ((maskw[x]&0x11) == 0x11)) ||
							((maskw[x]&0x11) && ((maskw[x]&0x22) == 0x22)))
							maskw[x] = 0xFF;
						else maskw[x] = 0;
					}
					maskw += msk_pitch;
				}
			}
			else
			{
				fmemset(env->GetCPUFlags(),maskw-msk_pitch,msk_pitch*height,opt,0xFF);
				for (int y=1; y<height-1; ++y)
				{
					for (int x=0; x<width; ++x) 
					{
						if (!(((abs(prvp[x] - srcp[x]) > mthresh) && (abs(nxtpp[x] - srcpp[x]) > mthresh || 
							abs(nxtp[x] - srcp[x]) > mthresh || abs(nxtpn[x] - srcpn[x]) > mthresh)) || 
							((abs(nxtp[x] - srcp[x]) > mthresh) && (abs(prvpp[x] - srcpp[x]) > mthresh || 
							abs(prvp[x] - srcp[x]) > mthresh || abs(prvpn[x] - srcpn[x]) > mthresh)) || 
							(abs(prvpp[x] - srcpp[x]) > mthresh && abs(prvpn[x] - srcpn[x]) > mthresh && 
							(abs(nxtpp[x] - srcpp[x]) > mthresh || abs(nxtpn[x] - srcpn[x]) > mthresh)) ||
							((abs(prvpp[x] - srcpp[x]) > mthresh || abs(prvpn[x] - srcpn[x]) > mthresh) && 
							abs(nxtpp[x] - srcpp[x]) > mthresh && abs(nxtpn[x] - srcpn[x]) > mthresh)))
							maskw[x] = 0;
					}
					prvpp += prv_pitch;
					prvp += prv_pitch;
					prvpn += prv_pitch;
					srcpp += src_pitch;
					srcp += src_pitch;
					srcpn += src_pitch;
					nxtpp += nxt_pitch;
					nxtp += nxt_pitch;
					nxtpn += nxt_pitch;
					maskw += msk_pitch;
				}
			}
		}
	}
	if (np == 3)
	{
		denoiseYV12(mask);
		linkYV12(mask);
	}
	else
	{
		denoiseYUY2(mask);
		linkYUY2(mask);
	}
}

void TFMPP::buildMotionMask1_SSE2(const unsigned char *srcp1, const unsigned char *srcp2,
		unsigned char *dstp, int s1_pitch, int s2_pitch, int dst_pitch, int width, 
		int height, long cpu)
{
	__int64 thresh[2];
	thresh[0] = max(min(255-mthresh-1,255),0);
	thresh[0] += (thresh[0]<<8);
	thresh[0] += (thresh[0]<<48)+(thresh[0]<<32)+(thresh[0]<<16);
	thresh[1] = thresh[0];
	fmemset(cpu,dstp-dst_pitch,dst_pitch,opt,0xFF);
	fmemset(cpu,dstp+dst_pitch*height,dst_pitch,opt,0xFF);
	__asm
	{
		mov ebx,srcp1
		mov edx,srcp2
		add ebx,s1_pitch
		add edx,s2_pitch
		mov esi,dstp
		mov ecx,width
		mov edi,s1_pitch
		movdqu xmm5,thresh
		pcmpeqb xmm6,xmm6
yloop:
		xor eax,eax
		align 16
xloop:
		movdqa xmm0,[ebx+eax]
		movdqa xmm1,[edx+eax]
		sub ebx,edi
		movdqa xmm2,xmm0
		sub edx,s2_pitch
		psubusb xmm0,xmm1
		psubusb xmm1,xmm2
		movdqa xmm2,[ebx+eax]
		movdqa xmm3,[edx+eax]
		por xmm0,xmm1
		movdqa xmm4,xmm2
		sub ebx,edi
		psubusb xmm2,xmm3
		sub edx,s2_pitch
		psubusb xmm3,xmm4
		por xmm2,xmm3
		movdqa xmm1,[ebx+eax]
		movdqa xmm3,[edx+eax]
		movdqa xmm4,xmm1
		psubusb xmm1,xmm3
		psubusb xmm3,xmm4
		lea ebx,[ebx+edi*2]
		por xmm1,xmm3
		paddusb xmm0,xmm5
		paddusb xmm1,xmm5
		paddusb xmm2,xmm5
		pcmpeqb xmm0,xmm6
		pcmpeqb xmm1,xmm6
		pcmpeqb xmm2,xmm6
		por xmm0,xmm1
		mov edi,s2_pitch
		por xmm0,xmm2
		lea edx,[edx+edi*2]
		movdqa [esi+eax],xmm0
		add eax,16
		mov edi,s1_pitch
		cmp eax,ecx
		jl xloop
		add ebx,s1_pitch
		add edx,s2_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
	}
}

void TFMPP::buildMotionMask1_MMX(const unsigned char *srcp1, const unsigned char *srcp2,
		unsigned char *dstp, int s1_pitch, int s2_pitch, int dst_pitch, int width, 
		int height, long cpu)
{
	__int64 thresh;
	thresh = max(min(255-mthresh-1,255),0);
	thresh += (thresh<<8);
	thresh += (thresh<<48)+(thresh<<32)+(thresh<<16);
	fmemset(cpu,dstp-dst_pitch,dst_pitch,opt,0xFF);
	fmemset(cpu,dstp+dst_pitch*height,dst_pitch,opt,0xFF);
	__asm
	{
		mov ebx,srcp1
		mov edx,srcp2
		add ebx,s1_pitch
		add edx,s2_pitch
		mov esi,dstp
		mov ecx,width
		mov edi,s1_pitch
		movq mm5,thresh
		pcmpeqb mm6,mm6
yloop:
		xor eax,eax
		align 16
xloop:
		movq mm0,[ebx+eax]
		movq mm1,[edx+eax]
		sub ebx,edi
		movq mm2,mm0
		sub edx,s2_pitch
		psubusb mm0,mm1
		psubusb mm1,mm2
		movq mm2,[ebx+eax]
		movq mm3,[edx+eax]
		por mm0,mm1
		movq mm4,mm2
		sub ebx,edi
		psubusb mm2,mm3
		sub edx,s2_pitch
		psubusb mm3,mm4
		por mm2,mm3
		movq mm1,[ebx+eax]
		movq mm3,[edx+eax]
		movq mm4,mm1
		psubusb mm1,mm3
		psubusb mm3,mm4
		lea ebx,[ebx+edi*2]
		por mm1,mm3
		paddusb mm0,mm5
		paddusb mm1,mm5
		paddusb mm2,mm5
		pcmpeqb mm0,mm6
		pcmpeqb mm1,mm6
		pcmpeqb mm2,mm6
		por mm0,mm1
		mov edi,s2_pitch
		por mm0,mm2
		lea edx,[edx+edi*2]
		movq [esi+eax],mm0
		add eax,8
		mov edi,s1_pitch
		cmp eax,ecx
		jl xloop
		add ebx,s1_pitch
		add edx,s2_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
		emms
	}
}

void TFMPP::buildMotionMask2_SSE2(const unsigned char *srcp1, const unsigned char *srcp2,
		const unsigned char *srcp3, unsigned char *dstp, int s1_pitch, int s2_pitch, 
		int s3_pitch, int dst_pitch, int width, int height, long cpu)
{
	__int64 thresh[2];
	thresh[0] = max(min(255-mthresh-1,255),0);
	thresh[0] += (thresh[0]<<8);
	thresh[0] += (thresh[0]<<48)+(thresh[0]<<32)+(thresh[0]<<16);
	thresh[1] = thresh[0];
	fmemset(cpu,dstp-dst_pitch,dst_pitch,opt,0xFF);
	fmemset(cpu,dstp+dst_pitch*height,dst_pitch,opt,0xFF);
	__asm
	{
		mov ebx,srcp1
		mov edx,srcp2
		mov edi,srcp3
		add ebx,s1_pitch
		add edx,s2_pitch
		add edi,s3_pitch
		mov esi,dstp
		movdqu xmm7,thresh
yloop:
		xor eax,eax
		align 16
xloop:
		movdqa xmm0,[ebx+eax]
		movdqa xmm1,[edx+eax]
		movdqa xmm2,[edi+eax]
		movdqa xmm3,xmm0
		movdqa xmm4,xmm1
		movdqa xmm5,xmm2
		psubusb xmm3,xmm1 // prv-src
		psubusb xmm5,xmm4 // nxt-src
		psubusb xmm1,xmm0 // src-prv
		psubusb xmm4,xmm2 // src-nxt
		por xmm1,xmm3
		por xmm4,xmm5
		pcmpeqb xmm0,xmm0
		sub ebx,s1_pitch
		paddusb xmm1,xmm7
		paddusb xmm4,xmm7
		pcmpeqb xmm1,xmm0
		pcmpeqb xmm4,xmm0
		sub edx,s2_pitch
		pand xmm1,onesByte
		pand xmm4,twosByte
		sub edi,s3_pitch
		por xmm1,xmm4
		movdqa xmm0,[ebx+eax]
		movdqa xmm2,[edx+eax]
		movdqa xmm3,[edi+eax]
		movdqa xmm4,xmm0
		movdqa xmm5,xmm2
		movdqa xmm6,xmm3
		psubusb xmm4,xmm2 // prv-src
		psubusb xmm6,xmm5 // nxt-src
		psubusb xmm2,xmm0 // src-prv
		psubusb xmm5,xmm3 // src-nxt
		por xmm2,xmm4
		por xmm5,xmm6
		pcmpeqb xmm0,xmm0
		sub ebx,s1_pitch
		paddusb xmm2,xmm7
		paddusb xmm5,xmm7
		pcmpeqb xmm2,xmm0
		pcmpeqb xmm5,xmm0
		pand xmm2,foursByte
		pand xmm5,eightsByte
		sub edx,s2_pitch
		por xmm1,xmm2
		sub edi,s3_pitch
		por xmm1,xmm5
		movdqa xmm0,[ebx+eax]
		movdqa xmm2,[edx+eax]
		movdqa xmm3,[edi+eax]
		movdqa xmm4,xmm0
		movdqa xmm5,xmm2
		movdqa xmm6,xmm3
		psubusb xmm4,xmm2 // prv-src
		psubusb xmm6,xmm5 // nxt-src
		psubusb xmm2,xmm0 // src-prv
		psubusb xmm5,xmm3 // src-nxt
		por xmm2,xmm4
		por xmm5,xmm6
		mov ecx,s1_pitch
		pcmpeqb xmm0,xmm0
		lea edx,[edx+ecx*2]
		paddusb xmm2,xmm7
		paddusb xmm5,xmm7
		pcmpeqb xmm2,xmm0
		pcmpeqb xmm5,xmm0
		mov ecx,s2_pitch
		pand xmm2,sixteensByte
		lea ebx,[ebx+ecx*2]
		pand xmm5,thirtytwosByte
		por xmm1,xmm2
		mov ecx,s3_pitch
		por xmm1,xmm5
		lea edi,[edi+ecx*2]
		movdqa [esi+eax],xmm1
		add eax,16
		cmp eax,width
		jl xloop
		add ebx,s1_pitch
		add edx,s2_pitch
		add edi,s3_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
	}
}

void TFMPP::buildMotionMask2_MMX(const unsigned char *srcp1, const unsigned char *srcp2,
		const unsigned char *srcp3, unsigned char *dstp, int s1_pitch, int s2_pitch, 
		int s3_pitch, int dst_pitch, int width, int height, long cpu)
{
	__int64 thresh;
	thresh = max(min(255-mthresh-1,255),0);
	thresh += (thresh<<8);
	thresh += (thresh<<48)+(thresh<<32)+(thresh<<16);
	fmemset(cpu,dstp-dst_pitch,dst_pitch,opt,0xFF);
	fmemset(cpu,dstp+dst_pitch*height,dst_pitch,opt,0xFF);
	__asm
	{
		mov ebx,srcp1
		mov edx,srcp2
		mov edi,srcp3
		add ebx,s1_pitch
		add edx,s2_pitch
		add edi,s3_pitch
		mov esi,dstp
		movq mm7,thresh
yloop:
		xor eax,eax
		align 16
xloop:
		movq mm0,[ebx+eax]
		movq mm1,[edx+eax]
		movq mm2,[edi+eax]
		movq mm3,mm0
		movq mm4,mm1
		movq mm5,mm2
		psubusb mm3,mm1 // prv-src
		psubusb mm5,mm4 // nxt-src
		psubusb mm1,mm0 // src-prv
		psubusb mm4,mm2 // src-nxt
		por mm1,mm3
		por mm4,mm5
		pcmpeqb mm0,mm0
		sub ebx,s1_pitch
		paddusb mm1,mm7
		paddusb mm4,mm7
		pcmpeqb mm1,mm0
		pcmpeqb mm4,mm0
		sub edx,s2_pitch
		pand mm1,onesByte
		pand mm4,twosByte
		sub edi,s3_pitch
		por mm1,mm4
		movq mm0,[ebx+eax]
		movq mm2,[edx+eax]
		movq mm3,[edi+eax]
		movq mm4,mm0
		movq mm5,mm2
		movq mm6,mm3
		psubusb mm4,mm2 // prv-src
		psubusb mm6,mm5 // nxt-src
		psubusb mm2,mm0 // src-prv
		psubusb mm5,mm3 // src-nxt
		por mm2,mm4
		por mm5,mm6
		pcmpeqb mm0,mm0
		sub ebx,s1_pitch
		paddusb mm2,mm7
		paddusb mm5,mm7
		pcmpeqb mm2,mm0
		pcmpeqb mm5,mm0
		pand mm2,foursByte
		pand mm5,eightsByte
		sub edx,s2_pitch
		por mm1,mm2
		sub edi,s3_pitch
		por mm1,mm5
		movq mm0,[ebx+eax]
		movq mm2,[edx+eax]
		movq mm3,[edi+eax]
		movq mm4,mm0
		movq mm5,mm2
		movq mm6,mm3
		psubusb mm4,mm2 // prv-src
		psubusb mm6,mm5 // nxt-src
		psubusb mm2,mm0 // src-prv
		psubusb mm5,mm3 // src-nxt
		por mm2,mm4
		por mm5,mm6
		mov ecx,s1_pitch
		pcmpeqb mm0,mm0
		lea edx,[edx+ecx*2]
		paddusb mm2,mm7
		paddusb mm5,mm7
		pcmpeqb mm2,mm0
		pcmpeqb mm5,mm0
		mov ecx,s2_pitch
		pand mm2,sixteensByte
		lea ebx,[ebx+ecx*2]
		pand mm5,thirtytwosByte
		por mm1,mm2
		mov ecx,s3_pitch
		por mm1,mm5
		lea edi,[edi+ecx*2]
		movq [esi+eax],mm1
		add eax,8
		cmp eax,width
		jl xloop
		add ebx,s1_pitch
		add edx,s2_pitch
		add edi,s3_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
		emms
	}
}

void TFMPP::denoiseYUY2(PlanarFrame *mask) 
{
	unsigned char *maskw = mask->GetPtr();
	const int mask_pitch = mask->GetPitch();
	const int Height = mask->GetHeight();
	const int Width = mask->GetWidth();
	unsigned char *maskwp = maskw - mask_pitch;
	unsigned char *maskwn = maskw + mask_pitch;
	for (int y=1; y<Height-1; ++y)
	{
		maskwp += mask_pitch;
		maskw += mask_pitch;
		maskwn += mask_pitch;
		for (int x=4; x<Width-4; ++x)
		{
			if (maskw[x] == 0xFF)
			{
				if (maskwp[x-2] == 0xFF) goto check_chroma;
				if (maskwp[x] == 0xFF) goto check_chroma;
				if (maskwp[x+2] == 0xFF) goto check_chroma;
				if (maskw[x-2] == 0xFF) goto check_chroma;
				if (maskw[x+2] == 0xFF) goto check_chroma;
				if (maskwn[x-2] == 0xFF) goto check_chroma;
				if (maskwn[x] == 0xFF) goto check_chroma;
				if (maskwn[x+2] == 0xFF) goto check_chroma;
				maskw[x] = 0;
			}
check_chroma:
			++x;
			if (maskw[x] == 0xFF)
			{
				if (maskwp[x-4] == 0xFF) continue;
				if (maskwp[x] == 0xFF) continue;
				if (maskwp[x+4] == 0xFF) continue;
				if (maskw[x-4] == 0xFF) continue;
				if (maskw[x+4] == 0xFF) continue;
				if (maskwn[x-4] == 0xFF) continue;
				if (maskwn[x] == 0xFF) continue;
				if (maskwn[x+4] == 0xFF) continue;
				maskw[x] = 0;
			}
		}
	}
}

void TFMPP::linkYUY2(PlanarFrame *mask) 
{
	unsigned char *maskw = mask->GetPtr();
	const int mask_pitch = mask->GetPitch();
	const int Height = mask->GetHeight();
	const int Width = mask->GetWidth()>>2;
	for (int y=1; y<Height-1; ++y)
	{
		maskw += mask_pitch;
		for (int x=0; x<Width; ++x)
		{
			if ((((unsigned int*)maskw)[x]&0x00FF00FF) == 0x00FF00FF)
			{
				((unsigned int*)maskw)[x] = 0xFFFFFFFF;
			}
		}
	}
}

void TFMPP::denoiseYV12(PlanarFrame *mask) 
{
	for (int b=0; b<3; ++b)
	{
		int plane;
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_V;
		else plane = PLANAR_U;
		unsigned char *maskpp = mask->GetPtr(b);
		const int msk_pitch = mask->GetPitch(b);
		unsigned char *maskp = maskpp + msk_pitch;
		unsigned char *maskpn = maskp + msk_pitch;
		const int Height = mask->GetHeight(b);
		const int Width = mask->GetWidth(b);
		for (int y=1; y<Height-1; ++y)
		{
			for (int x=1; x<Width-1; ++x)
			{
				if (maskp[x] == 0xFF)
				{
					if (maskpp[x-1] == 0xFF) continue;
					if (maskpp[x] == 0xFF) continue;
					if (maskpp[x+1] == 0xFF) continue;
					if (maskp[x-1] == 0xFF) continue;
					if (maskp[x+1] == 0xFF) continue;
					if (maskpn[x-1] == 0xFF) continue;
					if (maskpn[x] == 0xFF) continue;
					if (maskpn[x+1] == 0xFF) continue;
					maskp[x] = 0;
				}
			}
			maskpp += msk_pitch;
			maskp += msk_pitch;
			maskpn += msk_pitch;
		}
	}
}

void TFMPP::linkYV12(PlanarFrame *mask) 
{
	unsigned char *maskpY = mask->GetPtr(0);
	unsigned char *maskpV = mask->GetPtr(2);
	unsigned char *maskpU = mask->GetPtr(1);
	const int mask_pitchY = mask->GetPitch(0)<<1;
	const int mask_pitchUV = mask->GetPitch(2);
	const int HeightUV = mask->GetHeight(2);
	const int WidthUV = mask->GetWidth(2);
	unsigned char *maskppY = maskpY - (mask_pitchY>>1);
	unsigned char *maskpnY = maskpY + (mask_pitchY>>1);
	unsigned char *maskpnnY = maskpnY + (mask_pitchY>>1);
	for (int y=1; y<HeightUV-1; ++y)
	{
		maskppY = maskpnY;
		maskpY = maskpnnY;
		maskpnY += mask_pitchY;
		maskpnnY += mask_pitchY;
		maskpV += mask_pitchUV;
		maskpU += mask_pitchUV;
		for (int x=0; x<WidthUV; ++x)
		{
			if ((((unsigned short*)maskpY)[x] == (unsigned short) 0xFFFF) && 
				(((unsigned short*)maskpnY)[x] == (unsigned short) 0xFFFF) && 
				(((y&1) && (((unsigned short*)maskppY)[x] == (unsigned short) 0xFFFF)) || 
				(!(y&1) && (((unsigned short*)maskpnnY)[x] == (unsigned short) 0xFFFF))))
			{
				maskpV[x] = maskpU[x] = 0xFF;
			}
		}
	}
}

void TFMPP::BlendDeint(PVideoFrame &src, PlanarFrame *mask, PVideoFrame &dst, bool nomask, 
					   int np, IScriptEnvironment *env)
{
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	for (int b=0; b<np; ++b)
	{
		int plane;
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_U;
		else plane = PLANAR_V;
		const unsigned char *srcp = src->GetReadPtr(plane);
		const int src_pitch = src->GetPitch(plane);
		const int width = src->GetRowSize(plane);
		const int widtha = src->GetRowSize(plane+8);
		const int height = src->GetHeight(plane);
		const unsigned char *srcpp = srcp - src_pitch;
		const unsigned char *srcpn = srcp + src_pitch;
		unsigned char *dstp = dst->GetWritePtr(plane);
		const int dst_pitch = dst->GetPitch(plane);
		const unsigned char *maskp = mask->GetPtr(b);
		const int msk_pitch = mask->GetPitch(b);
		for (int x=0; x<width; ++x) 
			dstp[x] = (srcp[x]+srcpn[x]+1)>>1;
		srcpp += src_pitch;
		srcp += src_pitch;
		srcpn += src_pitch;
		dstp += dst_pitch;
		maskp += msk_pitch;
		if (nomask)
		{
			if ((cpu&CPUF_SSE2) && !((int(srcp)|int(dstp)|src_pitch|dst_pitch)&15))
			{
				blendDeint_SSE2(srcp, dstp, src_pitch, dst_pitch, widtha, height-2);
				srcpp += src_pitch*(height-2);
				srcp += src_pitch*(height-2);
				srcpn += src_pitch*(height-2);
				dstp += dst_pitch*(height-2);
				maskp += msk_pitch*(height-2);
			}
			else if (cpu&CPUF_MMX)
			{
				blendDeint_MMX(srcp, dstp, src_pitch, dst_pitch, widtha, height-2);
				srcpp += src_pitch*(height-2);
				srcp += src_pitch*(height-2);
				srcpn += src_pitch*(height-2);
				dstp += dst_pitch*(height-2);
				maskp += msk_pitch*(height-2);
			}
			else
			{
				for (int y=1; y<height-1; ++y)
				{
					for (int x=0; x<width; ++x) 
						dstp[x] = (srcpp[x]+(srcp[x]<<1)+srcpn[x]+2)>>2;
					srcpp += src_pitch;
					srcp += src_pitch;
					srcpn += src_pitch;
					dstp += dst_pitch;
					maskp += msk_pitch;
				}
			}
		}
		else
		{
			if ((cpu&CPUF_SSE2) && !((int(srcp)|int(dstp)|int(maskp)|src_pitch|dst_pitch|msk_pitch)&15))
			{
				blendDeintMask_SSE2(srcp, dstp, maskp, src_pitch, dst_pitch, msk_pitch, widtha, height-2);
				srcpp += src_pitch*(height-2);
				srcp += src_pitch*(height-2);
				srcpn += src_pitch*(height-2);
				dstp += dst_pitch*(height-2);
				maskp += msk_pitch*(height-2);
			}
			else if (cpu&CPUF_MMX)
			{
				blendDeintMask_MMX(srcp, dstp, maskp, src_pitch, dst_pitch, msk_pitch, widtha, height-2);
				srcpp += src_pitch*(height-2);
				srcp += src_pitch*(height-2);
				srcpn += src_pitch*(height-2);
				dstp += dst_pitch*(height-2);
				maskp += msk_pitch*(height-2);
			}
			else
			{
				for (int y=1; y<height-1; ++y)
				{
					for (int x=0; x<width; ++x) 
					{
						if (maskp[x] == 0xFF) 
							dstp[x] = (srcpp[x]+(srcp[x]<<1)+srcpn[x]+2)>>2;
						else dstp[x] = srcp[x];
					}
					srcpp += src_pitch;
					srcp += src_pitch;
					srcpn += src_pitch;
					dstp += dst_pitch;
					maskp += msk_pitch;
				}
			}
		}
		for (int x=0; x<width; ++x) 
			dstp[x] = (srcpp[x]+srcp[x]+1)>>1;
	}
}

void TFMPP::blendDeint_SSE2(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
		int dst_pitch, int width, int height)
{
	__asm
	{
		mov ebx,srcp
		mov eax,src_pitch
		mov edx,ebx
		mov edi,ebx
		sub edx,eax
		add edi,eax
		mov esi,dstp
		mov ecx,width
		movdqa xmm6,twosWord
		pxor xmm7,xmm7
yloop:
		xor eax,eax
		align 16
xloop:
		movdqa xmm0,[edx+eax]
		movdqa xmm1,[ebx+eax]
		movdqa xmm2,[edi+eax]
		movdqa xmm3,xmm0
		movdqa xmm4,xmm1
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm1,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm4,xmm7
		punpckhbw xmm5,xmm7
		psllw xmm1,1
		psllw xmm4,1
		paddw xmm1,xmm0
		paddw xmm4,xmm3
		paddw xmm1,xmm2
		paddw xmm4,xmm5
		paddw xmm1,xmm6
		paddw xmm4,xmm6
		psrlw xmm1,2
		psrlw xmm4,2
		packuswb xmm1,xmm4
		movdqa [esi+eax],xmm1
		add eax,16
		cmp eax,ecx
		jl xloop
		add edx,src_pitch
		add ebx,src_pitch
		add edi,src_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
	}
}

void TFMPP::blendDeint_MMX(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
		int dst_pitch, int width, int height)
{
	__asm
	{
		mov ebx,srcp
		mov eax,src_pitch
		mov edx,ebx
		mov edi,ebx
		sub edx,eax
		add edi,eax
		mov esi,dstp
		mov ecx,width
		movq mm6,twosWord
		pxor mm7,mm7
yloop:
		xor eax,eax
		align 16
xloop:
		movq mm0,[edx+eax]
		movq mm1,[ebx+eax]
		movq mm2,[edi+eax]
		movq mm3,mm0
		movq mm4,mm1
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm1,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm4,mm7
		punpckhbw mm5,mm7
		psllw mm1,1
		psllw mm4,1
		paddw mm1,mm0
		paddw mm4,mm3
		paddw mm1,mm2
		paddw mm4,mm5
		paddw mm1,mm6
		paddw mm4,mm6
		psrlw mm1,2
		psrlw mm4,2
		packuswb mm1,mm4
		movq [esi+eax],mm1
		add eax,8
		cmp eax,ecx
		jl xloop
		add edx,src_pitch
		add ebx,src_pitch
		add edi,src_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
		emms
	}
}

void TFMPP::blendDeintMask_SSE2(const unsigned char *srcp, unsigned char *dstp, 
		const unsigned char *maskp, int src_pitch, int dst_pitch, int msk_pitch, 
		int width, int height)
{
	__asm
	{
		mov ebx,srcp
		mov eax,src_pitch
		mov edx,ebx
		mov edi,ebx
		sub edx,eax
		add edi,eax
		mov esi,dstp
		mov ecx,maskp
		movdqa xmm6,twosWord
		pxor xmm7,xmm7
yloop:
		xor eax,eax
		align 16
xloop:
		movdqa xmm0,[edx+eax]
		movdqa xmm1,[ebx+eax]
		movdqa xmm2,[edi+eax]
		movdqa xmm3,xmm0
		movdqa xmm4,xmm1
		movdqa xmm5,xmm2
		punpcklbw xmm0,xmm7
		punpcklbw xmm1,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm4,xmm7
		punpckhbw xmm5,xmm7
		psllw xmm1,1
		psllw xmm4,1
		paddw xmm1,xmm0
		paddw xmm4,xmm3
		paddw xmm1,xmm2
		paddw xmm4,xmm5
		paddw xmm1,xmm6
		paddw xmm4,xmm6
		movdqa xmm0,[ecx+eax]
		psrlw xmm1,2
		psrlw xmm4,2
		movdqa xmm2,xmm0
		packuswb xmm1,xmm4
		pxor xmm2,onesMask
		pand xmm0,xmm1
		pand xmm2,[ebx+eax]
		por xmm0,xmm2
		movdqa [esi+eax],xmm0
		add eax,16
		cmp eax,width
		jl xloop
		add edx,src_pitch
		add ebx,src_pitch
		add edi,src_pitch
		add esi,dst_pitch
		add ecx,msk_pitch
		dec height
		jnz yloop
	}
}

void TFMPP::blendDeintMask_MMX(const unsigned char *srcp, unsigned char *dstp, 
		const unsigned char *maskp, int src_pitch, int dst_pitch, int msk_pitch, 
		int width, int height)
{
	__asm
	{
		mov ebx,srcp
		mov eax,src_pitch
		mov edx,ebx
		mov edi,ebx
		sub edx,eax
		add edi,eax
		mov esi,dstp
		mov ecx,maskp
		movq mm6,twosWord
		pxor mm7,mm7
yloop:
		xor eax,eax
		align 16
xloop:
		movq mm0,[edx+eax]
		movq mm1,[ebx+eax]
		movq mm2,[edi+eax]
		movq mm3,mm0
		movq mm4,mm1
		movq mm5,mm2
		punpcklbw mm0,mm7
		punpcklbw mm1,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm4,mm7
		punpckhbw mm5,mm7
		psllw mm1,1
		psllw mm4,1
		paddw mm1,mm0
		paddw mm4,mm3
		paddw mm1,mm2
		paddw mm4,mm5
		paddw mm1,mm6
		paddw mm4,mm6
		movq mm0,[ecx+eax]
		psrlw mm1,2
		psrlw mm4,2
		movq mm2,mm0
		packuswb mm1,mm4
		pxor mm2,onesMask
		pand mm0,mm1
		pand mm2,[ebx+eax]
		por mm0,mm2
		movq [esi+eax],mm0
		add eax,8
		cmp eax,width
		jl xloop
		add edx,src_pitch
		add ebx,src_pitch
		add edi,src_pitch
		add esi,dst_pitch
		add ecx,msk_pitch
		dec height
		jnz yloop
		emms
	}
}

void TFMPP::CubicDeint(PVideoFrame &src, PlanarFrame *mask, PVideoFrame &dst, bool nomask, 
					   int field, int np, IScriptEnvironment *env)
{
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	for (int b=0; b<np; ++b)
	{
		int plane;
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_U;
		else plane = PLANAR_V;
		const unsigned char *srcp = src->GetReadPtr(plane);
		const int src_pitch = src->GetPitch(plane)<<1;
		const int width = src->GetRowSize(plane);
		const int height = src->GetHeight(plane);
		unsigned char *dstp = dst->GetWritePtr(plane);
		const int dst_pitch = dst->GetPitch(plane)<<1;
		const unsigned char *maskp = mask->GetPtr(b);
		const int msk_pitch = mask->GetPitch(b)<<1;
		srcp += (src_pitch>>1)*(3-field);
		dstp += (dst_pitch>>1)*(2-field);
		maskp += (msk_pitch>>1)*(2-field);
		const unsigned char *srcpp = srcp - src_pitch;
		const unsigned char *srcppp = srcpp - src_pitch;
		const unsigned char *srcpn = srcp + src_pitch;
		const unsigned char *srcr = srcp - (src_pitch>>1);
		if (field == 0)
			env->BitBlt(dst->GetWritePtr(plane),dst_pitch>>1,
				src->GetReadPtr(plane)+(src_pitch>>1),src_pitch>>1,width,1);
		if (nomask)
		{
			for (int x=0; x<width; ++x)
				dstp[x] = (srcp[x]+srcpp[x]+1)>>1;
			srcppp += src_pitch;
			srcpp += src_pitch;
			srcp += src_pitch;
			srcpn += src_pitch;
			dstp += dst_pitch;
			if ((cpu&CPUF_SSE2) && !((int(srcp)|int(dstp)|src_pitch|dst_pitch)&15))
			{
				cubicDeint_SSE2(srcp, dstp, src_pitch, dst_pitch, width, height/2-3);
				srcppp += src_pitch*(height/2-3);
				srcpp += src_pitch*(height/2-3);
				srcp += src_pitch*(height/2-3);
				srcpn += src_pitch*(height/2-3);
				dstp += dst_pitch*(height/2-3);
			}
			else if (cpu&CPUF_MMX)
			{
				cubicDeint_MMX(srcp, dstp, src_pitch, dst_pitch, width, height/2-3);
				srcppp += src_pitch*(height/2-3);
				srcpp += src_pitch*(height/2-3);
				srcp += src_pitch*(height/2-3);
				srcpn += src_pitch*(height/2-3);
				dstp += dst_pitch*(height/2-3);
			}
			else
			{
				for (int y=4-field; y<height-3; y+=2)
				{
					for (int x=0; x<width; ++x)
					{
						const int temp = (19*(srcpp[x]+srcp[x])-3*(srcppp[x]+srcpn[x])+16)>>5;
						if (temp > 255) dstp[x] = 255; 
						else if (temp < 0) dstp[x] = 0;
						else dstp[x] = temp;
					}
					srcppp += src_pitch;
					srcpp += src_pitch;
					srcp += src_pitch;
					srcpn += src_pitch;
					dstp += dst_pitch;
				}
			}
			for (int x=0; x<width; ++x)
				dstp[x] = (srcp[x]+srcpp[x]+1)>>1;
		}
		else
		{
			for (int x=0; x<width; ++x)
			{
				if (maskp[x] == 0xFF) 
					dstp[x] = (srcp[x]+srcpp[x]+1)>>1;
				else dstp[x] = srcr[x];
			}
			srcppp += src_pitch;
			srcpp += src_pitch;
			srcr += src_pitch;
			srcp += src_pitch;
			srcpn += src_pitch;
			maskp += msk_pitch;
			dstp += dst_pitch;
			if ((cpu&CPUF_SSE2) && !((int(srcp)|int(dstp)|int(maskp)|src_pitch|dst_pitch|msk_pitch)&15))
			{
				cubicDeintMask_SSE2(srcp, dstp, maskp, src_pitch, dst_pitch, msk_pitch, width, height/2-3);
				srcppp += src_pitch*(height/2-3);
				srcpp += src_pitch*(height/2-3);
				srcr += src_pitch*(height/2-3);
				srcp += src_pitch*(height/2-3);
				srcpn += src_pitch*(height/2-3);
				maskp += msk_pitch*(height/2-3);
				dstp += dst_pitch*(height/2-3);
			}
			else if (cpu&CPUF_MMX)
			{
				cubicDeintMask_MMX(srcp, dstp, maskp, src_pitch, dst_pitch, msk_pitch, width, height/2-3);
				srcppp += src_pitch*(height/2-3);
				srcpp += src_pitch*(height/2-3);
				srcr += src_pitch*(height/2-3);
				srcp += src_pitch*(height/2-3);
				srcpn += src_pitch*(height/2-3);
				maskp += msk_pitch*(height/2-3);
				dstp += dst_pitch*(height/2-3);
			}
			else
			{
				for (int y=4-field; y<height-3; y+=2)
				{
					for (int x=0; x<width; ++x)
					{
						if (maskp[x] == 0xFF)
						{
							const int temp = (19*(srcpp[x]+srcp[x])-3*(srcppp[x]+srcpn[x])+16)>>5;
							if (temp > 255) dstp[x] = 255; 
							else if (temp < 0) dstp[x] = 0;
							else dstp[x] = temp;
						}
						else dstp[x] = srcr[x];
					}
					srcppp += src_pitch;
					srcpp += src_pitch;
					srcr += src_pitch;
					srcp += src_pitch;
					srcpn += src_pitch;
					maskp += msk_pitch;
					dstp += dst_pitch;
				}
			}
			for (int x=0; x<width; ++x)
			{
				if (maskp[x] == 0xFF) 
					dstp[x] = (srcp[x]+srcpp[x]+1)>>1;
				else dstp[x] = srcr[x];
			}
		}
		if (field == 1)
			env->BitBlt(dst->GetWritePtr(plane)+(height-1)*(dst_pitch>>1),dst_pitch>>1,
				src->GetReadPtr(plane)+(height-2)*(src_pitch>>1),src_pitch>>1,width,1);
	}
}

void TFMPP::cubicDeint_SSE2(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
		int dst_pitch, int width, int height)
{
	__asm
	{
		mov edi,srcp
		mov eax,src_pitch
		mov ebx,edi
		mov ecx,edi
		sub ebx,eax
		add ecx,eax
		mov edx,ebx
		sub edx,eax
		mov esi,dstp
		movdqa xmm6,sixteenWord
		pxor xmm7,xmm7
yloop:
		xor eax,eax
		align 16
xloop:
		movdqa xmm0,[edx+eax]
		movdqa xmm1,[ebx+eax]
		movdqa xmm2,[edi+eax]
		movdqa xmm3,[ecx+eax]
		movdqa xmm4,xmm0
		movdqa xmm5,xmm3
		punpcklbw xmm0,xmm7
		punpcklbw xmm3,xmm7
		punpckhbw xmm4,xmm7
		punpckhbw xmm5,xmm7
		paddw xmm0,xmm3
		paddw xmm4,xmm5
		movdqa xmm3,xmm1
		movdqa xmm5,xmm2
		punpcklbw xmm1,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm5,xmm7
		paddw xmm1,xmm2
		paddw xmm3,xmm5
		pmullw xmm0,threeWord
		pmullw xmm4,threeWord
		pmullw xmm1,nineteenWord
		pmullw xmm3,nineteenWord
		psubusw xmm1,xmm0
		psubusw xmm3,xmm4
		paddusw xmm1,xmm6
		paddusw xmm3,xmm6
		psrlw xmm1,5
		psrlw xmm3,5
		packuswb xmm1,xmm3
		movdqa [esi+eax],xmm1
		add eax,16
		cmp eax,width
		jl xloop
		add edx,src_pitch
		add ebx,src_pitch
		add edi,src_pitch
		add ecx,src_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
	}
}

void TFMPP::cubicDeint_MMX(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
		int dst_pitch, int width, int height)
{
	__asm
	{
		mov edi,srcp
		mov eax,src_pitch
		mov ebx,edi
		mov ecx,edi
		sub ebx,eax
		add ecx,eax
		mov edx,ebx
		sub edx,eax
		mov esi,dstp
		movq mm6,sixteenWord
		pxor mm7,mm7
yloop:
		xor eax,eax
		align 16
xloop:
		movq mm0,[edx+eax]
		movq mm1,[ebx+eax]
		movq mm2,[edi+eax]
		movq mm3,[ecx+eax]
		movq mm4,mm0
		movq mm5,mm3
		punpcklbw mm0,mm7
		punpcklbw mm3,mm7
		punpckhbw mm4,mm7
		punpckhbw mm5,mm7
		paddw mm0,mm3
		paddw mm4,mm5
		movq mm3,mm1
		movq mm5,mm2
		punpcklbw mm1,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm5,mm7
		paddw mm1,mm2
		paddw mm3,mm5
		pmullw mm0,threeWord
		pmullw mm4,threeWord
		pmullw mm1,nineteenWord
		pmullw mm3,nineteenWord
		psubusw mm1,mm0
		psubusw mm3,mm4
		paddusw mm1,mm6
		paddusw mm3,mm6
		psrlw mm1,5
		psrlw mm3,5
		packuswb mm1,mm3
		movq [esi+eax],mm1
		add eax,8
		cmp eax,width
		jl xloop
		add edx,src_pitch
		add ebx,src_pitch
		add edi,src_pitch
		add ecx,src_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
		emms
	}
}

void TFMPP::cubicDeintMask_SSE2(const unsigned char *srcp, unsigned char *dstp, 
		const unsigned char *maskp, int src_pitch, int dst_pitch, int msk_pitch,
		int width, int height)
{
	const int s1 = src_pitch>>1;
	__asm
	{
		mov edi,srcp
		mov eax,src_pitch
		mov ebx,edi
		sub ebx,eax
		mov edx,ebx
		sub edx,eax
		mov ecx,maskp
		mov esi,dstp
		movdqa xmm6,sixteenWord
		pxor xmm7,xmm7
yloop:
		xor eax,eax
		align 16
xloop:
		movdqa xmm0,[edx+eax]
		movdqa xmm1,[ebx+eax]
		movdqa xmm2,[edi+eax]
		add edi,src_pitch
		movdqa xmm4,xmm0
		movdqa xmm3,[edi+eax]
		movdqa xmm5,xmm3
		punpcklbw xmm0,xmm7
		punpcklbw xmm3,xmm7
		punpckhbw xmm4,xmm7
		punpckhbw xmm5,xmm7
		paddw xmm0,xmm3
		paddw xmm4,xmm5
		movdqa xmm3,xmm1
		movdqa xmm5,xmm2
		punpcklbw xmm1,xmm7
		punpcklbw xmm2,xmm7
		punpckhbw xmm3,xmm7
		punpckhbw xmm5,xmm7
		paddw xmm1,xmm2
		paddw xmm3,xmm5
		pmullw xmm0,threeWord
		pmullw xmm4,threeWord
		pmullw xmm1,nineteenWord
		pmullw xmm3,nineteenWord
		psubusw xmm1,xmm0
		psubusw xmm3,xmm4
		paddusw xmm1,xmm6
		paddusw xmm3,xmm6
		movdqa xmm0,[ecx+eax]
		add ebx,s1
		psrlw xmm1,5
		psrlw xmm3,5
		movdqa xmm2,xmm0
		packuswb xmm1,xmm3
		pxor xmm2,onesMask
		pand xmm0,xmm1
		pand xmm2,[ebx+eax]
		sub edi,src_pitch
		por xmm0,xmm2
		sub ebx,s1
		movdqa [esi+eax],xmm0
		add eax,16
		cmp eax,width
		jl xloop
		add edx,src_pitch
		add ebx,src_pitch
		add edi,src_pitch
		add ecx,msk_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
	}
}

void TFMPP::cubicDeintMask_MMX(const unsigned char *srcp, unsigned char *dstp, 
		const unsigned char *maskp, int src_pitch, int dst_pitch, int msk_pitch, 
		int width, int height)
{
	const int s1 = src_pitch>>1;
	__asm
	{
		mov edi,srcp
		mov eax,src_pitch
		mov ebx,edi
		sub ebx,eax
		mov edx,ebx
		sub edx,eax
		mov ecx,maskp
		mov esi,dstp
		movq mm6,sixteenWord
		pxor mm7,mm7
yloop:
		xor eax,eax
		align 16
xloop:
		movq mm0,[edx+eax]
		movq mm1,[ebx+eax]
		movq mm2,[edi+eax]
		add edi,src_pitch
		movq mm4,mm0
		movq mm3,[edi+eax]
		movq mm5,mm3
		punpcklbw mm0,mm7
		punpcklbw mm3,mm7
		punpckhbw mm4,mm7
		punpckhbw mm5,mm7
		paddw mm0,mm3
		paddw mm4,mm5
		movq mm3,mm1
		movq mm5,mm2
		punpcklbw mm1,mm7
		punpcklbw mm2,mm7
		punpckhbw mm3,mm7
		punpckhbw mm5,mm7
		paddw mm1,mm2
		paddw mm3,mm5
		pmullw mm0,threeWord
		pmullw mm4,threeWord
		pmullw mm1,nineteenWord
		pmullw mm3,nineteenWord
		psubusw mm1,mm0
		psubusw mm3,mm4
		paddusw mm1,mm6
		paddusw mm3,mm6
		movq mm0,[ecx+eax]
		add ebx,s1
		psrlw mm1,5
		psrlw mm3,5
		movq mm2,mm0
		packuswb mm1,mm3
		pxor mm2,onesMask
		pand mm0,mm1
		pand mm2,[ebx+eax]
		sub edi,src_pitch
		por mm0,mm2
		sub ebx,s1
		movq [esi+eax],mm0
		add eax,8
		cmp eax,width
		jl xloop
		add edx,src_pitch
		add ebx,src_pitch
		add edi,src_pitch
		add ecx,msk_pitch
		add esi,dst_pitch
		dec height
		jnz yloop
		emms
	}
}

void TFMPP::destroyHint(PVideoFrame &dst, unsigned int hint)
{
	unsigned char *p = dst->GetWritePtr(PLANAR_Y), i;
	if (hint&0x80)
	{
		hint >>= 8;
		for (i=0; i<32; ++i)
		{
			*p &= ~1;
			*p++ |= ((MAGIC_NUMBER_2 & (1 << i)) >> i);
		}
		for (i=0; i<32; ++i)
		{
			*p &= ~1;
			*p++ |= ((hint & (1 << i)) >> i);
		}
	}
	else
	{
		for (int i=0; i<64; ++i) 
			*p++ &= ~1;
	}
}

void TFMPP::putHint(PVideoFrame &dst, int field, unsigned int hint)
{
	unsigned char *p = dst->GetWritePtr(PLANAR_Y);
	unsigned int i;
	hint &= (D2VFILM|0xFF80);
	if (field == 1) 
	{
		hint |= TOP_FIELD;
		hint |= ISDT;
	}
	else hint |= ISDB;
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		*p++ |= ((MAGIC_NUMBER & (1 << i)) >> i);
	}
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		*p++ |= ((hint & (1 << i)) >> i);
	}
}

bool TFMPP::getHint(PVideoFrame &src, int &field, bool &combed, unsigned int &hint)
{
	field = -1; combed = false; hint = 0;
	const unsigned char *srcp = src->GetReadPtr(PLANAR_Y);
	unsigned int i, magic_number = 0;
	for (i=0; i<32; ++i)
	{
		magic_number |= ((*srcp++ & 1) << i);
	}
	if (magic_number != MAGIC_NUMBER) return false;
	for (i=0; i<32; ++i)
	{
		hint |= ((*srcp++ & 1) << i);
	}
	if (hint&0xFFFF0000) return false;
	if (hint&TOP_FIELD) field = 1;
	else field = 0;
	if (hint&COMBED) combed = true;
	int value = hint&0x07;
	if (value == 5) { combed = true; field = 0; }
	else if (value == 6) { combed = true; field = 1; }
	return true;
}

void TFMPP::getSetOvr(int n)
{
	if (setArray == NULL || setArraySize <= 0) return;
	mthresh = mthreshS;
	PP = PPS;
	for (int x=0; x<setArraySize; x+=4)
	{
		if (n >= setArray[x+1] && n <= setArray[x+2])
		{
			if (setArray[x] == 80) PP = setArray[x+3];
			else if (setArray[x] == 77) mthresh = setArray[x+3];
		}
	}
}

void TFMPP::copyFrame(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env, int np)
{
	int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
	for (int b=0; b<np; ++b)
	{
		env->BitBlt(dst->GetWritePtr(plane[b]), dst->GetPitch(plane[b]), src->GetReadPtr(plane[b]),
			src->GetPitch(plane[b]), src->GetRowSize(plane[b]+8), src->GetHeight(plane[b]));
	}
}

void TFMPP::copyField(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env, int np, int field)
{
	int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
	for (int b=0; b<np; ++b)
	{
		if (field == 0)
			env->BitBlt(dst->GetWritePtr(plane[b]),dst->GetPitch(plane[b]),src->GetReadPtr(plane[b])+
				src->GetPitch(plane[b]),src->GetPitch(plane[b]),src->GetRowSize(plane[b]+8),1);
		env->BitBlt(dst->GetWritePtr(plane[b])+dst->GetPitch(plane[b])*(1-field), 
			dst->GetPitch(plane[b])*2, src->GetReadPtr(plane[b])+src->GetPitch(plane[b])*(1-field),
			src->GetPitch(plane[b])*2, src->GetRowSize(plane[b]+8), src->GetHeight(plane[b])>>1);
		if (field == 1)
			env->BitBlt(dst->GetWritePtr(plane[b])+dst->GetPitch(plane[b])*(dst->GetHeight(plane[b])-1),
				dst->GetPitch(plane[b]),src->GetReadPtr(plane[b])+src->GetPitch(plane[b])*(src->GetHeight(plane[b])-2),
				src->GetPitch(plane[b]),src->GetRowSize(plane[b]+8),1);
	}
}

unsigned char TFMPP::cubicInt(unsigned char p1, unsigned char p2, unsigned char p3, unsigned char p4)
{
	const int temp = (19*(p2+p3)-3*(p1+p4)+16)>>5;
	if (temp > 255) return 255; 
	else if (temp < 0) return 0;
	return (unsigned char)temp;
}

void TFMPP::writeDisplay(PVideoFrame &dst, int np, int n, int field)
{
	sprintf(buf, "TFMPP %s by tritical ", VERSION);
	if (np == 3) TFM::DrawYV12(dst, 0, 0, buf);
	else TFM::DrawYUY2(dst, 0, 0, buf);
	sprintf(buf, "field = %d  PP = %d  mthresh = %d ", field, PP, mthresh);
	if (np == 3) TFM::DrawYV12(dst, 0, 1, buf);
	else TFM::DrawYUY2(dst, 0, 1, buf);
	sprintf(buf, "frame: %d  (COMBED - DEINTERLACED)! ", n);
	if (np == 3) TFM::DrawYV12(dst, 0, 2, buf);
	else TFM::DrawYUY2(dst, 0, 2, buf);
}

void TFMPP::elaDeint(PVideoFrame &dst, PlanarFrame *mask, PVideoFrame &src, bool nomask, int field, int np)
{
	if (np == 3) elaDeintYV12(dst, mask, src, nomask, field);
	else elaDeintYUY2(dst, mask, src, nomask, field);
}

void TFMPP::elaDeintYV12(PVideoFrame &dst, PlanarFrame *mask, PVideoFrame &src, bool nomask, int field)
{
	const unsigned char *srcpY = src->GetReadPtr(PLANAR_Y);
	const unsigned char *srcpV = src->GetReadPtr(PLANAR_V);
	const unsigned char *srcpU = src->GetReadPtr(PLANAR_U);
	int src_pitchY = src->GetPitch(PLANAR_Y);
	int src_pitchUV = src->GetPitch(PLANAR_V);
	int WidthY = src->GetRowSize(PLANAR_Y);
	int WidthUV = src->GetRowSize(PLANAR_V);
	int HeightY = src->GetHeight(PLANAR_Y);
	int HeightUV = src->GetHeight(PLANAR_V);
	unsigned char *dstpY = dst->GetWritePtr(PLANAR_Y);
	unsigned char *dstpV = dst->GetWritePtr(PLANAR_V);
	unsigned char *dstpU = dst->GetWritePtr(PLANAR_U);
	int dst_pitchY = dst->GetPitch(PLANAR_Y);
	int dst_pitchUV = dst->GetPitch(PLANAR_V);
	const unsigned char *maskpY = mask->GetPtr(0);
	const unsigned char *maskpV = mask->GetPtr(2);
	const unsigned char *maskpU = mask->GetPtr(1);
	int mask_pitchY = mask->GetPitch(0);
	int mask_pitchUV = mask->GetPitch(2);
	srcpY += src_pitchY*(3-field);
	srcpV += src_pitchUV*(3-field);
	srcpU += src_pitchUV*(3-field);
	dstpY += dst_pitchY*(2-field);
	dstpV += dst_pitchUV*(2-field);
	dstpU += dst_pitchUV*(2-field);
	maskpY += mask_pitchY*(2-field);
	maskpV += mask_pitchUV*(2-field);
	maskpU += mask_pitchUV*(2-field);
	src_pitchY <<= 1;
	src_pitchUV <<= 1;
	dst_pitchY <<= 1;
	dst_pitchUV <<= 1;
	mask_pitchY <<= 1;
	mask_pitchUV <<= 1;
	const unsigned char *srcppY = srcpY - src_pitchY;
	const unsigned char *srcpppY = srcppY - src_pitchY;
	const unsigned char *srcpnY = srcpY + src_pitchY;
	const unsigned char *srcppV = srcpV - src_pitchUV;
	const unsigned char *srcpppV = srcppV - src_pitchUV;
	const unsigned char *srcpnV = srcpV + src_pitchUV;
	const unsigned char *srcppU = srcpU - src_pitchUV;
	const unsigned char *srcpppU = srcppU - src_pitchUV;
	const unsigned char *srcpnU = srcpU + src_pitchUV;
	int stopx = WidthY;
	int startxuv = 0, x, y;
	int stopxuv = WidthUV;
	int Iy1, Iy2, Iye, Ix1, Ix2, edgeS1, edgeS2, sum, sumsq, temp, temp1, temp2, minN, maxN;
	double dir1, dir2, dir, dirF;
	for (y=2-field; y<HeightY-1; y+=2)
	{
		for (x=0; x<stopx; ++x)
		{
			if (nomask || maskpY[x] == 0xFF)
			{
				if (y>2 && y<HeightY-3 && x>3 && x<WidthY-4)
				{
					Iy1 = -srcpY[x-1]-srcpY[x]-srcpY[x]-srcpY[x+1]+srcpppY[x-1]+srcpppY[x]+srcpppY[x]+srcpppY[x+1];
					Iy2 = -srcpnY[x-1]-srcpnY[x]-srcpnY[x]-srcpnY[x+1]+srcppY[x-1]+srcppY[x]+srcppY[x]+srcppY[x+1];
					Ix1 = srcpppY[x+1]+srcppY[x+1]+srcppY[x+1]+srcpY[x+1]-srcpppY[x-1]-srcppY[x-1]-srcppY[x-1]-srcpY[x-1];
					Ix2 = srcppY[x+1]+srcpY[x+1]+srcpY[x+1]+srcpnY[x+1]-srcppY[x-1]-srcpY[x-1]-srcpY[x-1]-srcpnY[x-1];
					edgeS1 = Ix1*Ix1 + Iy1*Iy1;
					edgeS2 = Ix2*Ix2 + Iy2*Iy2;
					if (edgeS1 < 1600 && edgeS2 < 1600)
					{
						dstpY[x] = (srcppY[x]+srcpY[x]+1)>>1;
						continue;
					}
					if (abs(srcppY[x]-srcpY[x]) < 10 && (edgeS1 < 1600 || edgeS2 < 1600))
					{
						dstpY[x] = (srcppY[x]+srcpY[x]+1)>>1;
						continue;
					}
					sum = srcppY[x-1] + srcppY[x] + srcppY[x+1] + srcpY[x-1] + srcpY[x] + srcpY[x+1];
					sumsq = srcppY[x-1]*srcppY[x-1] + srcppY[x]*srcppY[x] + srcppY[x+1]*srcppY[x+1] +
							srcpY[x-1]*srcpY[x-1] + srcpY[x]*srcpY[x] + srcpY[x+1]*srcpY[x+1];
					if ((6*sumsq - sum*sum) < 432)
					{
						dstpY[x] = (srcppY[x]+srcpY[x]+1)>>1;
						continue;
					}
					if (Ix1 == 0) dir1 = 3.1415926;
					else 
					{
						dir1 = atan(Iy1/(Ix1*2.0f)) + 1.5707963;
						if (Iy1 >= 0) { if (Ix1 < 0) dir1 += 3.1415927; }
						else { if (Ix1 >= 0) dir1 += 3.1415927; }
						if (dir1 >= 3.1415927) dir1 -= 3.1415927;
					}
					if (Ix2 == 0) dir2 = 3.1415926;
					else 
					{
						dir2 = atan(Iy2/(Ix2*2.0f)) + 1.5707963;
						if (Iy2 >= 0) { if (Ix2 < 0) dir2 += 3.1415927; }
						else { if (Ix2 >= 0) dir2 += 3.1415927; }
						if (dir2 >= 3.1415927) dir2 -= 3.1415927;
					}
					if (fabs(dir1-dir2) < 0.5) 
					{
						if (edgeS1 >= 3600 && edgeS2 >= 3600) dir = (dir1 + dir2) * 0.5;
						else dir = edgeS1 >= edgeS2 ? dir1 : dir2;
					}
					else 
					{
						if (edgeS1 >= 5000 && edgeS2 >= 5000)
						{
							Iye = -srcpY[x-1]-srcpY[x]-srcpY[x]-srcpY[x+1]+srcppY[x-1]+srcppY[x]+srcppY[x]+srcppY[x+1];
							if ((Iy1*Iye > 0) && (Iy2*Iye < 0)) dir = dir1;
							else if ((Iy1*Iye < 0) && (Iy2*Iye > 0)) dir = dir2;
							else 
							{
								if (abs(Iye-Iy1) <= abs(Iye-Iy2)) dir = dir1;
								else dir = dir2;
							}
						}
						else dir = edgeS1 >= edgeS2 ? dir1 : dir2;
					}
					dirF = 0.5f/tan(dir);
					if (dirF >= 0.0f)
					{
						if (dirF >= 0.5f)
						{
							if (dirF >= 1.0f)
							{
								if (dirF >= 1.5f)
								{
									if (dirF >= 2.0f)
									{
										if (dirF <= 2.50f)
										{
											temp1 = srcppY[x+4];
											temp2 = srcpY[x-4];
											temp = (srcppY[x+4]+srcpY[x-4]+1)>>1;
										}
										else 
										{
											temp1 = temp2 = srcpY[x];
											temp = cubicInt(srcpppY[x],srcppY[x],srcpY[x],srcpnY[x]);
										}
									}
									else 
									{
										temp1 = (int)((dirF-1.5f)*(srcppY[x+4]) + (2.0f-dirF)*(srcppY[x+3]) + 0.5f);
										temp2 = (int)((dirF-1.5f)*(srcpY[x-4]) + (2.0f-dirF)*(srcpY[x-3]) + 0.5f);
										temp = (int)((dirF-1.5f)*(srcppY[x+4]+srcpY[x-4]) + (2.0f-dirF)*(srcppY[x+3]+srcpY[x-3]) + 0.5f);
									}
								}
								else 
								{
									temp1 = (int)((dirF-1.0f)*(srcppY[x+3]) + (1.5f-dirF)*(srcppY[x+2]) + 0.5f);
									temp2 = (int)((dirF-1.0f)*(srcpY[x-3]) + (1.5f-dirF)*(srcpY[x-2]) + 0.5f);
									temp = (int)((dirF-1.0f)*(srcppY[x+3]+srcpY[x-3]) + (1.5f-dirF)*(srcppY[x+2]+srcpY[x-2]) + 0.5f);
								}
							}
							else 
							{
								temp1 = (int)((dirF-0.5f)*(srcppY[x+2]) + (1.0f-dirF)*(srcppY[x+1]) + 0.5f);
								temp2 = (int)((dirF-0.5f)*(srcpY[x-2]) + (1.0f-dirF)*(srcpY[x-1]) + 0.5f);
								temp = (int)((dirF-0.5f)*(srcppY[x+2]+srcpY[x-2]) + (1.0f-dirF)*(srcppY[x+1]+srcpY[x-1]) + 0.5f);
							}
						}
						else 
						{
							temp1 = (int)(dirF*(srcppY[x+1]) + (0.5f-dirF)*(srcppY[x]) + 0.5f);
							temp2 = (int)(dirF*(srcpY[x-1]) + (0.5f-dirF)*(srcpY[x]) + 0.5f);
							temp = (int)(dirF*(srcppY[x+1]+srcpY[x-1]) + (0.5f-dirF)*(srcppY[x]+srcpY[x]) + 0.5f);
						}
					}
					else
					{
						if (dirF <= -0.5f)
						{
							if (dirF <= -1.0f)
							{
								if (dirF <= -1.5f)
								{
									if (dirF <= -2.0f)
									{
										if (dirF >= -2.50f) 
										{
											temp1 = srcppY[x-4];
											temp2 = srcpY[x+4];
											temp = (srcppY[x-4]+srcpY[x+4]+1)>>1;
										}
										else 
										{
											temp1 = temp2 = srcpY[x];
											temp = cubicInt(srcpppY[x],srcppY[x],srcpY[x],srcpnY[x]);
										}
									}
									else
									{
										temp1 = (int)((-dirF-1.5f)*(srcppY[x-4]) + (2.0f+dirF)*(srcppY[x-3]) + 0.5f);
										temp2 = (int)((-dirF-1.5f)*(srcpY[x+4]) + (2.0f+dirF)*(srcpY[x+3]) + 0.5f);
										temp = (int)((-dirF-1.5f)*(srcppY[x-4]+srcpY[x+4]) + (2.0f+dirF)*(srcppY[x-3]+srcpY[x+3]) + 0.5f);
									}
								}
								else 
								{
									temp1 = (int)((-dirF-1.0f)*(srcppY[x-3]) + (1.5f+dirF)*(srcppY[x-2]) + 0.5f);
									temp2 = (int)((-dirF-1.0f)*(srcpY[x+3]) + (1.5f+dirF)*(srcpY[x+2]) + 0.5f);
									temp = (int)((-dirF-1.0f)*(srcppY[x-3]+srcpY[x+3]) + (1.5f+dirF)*(srcppY[x-2]+srcpY[x+2]) + 0.5f);
								}
							}
							else 
							{
								temp1 = (int)((-dirF-0.5f)*(srcppY[x-2]) + (1.0f+dirF)*(srcppY[x-1]) + 0.5f);
								temp2 = (int)((-dirF-0.5f)*(srcpY[x+2]) + (1.0f+dirF)*(srcpY[x+1]) + 0.5f);
								temp = (int)((-dirF-0.5f)*(srcppY[x-2]+srcpY[x+2]) + (1.0f+dirF)*(srcppY[x-1]+srcpY[x+1]) + 0.5f);
							}
						}
						else 
						{
							temp1 = (int)((-dirF)*(srcppY[x-1]) + (0.5f+dirF)*(srcppY[x]) + 0.5f);
							temp2 = (int)((-dirF)*(srcpY[x+1]) + (0.5f+dirF)*(srcpY[x]) + 0.5f);
							temp = (int)((-dirF)*(srcppY[x-1]+srcpY[x+1]) + (0.5f+dirF)*(srcppY[x]+srcpY[x]) + 0.5f);
						}
					}
					minN = min(srcppY[x],srcpY[x]) - 25;
					maxN = max(srcppY[x],srcpY[x]) + 25;
					if (abs(temp1-temp2) > 20 || abs(srcppY[x]+srcpY[x]-temp-temp) > 60 || temp < minN || temp > maxN)
					{
						temp = cubicInt(srcpppY[x],srcppY[x],srcpY[x],srcpnY[x]);
					}
					if (temp > 255) temp = 255;
					else if (temp < 0) temp = 0;
					dstpY[x] = temp;
				}
				else
				{
					if (y<3 || y>HeightY-4) dstpY[x] = ((srcpY[x] + srcppY[x] + 1) >> 1);
					else dstpY[x] = cubicInt(srcpppY[x],srcppY[x],srcpY[x],srcpnY[x]);
				}
			}
		}
		srcpppY = srcppY;
		srcppY = srcpY;
		srcpY = srcpnY;
		srcpnY += src_pitchY;
		maskpY += mask_pitchY;
		dstpY += dst_pitchY;
	}
	for (y=2-field; y<HeightUV-1; y+=2)
	{
		for (x=startxuv; x<stopxuv; ++x)
		{
			if (nomask || maskpV[x] == 0xFF)
			{
				if (y<3 || y>HeightUV-4) dstpV[x] = ((srcpV[x] + srcppV[x] + 1) >> 1);
				else dstpV[x] = cubicInt(srcpppV[x],srcppV[x],srcpV[x],srcpnV[x]);
			}
			if (nomask || maskpU[x] == 0xFF)
			{
				if (y<3 || y>HeightUV-4) dstpU[x] = ((srcpU[x] + srcppU[x] + 1) >> 1);
				else dstpU[x] = cubicInt(srcpppU[x],srcppU[x],srcpU[x],srcpnU[x]);
			}
		}
		srcpppV = srcppV;
		srcppV = srcpV;
		srcpV = srcpnV;
		srcpnV += src_pitchUV;
		srcpppU = srcppU;
		srcppU = srcpU;
		srcpU = srcpnU;
		srcpnU += src_pitchUV;
		maskpV += mask_pitchUV;
		maskpU += mask_pitchUV;
		dstpV += dst_pitchUV;
		dstpU += dst_pitchUV;
	}
}

void TFMPP::elaDeintYUY2(PVideoFrame &dst, PlanarFrame *mask, PVideoFrame &src, bool nomask, int field)
{
	const unsigned char *srcp = src->GetReadPtr();
	int src_pitch = src->GetPitch();
	int Width = src->GetRowSize();
	int Height = src->GetHeight();
	unsigned char *dstp = dst->GetWritePtr();
	int dst_pitch = dst->GetPitch();
	const unsigned char *maskp = mask->GetPtr();
	int mask_pitch = mask->GetPitch();
	srcp += src_pitch*(3-field);
	dstp += dst_pitch*(2-field);
	maskp += mask_pitch*(2-field);
	src_pitch <<= 1;
	dst_pitch <<= 1;
	mask_pitch <<= 1;
	const unsigned char *srcpp = srcp - src_pitch;
	const unsigned char *srcppp = srcpp - src_pitch;
	const unsigned char *srcpn = srcp + src_pitch;
	int stopx = Width;
	int Iy1, Iy2, Iye, Ix1, Ix2, edgeS1, edgeS2, sum, sumsq, temp, temp1, temp2, minN, maxN, x, y;
	double dir1, dir2, dir, dirF;
	for (y=2-field; y<Height-1; y+=2)
	{
		for (x=0; x<stopx; ++x)
		{
			if (nomask || maskp[x] == 0xFF)
			{
				if (y>2 && y<Height-3 && x>7 && x<Width-9)
				{
					Iy1 = -srcp[x-2]-srcp[x]-srcp[x]-srcp[x+2]+srcppp[x-2]+srcppp[x]+srcppp[x]+srcppp[x+2];
					Iy2 = -srcpn[x-2]-srcpn[x]-srcpn[x]-srcpn[x+2]+srcpp[x-2]+srcpp[x]+srcpp[x]+srcpp[x+2];
					Ix1 = srcppp[x+2]+srcpp[x+2]+srcpp[x+2]+srcp[x+2]-srcppp[x-2]-srcpp[x-2]-srcpp[x-2]-srcp[x-2];
					Ix2 = srcpp[x+2]+srcp[x+2]+srcp[x+2]+srcpn[x+2]-srcpp[x-2]-srcp[x-2]-srcp[x-2]-srcpn[x-2];
					edgeS1 = Ix1*Ix1 + Iy1*Iy1;
					edgeS2 = Ix2*Ix2 + Iy2*Iy2;
					if (edgeS1 < 1600 && edgeS2 < 1600) 
					{
						dstp[x] = (srcpp[x]+srcp[x]+1)>>1;
						goto chromajump;
					}
					if (abs(srcpp[x]-srcp[x]) < 10 && (edgeS1 < 1600 || edgeS2 < 1600))
					{
						dstp[x] = (srcpp[x]+srcp[x]+1)>>1;
						goto chromajump;
					}
					sum = srcpp[x-2] + srcpp[x] + srcpp[x+2] + srcp[x-2] + srcp[x] + srcp[x+2];
					sumsq = srcpp[x-2]*srcpp[x-2] + srcpp[x]*srcpp[x] + srcpp[x+2]*srcpp[x+2] +
							srcp[x-2]*srcp[x-2] + srcp[x]*srcp[x] + srcp[x+2]*srcp[x+2];
					if ((6*sumsq - sum*sum) < 432)
					{
						dstp[x] = (srcpp[x]+srcp[x]+1)>>1;
						goto chromajump;
					}
					if (Ix1 == 0) dir1 = 3.1415926;
					else 
					{
						dir1 = atan(Iy1/(Ix1*2.0f)) + 1.5707963;
						if (Iy1 >= 0) { if (Ix1 < 0) dir1 += 3.1415927; }
						else { if (Ix1 >= 0) dir1 += 3.1415927; }
						if (dir1 >= 3.1415927) dir1 -= 3.1415927;
					}
					if (Ix2 == 0) dir2 = 3.1415926;
					else 
					{
						dir2 = atan(Iy2/(Ix2*2.0f)) + 1.5707963;
						if (Iy2 >= 0) { if (Ix2 < 0) dir2 += 3.1415927; }
						else { if (Ix2 >= 0) dir2 += 3.1415927; }
						if (dir2 >= 3.1415927) dir2 -= 3.1415927;
					}
					if (fabs(dir1-dir2) < 0.5f) 
					{
						if (edgeS1 >= 3600 && edgeS2 >= 3600) dir = (dir1 + dir2) * 0.5f;
						else dir = edgeS1 >= edgeS2 ? dir1 : dir2;
					}
					else 
					{
						if (edgeS1 >= 5000 && edgeS2 >= 5000)
						{
							Iye = -srcp[x-2]-srcp[x]-srcp[x]-srcp[x+2]+srcpp[x-2]+srcpp[x]+srcpp[x]+srcpp[x+2];
							if ((Iy1*Iye > 0) && (Iy2*Iye < 0)) dir = dir1;
							else if ((Iy1*Iye < 0) && (Iy2*Iye > 0)) dir = dir2;
							else 
							{
								if (abs(Iye-Iy1) <= abs(Iye-Iy2)) dir = dir1;
								else dir = dir2;
							}
						}
						else dir = edgeS1 >= edgeS2 ? dir1 : dir2;
					}
					dirF = 0.5f/tan(dir);
					if (dirF >= 0.0f)
					{
						if (dirF >= 0.5f)
						{
							if (dirF >= 1.0f)
							{
								if (dirF >= 1.5f)
								{
									if (dirF >= 2.0f)
									{
										if (dirF <= 2.50f) 
										{
											temp1 = srcpp[x+8];
											temp2 = srcp[x-8];
											temp = (srcpp[x+8]+srcp[x-8]+1)>>1;
										}
										else 
										{
											temp1 = temp2 = srcp[x];
											temp = cubicInt(srcppp[x],srcpp[x],srcp[x],srcpn[x]);
										}
									}
									else 
									{
										temp1 = (int)((dirF-1.5f)*(srcpp[x+8]) + (2.0f-dirF)*(srcpp[x+6]) + 0.5f);
										temp2 = (int)((dirF-1.5f)*(srcp[x-8]) + (2.0f-dirF)*(srcp[x-6]) + 0.5f); 
										temp = (int)((dirF-1.5f)*(srcpp[x+8]+srcp[x-8]) + (2.0f-dirF)*(srcpp[x+6]+srcp[x-6]) + 0.5f);
									}
								}
								else 
								{
									temp1 = (int)((dirF-1.0f)*(srcpp[x+6]) + (1.5f-dirF)*(srcpp[x+4]) + 0.5f);
									temp2 = (int)((dirF-1.0f)*(srcp[x-6]) + (1.5f-dirF)*(srcp[x-4]) + 0.5f);
									temp = (int)((dirF-1.0f)*(srcpp[x+6]+srcp[x-6]) + (1.5f-dirF)*(srcpp[x+4]+srcp[x-4]) + 0.5f);
								}
							}
							else 
							{
								temp1 = (int)((dirF-0.5f)*(srcpp[x+4]) + (1.0f-dirF)*(srcpp[x+2]) + 0.5f);
								temp2 = (int)((dirF-0.5f)*(srcp[x-4]) + (1.0f-dirF)*(srcp[x-2]) + 0.5f);
								temp = (int)((dirF-0.5f)*(srcpp[x+4]+srcp[x-4]) + (1.0f-dirF)*(srcpp[x+2]+srcp[x-2]) + 0.5f);
							}
						}
						else 
						{
							temp1 = (int)(dirF*(srcpp[x+2]) + (0.5f-dirF)*(srcpp[x]) + 0.5f);
							temp2 = (int)(dirF*(srcp[x-2]) + (0.5f-dirF)*(srcp[x]) + 0.5f);
							temp = (int)(dirF*(srcpp[x+2]+srcp[x-2]) + (0.5f-dirF)*(srcpp[x]+srcp[x]) + 0.5f);
						}
					}
					else
					{
						if (dirF <= -0.5f)
						{
							if (dirF <= -1.0f)
							{
								if (dirF <= -1.5f)
								{
									if (dirF <= -2.0f)
									{
										if (dirF >= -2.50f) 
										{
											temp1 = srcpp[x-8];
											temp2 = srcp[x+8];
											temp = (srcpp[x-8]+srcp[x+8]+1)>>1;
										}
										else 
										{
											temp1 = temp2 = srcp[x];
											temp = cubicInt(srcppp[x],srcpp[x],srcp[x],srcpn[x]);
										}
									}
									else 
									{
										temp1 = (int)((-dirF-1.5f)*(srcpp[x-8]) + (2.0f+dirF)*(srcpp[x-6]) + 0.5f);
										temp2 = (int)((-dirF-1.5f)*(srcp[x+8]) + (2.0f+dirF)*(srcp[x+6]) + 0.5f);
										temp = (int)((-dirF-1.5f)*(srcpp[x-8]+srcp[x+8]) + (2.0f+dirF)*(srcpp[x-6]+srcp[x+6]) + 0.5f);
									}
								}
								else 
								{
									temp1 = (int)((-dirF-1.0f)*(srcpp[x-6]) + (1.5f+dirF)*(srcpp[x-4]) + 0.5f);
									temp2 = (int)((-dirF-1.0f)*(srcp[x+6]) + (1.5f+dirF)*(srcp[x+4]) + 0.5f);
									temp = (int)((-dirF-1.0f)*(srcpp[x-6]+srcp[x+6]) + (1.5f+dirF)*(srcpp[x-4]+srcp[x+4]) + 0.5f);
								}
							}
							else 
							{
								temp1 = (int)((-dirF-0.5f)*(srcpp[x-4]) + (1.0f+dirF)*(srcpp[x-2]) + 0.5f);
								temp2 = (int)((-dirF-0.5f)*(srcp[x+4]) + (1.0f+dirF)*(srcp[x+2]) + 0.5f);
								temp = (int)((-dirF-0.5f)*(srcpp[x-4]+srcp[x+4]) + (1.0f+dirF)*(srcpp[x-2]+srcp[x+2]) + 0.5f);
							}
						}
						else 
						{
							temp1 = (int)((-dirF)*(srcpp[x-2]) + (0.5f+dirF)*(srcpp[x]) + 0.5f);
							temp2 = (int)((-dirF)*(srcp[x+2]) + (0.5f+dirF)*(srcp[x]) + 0.5f);
							temp = (int)((-dirF)*(srcpp[x-2]+srcp[x+2]) + (0.5f+dirF)*(srcpp[x]+srcp[x]) + 0.5f);
						}
					}
					minN = min(srcpp[x],srcp[x]) - 25;
					maxN = max(srcpp[x],srcp[x]) + 25;
					if (abs(temp1-temp2) > 20 || abs(srcpp[x]+srcp[x]-temp-temp) > 60 || temp < minN || temp > maxN)
					{
						temp = cubicInt(srcppp[x],srcpp[x],srcp[x],srcpn[x]);
					}
					if (temp > 255) temp = 255;
					else if (temp < 0) temp = 0;
					dstp[x] = temp;
				}
				else
				{
					if (y<3 || y>Height-4) dstp[x] = ((srcp[x] + srcpp[x] + 1) >> 1);
					else dstp[x] = cubicInt(srcppp[x],srcpp[x],srcp[x],srcpn[x]);
				}
			}
chromajump:
			++x;
			if (nomask || maskp[x] == 0xFF)
			{
				if (y<3 || y>Height-4) dstp[x] = ((srcp[x] + srcpp[x] + 1) >> 1);
				else dstp[x] = cubicInt(srcppp[x],srcpp[x],srcp[x],srcpn[x]);
			}
		}
		srcppp = srcpp;
		srcpp = srcp;
		srcp = srcpn;
		srcpn += src_pitch;
		maskp += mask_pitch;
		dstp += dst_pitch;
	}
}

void TFMPP::maskClip2(PVideoFrame &src, PVideoFrame &deint, PlanarFrame *mask, 
					  PVideoFrame &dst, int np, IScriptEnvironment *env)
{
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	const unsigned char *srcp, *maskp, *dntp;
	unsigned char *dstp;
	int b, plane, x, y, width, height;
	int src_pitch, msk_pitch, dst_pitch, dnt_pitch;
	for (b=0; b<np; ++b)
	{
		if (b==0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_V;
		else plane = PLANAR_U;
		srcp = src->GetReadPtr(plane);
		width = src->GetRowSize(plane);
		height = src->GetHeight(plane);
		src_pitch = src->GetPitch(plane);
		maskp = mask->GetPtr(b);
		msk_pitch = mask->GetPitch(b);
		dntp = deint->GetReadPtr(plane);
		dnt_pitch = deint->GetPitch(plane);
		dstp = dst->GetWritePtr(plane);
		dst_pitch = dst->GetPitch(plane);
		if ((cpu&CPUF_SSE2) && !((int(srcp)|int(dntp)|int(maskp)|int(dstp)|src_pitch|
			dnt_pitch|msk_pitch|dst_pitch)&15))
		{
			maskClip2_SSE2(srcp, dntp, maskp, dstp, src_pitch, dnt_pitch, msk_pitch, 
				dst_pitch, width, height);
		}
		else if (cpu&CPUF_MMX)
		{
			maskClip2_MMX(srcp, dntp, maskp, dstp, src_pitch, dnt_pitch, msk_pitch, 
				dst_pitch, width, height);
		}
		else
		{
			for (y=0; y<height; ++y)
			{
				for (x=0; x<width; ++x)
				{
					if (maskp[x] == 0xFF) dstp[x] = dntp[x];
					else dstp[x] = srcp[x];
				}
				maskp += msk_pitch;
				srcp += src_pitch;
				dntp += dnt_pitch;
				dstp += dst_pitch;
			}
		}
	}
}

void TFMPP::maskClip2_MMX(const unsigned char *srcp, const unsigned char *dntp, 
	  const unsigned char *maskp, unsigned char *dstp, int src_pitch, int dnt_pitch,
	  int msk_pitch, int dst_pitch, int width, int height)
{
	if ((src_pitch|dnt_pitch|msk_pitch|dst_pitch)&15)
	{
		__asm
		{
			mov eax, srcp
			mov ebx, dntp
			mov esi, maskp
			mov edx, dstp
			mov edi, width
			movq mm7, onesMask
	yloop8:
			xor ecx, ecx
			align 16
	xloop8:
			movq mm0, [esi+ecx]
			movq mm1, mm0
			pand mm0, [ebx+ecx]
			pxor mm1, mm7
			pand mm1, [eax+ecx]
			por mm0, mm1
			movq [edx+ecx], mm0
			add ecx, 8
			cmp ecx, edi
			jl xloop8
			add eax, src_pitch
			add ebx, dnt_pitch
			add edx, dst_pitch
			add esi, msk_pitch
			dec height
			jnz yloop8
			emms
		}
	}
	else
	{
		__asm
		{
			mov eax, srcp
			mov ebx, dntp
			mov esi, maskp
			mov edx, dstp
			mov edi, width
			movq mm7, onesMask
	yloop16:
			xor ecx, ecx
			align 16
	xloop16:
			movq mm0, [esi+ecx]
			movq mm1, [esi+ecx+8]
			movq mm2, mm0
			movq mm3, mm1
			pand mm0, [ebx+ecx]
			pand mm1, [ebx+ecx+8]
			pxor mm2, mm7
			pxor mm3, mm7
			pand mm2, [eax+ecx]
			pand mm3, [eax+ecx+8]
			por mm0, mm2
			por mm1, mm3
			movq [edx+ecx], mm0
			movq [edx+ecx+8], mm1
			add ecx, 16
			cmp ecx, edi
			jl xloop16
			add eax, src_pitch
			add ebx, dnt_pitch
			add edx, dst_pitch
			add esi, msk_pitch
			dec height
			jnz yloop16
			emms
		}
	}
}

void TFMPP::maskClip2_SSE2(const unsigned char *srcp, const unsigned char *dntp, 
	  const unsigned char *maskp, unsigned char *dstp, int src_pitch, int dnt_pitch,
	  int msk_pitch, int dst_pitch, int width, int height)
{
	__asm
	{
		mov eax, srcp
		mov ebx, dntp
		mov esi, maskp
		mov edx, dstp
		mov edi, width
		movdqa xmm7, onesMask
yloop:
		xor ecx, ecx
		align 16
xloop:
		movdqa xmm0, [esi+ecx]
		movdqa xmm1, xmm0
		pand xmm0, [ebx+ecx]
		pxor xmm1, xmm7
		pand xmm1, [eax+ecx]
		por xmm0, xmm1
		movdqa [edx+ecx], xmm0
		add ecx, 16
		cmp ecx, edi
		jl xloop
		add eax, src_pitch
		add ebx, dnt_pitch
		add edx, dst_pitch
		add esi, msk_pitch
		dec height
		jnz yloop
	}
}

TFMPP::TFMPP(PClip _child, int _PP, int _mthresh, const char* _ovr, bool _display, 
	PClip _clip2, bool _usehints, int _opt, IScriptEnvironment* env) : GenericVideoFilter(_child), 
	PP(_PP), mthresh(_mthresh), ovr(_ovr), display(_display), clip2(_clip2), 
	usehints(_usehints), opt(_opt)
{
	setArray = NULL;
	mmask = NULL;
	int w, i, z, b, q, countOvrS;
	char linein[1024], *linep, *linet;
	FILE *f = NULL;
	if (!vi.IsYV12() && !vi.IsYUY2())
		env->ThrowError("TFMPP:  YV12 and YUY2 data only!");
	if (vi.height&1 || vi.width&1)
		env->ThrowError("TFMPP:  height and width must be divisible by 2!");
	if (PP < 2 || PP > 7)
		env->ThrowError("TFMPP:  PP must be set to 2, 3, 4, 5, 6, or 7!");
	if (opt < 0 || opt > 4)
		env->ThrowError("TFMPP:  opt must be set to 0, 1, 2, 3, or 4!");
	if (clip2)
	{
		uC2 = true;
		const VideoInfo &vi2 = clip2->GetVideoInfo();
		if (!vi2.IsYV12() && !vi2.IsYUY2())
			env->ThrowError("TFMPP:  clip2 must be in YUY2 or YV12 colorspace!");
		if ((vi.IsYV12() && vi2.IsYUY2()) || (vi.IsYUY2() && vi2.IsYV12()))
			env->ThrowError("TFMPP:  clip2 colorspace must be the same as input clip!");
		if (vi2.height != vi.height || vi2.width != vi.width)
			env->ThrowError("TFMPP:  clip2 frame dimensions do not match input clip!");
		if (vi2.num_frames != vi.num_frames)
			env->ThrowError("TFMPP:  clip2 does not have the same number of frames as input clip!");
		clip2->SetCacheHints(CACHE_NOTHING, 0);
	}
	else uC2 = false;
	child->SetCacheHints(CACHE_RANGE, 3); // fixed to diameter (07/30/2005)
	mmask = new PlanarFrame(vi, true);
	nfrms = vi.num_frames-1;
	PPS = PP;
	mthreshS = mthresh;
	setArraySize = 0;
	i = 0;
	if (*ovr)
	{
		if ((f = fopen(ovr, "r")) != NULL)
		{
			countOvrS = 0;
			while(fgets(linein, 1024, f) != 0)
			{
				if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' || linein[0] == ';' || linein[0] == '#') 
					continue;
				linep = linein;
				while (*linep != 'M' && *linep != 'P' && *linep != 0) *linep++;
				if (*linep != 0) ++countOvrS;
			}
			fclose(f);
			f = NULL;
			if (countOvrS == 0) { goto emptyovrFM; }
			++countOvrS;
			countOvrS *= 4;
			setArray = (int *)malloc(countOvrS*sizeof(int));
			if (setArray == NULL) env->ThrowError("TFMPP:  malloc failure (setArray)!");
			memset(setArray,255,countOvrS*sizeof(int));
			setArraySize = countOvrS;
			if ((f = fopen(ovr, "r")) != NULL)
			{
				while (fgets(linein, 1024, f) != NULL)
				{
					if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' ||  linein[0] == ';' || linein[0] == '#') 
						continue;
					linep = linein;
					while (*linep != 0 && *linep != ' ' && *linep != ',') *linep++;
					if (*linep == ' ')
					{
						linet = linein;
						while(*linet != 0)
						{
							if (*linet != ' ' && *linet != 10) break;
							*linet++;
						}
						if (*linet == 0) { continue; }
						*linep++;
						if (*linep == 'M' || *linep == 'P')
						{
							sscanf(linein, "%d", &z);
							if (z<0 || z>nfrms) 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TFMPP:  ovr input error (out of range frame #)!");
							}
							linep = linein;
							while (*linep != ' ' && *linep != 0) *linep++;
							if (*linep != 0)
							{
								*linep++;
								if (*linep == 'P' || *linep == 'M')
								{
									q = *linep;
									*linep++; 
									*linep++;
									if (*linep == 0) continue;
									sscanf(linep, "%d", &b);
									if (q == 80 && (b < 0 || b > 7))
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFMPP:  ovr input error (bad PP value)!");
									}
									else if (q != 80 && q != 77) continue;
									setArray[i] = q; ++i;
									setArray[i] = z; ++i;
									setArray[i] = z; ++i;
									setArray[i] = b; ++i;
								}
							}
						}
					}
					else if (*linep == ',')
					{
						while (*linep != ' ' && *linep != 0) *linep++;
						if (*linep == 0) continue;
						*linep++;
						if (*linep == 'P' || *linep == 'M')
						{
							sscanf(linein, "%d,%d", &z, &w);
							if (w == 0) w = nfrms;
							if (z<0 || z>nfrms || w<0 || w>nfrms || w < z) 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TFMPP: ovr input error (invalid frame range)!");
							}
							linep = linein;
							while (*linep != ' ' && *linep != 0) *linep++;
							if (*linep != 0)
							{
								*linep++;
								if (*linep == 'M' || *linep == 'P')
								{
									q = *linep;
									*linep++; 
									*linep++;
									if (*linep == 0) continue;
									sscanf(linep, "%d", &b);
									if (q == 80 && (b < 0 || b > 7))
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFMPP:  ovr input error (bad PP value)!");
									}
									else if (q != 77 && q != 80) continue;
									setArray[i] = q; ++i;
									setArray[i] = z; ++i;
									setArray[i] = w; ++i;
									setArray[i] = b; ++i;
								}
							}
						}
					}
				}
				fclose(f);
				f = NULL;
			}
			else env->ThrowError("TFMPP:  ovr file error (could not open file)!");
		}
		else env->ThrowError("TFMPP:  ovr input error (could not open ovr file)!");
	}
emptyovrFM:
	if (f != NULL) fclose(f);
}

TFMPP::~TFMPP()
{
	if (setArray != NULL) free(setArray);
	if (mmask) delete mmask;
}