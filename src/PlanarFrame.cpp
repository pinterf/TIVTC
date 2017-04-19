/*
**   My PlanarFrame class... fast mmx/sse2 YUY2 packed to planar and planar 
**   to packed conversions, and always gives 16 bit alignment for all
**   planes.  Supports YV12/YUY2 frames from avisynth, can do any planar format 
**   internally.
**
**   Copyright (C) 2005-2006 Kevin Stone
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

#include "PlanarFrame.h"

PlanarFrame::PlanarFrame()
{
	ypitch = uvpitch = 0;
	ywidth = uvwidth = 0;
	yheight = uvheight = 0;
	y = u = v = NULL;
	useSIMD = true;
	packed = false;
	cpu = getCPUInfo();
}

PlanarFrame::PlanarFrame(VideoInfo &viInfo)
{
	ypitch = uvpitch = 0;
	ywidth = uvwidth = 0;
	yheight = uvheight = 0;
	y = u = v = NULL;
	useSIMD = true;
	packed = false;
	cpu = getCPUInfo();
	allocSpace(viInfo);
}

PlanarFrame::PlanarFrame(VideoInfo &viInfo, bool _packed)
{
	ypitch = uvpitch = 0;
	ywidth = uvwidth = 0;
	yheight = uvheight = 0;
	y = u = v = NULL;
	useSIMD = true;
	packed = _packed;
	cpu = getCPUInfo();
	allocSpace(viInfo);
}

PlanarFrame::~PlanarFrame()
{
	if (y != NULL) { _aligned_free(y); y = NULL; }
	if (u != NULL) { _aligned_free(u); u = NULL; }
	if (v != NULL) { _aligned_free(v); v = NULL; }
}

bool PlanarFrame::allocSpace(VideoInfo &viInfo)
{
	if (y != NULL) { _aligned_free(y); y = NULL; }
	if (u != NULL) { _aligned_free(u); u = NULL; }
	if (v != NULL) { _aligned_free(v); v = NULL; }
	int height = viInfo.height;
	int width = viInfo.width;
	if (viInfo.IsYV12())
	{
		ypitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT-(width%MIN_ALIGNMENT));
		ywidth = width;
		yheight = height;
		width >>= 1;
		height >>= 1;
		uvpitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT-(width%MIN_ALIGNMENT));
		uvwidth = width;
		uvheight = height;
		y = (unsigned char*)_aligned_malloc(ypitch*yheight,MIN_ALIGNMENT);
		if (y == NULL) return false;
		u = (unsigned char*)_aligned_malloc(uvpitch*uvheight,MIN_ALIGNMENT);
		if (u == NULL) return false;
		v = (unsigned char*)_aligned_malloc(uvpitch*uvheight,MIN_ALIGNMENT);
		if (v == NULL) return false;
		return true;
	}
	else if (viInfo.IsYUY2())
	{
		if (!packed)
		{
			ypitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT-(width%MIN_ALIGNMENT));
			ywidth = width;
			yheight = height;
			width >>= 1;
			uvpitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT-(width%MIN_ALIGNMENT));
			uvwidth = width;
			uvheight = height;
			y = (unsigned char*)_aligned_malloc(ypitch*yheight,MIN_ALIGNMENT);
			if (y == NULL) return false;
			u = (unsigned char*)_aligned_malloc(uvpitch*uvheight,MIN_ALIGNMENT);
			if (u == NULL) return false;
			v = (unsigned char*)_aligned_malloc(uvpitch*uvheight,MIN_ALIGNMENT);
			if (v == NULL) return false;
			return true;
		}
		else
		{
			width *= 2;
			ypitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT-(width%MIN_ALIGNMENT));
			ywidth = width;
			yheight = height;
			y = (unsigned char*)_aligned_malloc(ypitch*yheight,MIN_ALIGNMENT);
			if (y == NULL) return false;
			uvpitch = uvwidth = uvheight = 0;
			u = v = NULL;
			return true;
		}
	}
	return false;
}

bool PlanarFrame::allocSpace(int specs[4])
{
	if (y != NULL) { _aligned_free(y); y = NULL; }
	if (u != NULL) { _aligned_free(u); u = NULL; }
	if (v != NULL) { _aligned_free(v); v = NULL; }
	int height = specs[0];
	int width = specs[2];
	ypitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT-(width%MIN_ALIGNMENT));
	ywidth = width;
	yheight = height;
	height = specs[1];
	width = specs[3];
	uvpitch = width + ((width%MIN_ALIGNMENT) == 0 ? 0 : MIN_ALIGNMENT-(width%MIN_ALIGNMENT));
	uvwidth = width;
	uvheight = height;
	y = (unsigned char*)_aligned_malloc(ypitch*yheight,MIN_ALIGNMENT);
	if (y == NULL) return false;
	u = (unsigned char*)_aligned_malloc(uvpitch*uvheight,MIN_ALIGNMENT);
	if (u == NULL) return false;
	v = (unsigned char*)_aligned_malloc(uvpitch*uvheight,MIN_ALIGNMENT);
	if (v == NULL) return false;
	return true;
}

int PlanarFrame::getCPUInfo()
{
	static const int cpu_saved = checkCPU();
	return cpu_saved;
}

int PlanarFrame::checkCPU()
{
	int cput = 0;
	__asm
	{
		xor edi,edi // zero to begin
		pushfd // check for CPUID.
		pop eax
		or eax,0x00200000
		push eax
		popfd
		pushfd
		pop eax
		and eax,0x00200000
		jz TEST_END
		xor eax,eax // check for features register.
		cpuid
		or eax,eax
		jz TEST_END
		mov eax,1
		cpuid
		test edx,0x00800000 // check MMX
		jz TEST_SSE
		add edi,1 // MMX = bit 0
TEST_SSE:
		test edx,0x02000000 // check SSE
		jz TEST_SSE2
		add edi,2 // iSSE = bit 1
		add edi,4 // SSE = bit 2
TEST_SSE2:
		test edx,0x04000000 // check SSE2
		jz TEST_AMD
		add edi,8 // SSE2 = bit 3
TEST_AMD:  //check for vendor feature register (K6/Athlon).
		mov eax,0x80000000
		cpuid
		mov ecx,0x80000001
		cmp eax,ecx
		jb TEST_END
		mov eax,0x80000001
		cpuid
		test edx,0x80000000 // check 3DNOW
		jz TEST_3DNOW2
		add edi,16 // 3DNOW = bit 4
TEST_3DNOW2:
		test edx,0x40000000 // check 3DNOW2
		jz TEST_SSEMMX
		add edi,32 // 3DNOW2 = bit 5
TEST_SSEMMX:
		test edx,0x00400000 // check iSSE
		jz TEST_END
		add edi,2 // iSSE = bit 1
TEST_END:
		mov cput,edi
	}
	if (cput&0x04) checkSSEOSSupport(cput);
	if (cput&0x08) checkSSE2OSSupport(cput);
	return cput;
}

void PlanarFrame::checkSSEOSSupport(int &cput)
{
	__try
	{
		__asm xorps xmm0,xmm0;
	} 
	__except (EXCEPTION_EXECUTE_HANDLER) 
	{
		if (GetExceptionCode() == 0xC000001Du) cput &= ~0x04;
	}
}

void PlanarFrame::checkSSE2OSSupport(int &cput)
{
	__try
	{
		__asm xorpd xmm0,xmm0;
	} 
	__except (EXCEPTION_EXECUTE_HANDLER) 
	{
		if (GetExceptionCode() == 0xC000001Du) cput &= ~0x08;
	}
}

void PlanarFrame::createPlanar(int yheight, int uvheight, int ywidth, int uvwidth)
{
	int specs[4] = { yheight, uvheight, ywidth, uvwidth };
	allocSpace(specs);
}

void PlanarFrame::createPlanar(int height, int width, int chroma_format)
{
	int specs[4];
	if (chroma_format <= 1) 
	{
		specs[0] = height; specs[1] = height>>1; 
		specs[2] = width; specs[3] = width>>1;  
	}
	else if (chroma_format == 2) 
	{
		specs[0] = height; specs[1] = height; 
		specs[2] = width; specs[3] = width>>1;
	}
	else
	{
		specs[0] = height; specs[1] = height; 
		specs[2] = width; specs[3] = width;  
	}
	allocSpace(specs);
}

void PlanarFrame::createFromProfile(VideoInfo &viInfo)
{
	allocSpace(viInfo);
}

void PlanarFrame::createFromFrame(PVideoFrame &frame, VideoInfo &viInfo)
{
	allocSpace(viInfo);
	copyInternalFrom(frame, viInfo);
}

void PlanarFrame::createFromPlanar(PlanarFrame &frame)
{
	int specs[4] = { frame.yheight, frame.uvheight, frame.ywidth, frame.uvwidth };
	allocSpace(specs);
	copyInternalFrom(frame);
}

void PlanarFrame::copyFrom(PVideoFrame &frame, VideoInfo &viInfo)
{
	copyInternalFrom(frame, viInfo);
}

void PlanarFrame::copyFrom(PlanarFrame &frame)
{
	copyInternalFrom(frame);
}

void PlanarFrame::copyTo(PVideoFrame &frame, VideoInfo &viInfo)
{
	copyInternalTo(frame, viInfo);
}

void PlanarFrame::copyTo(PlanarFrame &frame)
{
	copyInternalTo(frame);
}

void PlanarFrame::copyPlaneTo(PlanarFrame &frame, int plane)
{
	copyInternalPlaneTo(frame, plane);
}

unsigned char* PlanarFrame::GetPtr(int plane)
{
	if (plane == 0) return y;
	if (plane == 1) return u;
	return v;
}

int PlanarFrame::GetWidth(int plane)
{
	if (plane == 0) return ywidth;
	else return uvwidth;
}

int PlanarFrame::GetHeight(int plane)
{
	if (plane == 0) return yheight;
	else return uvheight;
}

int PlanarFrame::GetPitch(int plane)
{
	if (plane == 0) return ypitch;
	else return uvpitch;
}

void PlanarFrame::freePlanar()
{
	if (y != NULL) { _aligned_free(y); y = NULL; }
	if (u != NULL) { _aligned_free(u); u = NULL; }
	if (v != NULL) { _aligned_free(v); v = NULL; }
	ypitch = uvpitch = 0;
	ywidth = uvwidth = 0;
	yheight = uvheight = 0;
	cpu = 0;
}

void PlanarFrame::copyInternalFrom(PVideoFrame &frame, VideoInfo &viInfo)
{
	if (y == NULL || u == NULL || v == NULL) return;
	if (viInfo.IsYV12())
	{
		BitBlt(y, ypitch, frame->GetReadPtr(PLANAR_Y), frame->GetPitch(PLANAR_Y), 
			frame->GetRowSize(PLANAR_Y), frame->GetHeight(PLANAR_Y));
		BitBlt(u, uvpitch, frame->GetReadPtr(PLANAR_U), frame->GetPitch(PLANAR_U), 
			frame->GetRowSize(PLANAR_U), frame->GetHeight(PLANAR_U));
		BitBlt(v, uvpitch, frame->GetReadPtr(PLANAR_V), frame->GetPitch(PLANAR_V), 
			frame->GetRowSize(PLANAR_V), frame->GetHeight(PLANAR_V));	
	}
	else if (viInfo.IsYUY2())
	{
		convYUY2to422(frame->GetReadPtr(),y,u,v,frame->GetPitch(),ypitch,uvpitch,
			viInfo.width,viInfo.height);
	}
}

void PlanarFrame::copyInternalFrom(PlanarFrame &frame)
{
	if (y == NULL || u == NULL || v == NULL) return;
	BitBlt(y, ypitch, frame.y, frame.ypitch, frame.ywidth, frame.yheight);
	BitBlt(u, uvpitch, frame.u, frame.uvpitch, frame.uvwidth, frame.uvheight);
	BitBlt(v, uvpitch, frame.v, frame.uvpitch, frame.uvwidth, frame.uvheight);
}

void PlanarFrame::copyInternalTo(PVideoFrame &frame, VideoInfo &viInfo)
{
	if (y == NULL || u == NULL || v == NULL) return;
	if (viInfo.IsYV12())
	{
		BitBlt(frame->GetWritePtr(PLANAR_Y), frame->GetPitch(PLANAR_Y), y, ypitch, ywidth, yheight);
		BitBlt(frame->GetWritePtr(PLANAR_U), frame->GetPitch(PLANAR_U), u, uvpitch, uvwidth, uvheight);
		BitBlt(frame->GetWritePtr(PLANAR_V), frame->GetPitch(PLANAR_V), v, uvpitch, uvwidth, uvheight);	
	}
	else if (viInfo.IsYUY2())
	{
		conv422toYUY2(y,u,v,frame->GetWritePtr(),ypitch,uvpitch,frame->GetPitch(),ywidth,yheight);
	}
}

void PlanarFrame::copyInternalTo(PlanarFrame &frame)
{
	if (y == NULL || u == NULL || v == NULL) return;
	BitBlt(frame.y, frame.ypitch, y, ypitch, ywidth, yheight);
	BitBlt(frame.u, frame.uvpitch, u, uvpitch, uvwidth, uvheight);
	BitBlt(frame.v, frame.uvpitch, v, uvpitch, uvwidth, uvheight);
}

void PlanarFrame::copyInternalPlaneTo(PlanarFrame &frame, int plane)
{
	if (plane == 0 && y != NULL) 
		BitBlt(frame.y, frame.ypitch, y, ypitch, ywidth, yheight);
	else if (plane == 1 && u != NULL) 
		BitBlt(frame.u, frame.uvpitch, u, uvpitch, uvwidth, uvheight);
	else if (plane == 2 && v != NULL) 
		BitBlt(frame.v, frame.uvpitch, v, uvpitch, uvwidth, uvheight);
}

void PlanarFrame::copyChromaTo(PlanarFrame &dst)
{
	BitBlt(dst.u, dst.uvpitch, u, uvpitch, dst.uvwidth, dst.uvheight);
	BitBlt(dst.v, dst.uvpitch, v, uvpitch, dst.uvwidth, dst.uvheight);
}

void PlanarFrame::copyToForBMP(PVideoFrame &dst, VideoInfo &viInfo)
{
	unsigned char *dstp = dst->GetWritePtr(PLANAR_Y);
	if (viInfo.IsYV12())
	{
		int out_pitch = (ywidth+3) & -4;
		BitBlt(dstp, out_pitch, y, ypitch, ywidth, yheight);
		BitBlt(dstp+(out_pitch*yheight), out_pitch>>1, v, uvpitch, uvwidth, uvheight);
		BitBlt(dstp+(out_pitch*yheight)+((out_pitch>>1)*uvheight), out_pitch>>1, u, uvpitch, uvwidth, uvheight);
	}
	else 
	{
		int out_pitch = (dst->GetRowSize(PLANAR_Y)+3) & -4;
		conv422toYUY2(y,u,v,dstp,ypitch,uvpitch,out_pitch,viInfo.width,viInfo.height);
	}
}

PlanarFrame& PlanarFrame::operator=(PlanarFrame &ob2)
{
	cpu = ob2.cpu;
	ypitch = ob2.ypitch;
	yheight = ob2.yheight;
	ywidth = ob2.ywidth;
	uvpitch = ob2.uvpitch;
	uvheight = ob2.uvheight;
	uvwidth = ob2.uvwidth;
	this->copyFrom(ob2);
	return *this;
}

void PlanarFrame::convYUY2to422(const unsigned char *src, unsigned char *py, unsigned char *pu, 
		unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
	if ((cpu&CPU_SSE2) && useSIMD && !((int(src)|pitch1)&15))
		convYUY2to422_SSE2(src, py, pu, pv, pitch1, pitch2Y, pitch2UV, width, height);
	else if ((cpu&CPU_MMX) && useSIMD) 
		convYUY2to422_MMX(src, py, pu, pv, pitch1, pitch2Y, pitch2UV, width, height);
	else
	{
		width >>= 1;
		for (int y=0; y<height; ++y)
		{
			for (int x=0; x<width; ++x)
			{
				py[x<<1] = src[x<<2];
				pu[x] = src[(x<<2)+1];
				py[(x<<1)+1] = src[(x<<2)+2];
				pv[x] = src[(x<<2)+3];
			}
			py += pitch2Y;
			pu += pitch2UV;
			pv += pitch2UV;
			src += pitch1;
		}
	}
}

void PlanarFrame::convYUY2to422_MMX(const unsigned char *src, unsigned char *py, unsigned char *pu, 
		unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
	__asm
	{
		mov edi,src
		mov ebx,py
		mov edx,pu
		mov esi,pv
		mov ecx,width
		shr ecx,1
		movq mm5,Ymask
yloop:
		xor eax,eax
		align 16
xloop:
		movq mm0,[edi+eax*4]   ;VYUYVYUY
		movq mm1,[edi+eax*4+8] ;VYUYVYUY
		movq mm2,mm0           ;VYUYVYUY
		movq mm3,mm1           ;VYUYVYUY
		pand mm0,mm5           ;0Y0Y0Y0Y
		psrlw mm2,8 	       ;0V0U0V0U
		pand mm1,mm5           ;0Y0Y0Y0Y
		psrlw mm3,8            ;0V0U0V0U
		packuswb mm0,mm1       ;YYYYYYYY
		packuswb mm2,mm3       ;VUVUVUVU
		movq mm4,mm2           ;VUVUVUVU
		pand mm2,mm5           ;0U0U0U0U
		psrlw mm4,8            ;0V0V0V0V
		packuswb mm2,mm2       ;xxxxUUUU
		packuswb mm4,mm4       ;xxxxVVVV
		movq [ebx+eax*2],mm0   ;store y
		movd [edx+eax],mm2     ;store u
		movd [esi+eax],mm4     ;store v
		add eax,4
		cmp eax,ecx
		jl xloop
		add edi,pitch1
		add ebx,pitch2Y
		add edx,pitch2UV
		add esi,pitch2UV
		dec height
		jnz yloop
		emms
	}
}

void PlanarFrame::convYUY2to422_SSE2(const unsigned char *src, unsigned char *py, unsigned char *pu, 
		unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
	__asm
	{
		mov edi,src
		mov ebx,py
		mov edx,pu
		mov esi,pv
		mov ecx,width
		shr ecx,1
		movdqa xmm3,Ymask
yloop:
		xor eax,eax
		align 16
xloop:
		movdqa xmm0,[edi+eax*4] ;VYUYVYUYVYUYVYUY
		movdqa xmm1,xmm0        ;VYUYVYUYVYUYVYUY
		pand xmm0,xmm3          ;0Y0Y0Y0Y0Y0Y0Y0Y
		psrlw xmm1,8	        ;0V0U0V0U0V0U0V0U
		packuswb xmm0,xmm0      ;xxxxxxxxYYYYYYYY
		packuswb xmm1,xmm1      ;xxxxxxxxVUVUVUVU
		movdqa xmm2,xmm1        ;xxxxxxxxVUVUVUVU
		pand xmm1,xmm3          ;xxxxxxxx0U0U0U0U
		psrlw xmm2,8            ;xxxxxxxx0V0V0V0V
		packuswb xmm1,xmm1      ;xxxxxxxxxxxxUUUU
		packuswb xmm2,xmm2      ;xxxxxxxxxxxxVVVV
		movlpd [ebx+eax*2],xmm0 ;store y
		movd [edx+eax],xmm1     ;store u
		movd [esi+eax],xmm2     ;store v
		add eax,4
		cmp eax,ecx
		jl xloop
		add edi,pitch1
		add ebx,pitch2Y
		add edx,pitch2UV
		add esi,pitch2UV
		dec height
		jnz yloop
	}
}

void PlanarFrame::conv422toYUY2(unsigned char *py, unsigned char *pu, unsigned char *pv, 
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
	if ((cpu&CPU_SSE2) && useSIMD && !(int(dst)&15))
		conv422toYUY2_SSE2(py, pu, pv, dst, pitch1Y, pitch1UV, pitch2, width, height);
	else if ((cpu&CPU_MMX) && useSIMD) 
		conv422toYUY2_MMX(py, pu, pv, dst, pitch1Y, pitch1UV, pitch2, width, height);
	else
	{
		width >>= 1;
		for (int y=0; y<height; ++y)
		{
			for (int x=0; x<width; ++x)
			{
				dst[x<<2] = py[x<<1];
				dst[(x<<2)+1] = pu[x];
				dst[(x<<2)+2] = py[(x<<1)+1];
				dst[(x<<2)+3] = pv[x];
			}
			py += pitch1Y;
			pu += pitch1UV;
			pv += pitch1UV;
			dst += pitch2;
		}
	}
}

void PlanarFrame::conv422toYUY2_MMX(unsigned char *py, unsigned char *pu, unsigned char *pv, 
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
	__asm
	{
		mov ebx,py
		mov edx,pu
		mov esi,pv
		mov edi,dst
		mov ecx,width
		shr ecx,1
yloop:
		xor eax,eax
		align 16
xloop:
		movq mm0,[ebx+eax*2]   ;YYYYYYYY
		movd mm1,[edx+eax]     ;0000UUUU
		movd mm2,[esi+eax]     ;0000VVVV
		movq mm3,mm0           ;YYYYYYYY
		punpcklbw mm1,mm2      ;VUVUVUVU
		punpcklbw mm0,mm1      ;VYUYVYUY
		punpckhbw mm3,mm1      ;VYUYVYUY
		movq [edi+eax*4],mm0   ;store
		movq [edi+eax*4+8],mm3 ;store
		add eax,4
		cmp eax,ecx
		jl xloop
		add ebx,pitch1Y
		add edx,pitch1UV
		add esi,pitch1UV
		add edi,pitch2
		dec height
		jnz yloop
		emms
	}
}

void PlanarFrame::conv422toYUY2_SSE2(unsigned char *py, unsigned char *pu, unsigned char *pv, 
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
	__asm
	{
		mov ebx,py
		mov edx,pu
		mov esi,pv
		mov edi,dst
		mov ecx,width
		shr ecx,1
yloop:
		xor eax,eax
		align 16
xloop:
		movlpd xmm0,[ebx+eax*2] ;????????YYYYYYYY
		movd xmm1,[edx+eax]     ;000000000000UUUU
		movd xmm2,[esi+eax]     ;000000000000VVVV
		punpcklbw xmm1,xmm2     ;00000000VUVUVUVU
		punpcklbw xmm0,xmm1     ;VYUYVYUYVYUYVYUY
		movdqa [edi+eax*4],xmm0 ;store
		add eax,4
		cmp eax,ecx
		jl xloop
		add ebx,pitch1Y
		add edx,pitch1UV
		add esi,pitch1UV
		add edi,pitch2
		dec height
		jnz yloop
	}
}

// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

// from Avisynth 2.55 source...
// copied so we don't need an
// IScriptEnvironment pointer 
// to call it

void PlanarFrame::BitBlt(unsigned char* dstp, int dst_pitch, const unsigned char* srcp, 
			int src_pitch, int row_size, int height) 
{
	if (!height || !row_size) return;
	if (cpu&CPU_ISSE && useSIMD) 
	{
		if (height == 1 || (src_pitch == dst_pitch && dst_pitch == row_size)) 
			memcpy_amd(dstp, srcp, row_size*height);
		else asm_BitBlt_ISSE(dstp,dst_pitch,srcp,src_pitch,row_size,height);
	}
	else if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size)) 
		memcpy(dstp, srcp, src_pitch * height);
	else 
	{
		for (int y=height; y>0; --y) 
		{
			memcpy(dstp, srcp, row_size);
			dstp += dst_pitch;
			srcp += src_pitch;
		}
	}
}

  /*****************************
  * Assembler bitblit by Steady
   *****************************/

void PlanarFrame::asm_BitBlt_ISSE(unsigned char* dstp, int dst_pitch, 
	const unsigned char* srcp, int src_pitch, int row_size, int height) 
{
	if (row_size == 0 || height == 0) return;
	const unsigned char* srcStart = srcp+src_pitch*(height-1);
	unsigned char* dstStart = dstp+dst_pitch*(height-1);
	if(row_size < 64) 
	{
		_asm 
		{
			mov   esi,srcStart
			mov   edi,dstStart
			mov   edx,row_size
			dec   edx
			mov   ebx,height
			align 16
memoptS_rowloop:
			mov   ecx,edx
memoptS_byteloop:
			mov   AL,[esi+ecx]
			mov   [edi+ecx],AL
			sub   ecx,1
			jnc   memoptS_byteloop
			sub   esi,src_pitch
			sub   edi,dst_pitch
			dec   ebx
			jne   memoptS_rowloop
		};
		return;
	}
	else if ((int(dstp) | row_size | src_pitch | dst_pitch) & 7) 
	{
		_asm
		{
			mov   esi,srcStart
			mov   AL,[esi]
			mov   edi,dstStart
			mov   edx,row_size
			mov   ebx,height
			align 16
memoptU_rowloop:
			mov   ecx,edx
			dec   ecx
			add   ecx,esi
			and   ecx,~63
memoptU_prefetchloop:
			mov   AX,[ecx]
			sub   ecx,64
			cmp   ecx,esi
			jae   memoptU_prefetchloop
			movq    mm6,[esi]
			movntq  [edi],mm6
			mov   eax,edi
			neg   eax
			mov   ecx,eax
			and   eax,63
			and   ecx,7
			align 16
memoptU_prewrite8loop:
			cmp   ecx,eax
			jz    memoptU_pre8done
			movq    mm7,[esi+ecx]
			movntq  [edi+ecx],mm7
			add   ecx,8
			jmp   memoptU_prewrite8loop
			align 16
memoptU_write64loop:
			movntq  [edi+ecx-64],mm0
			movntq  [edi+ecx-56],mm1
			movntq  [edi+ecx-48],mm2
			movntq  [edi+ecx-40],mm3
			movntq  [edi+ecx-32],mm4
			movntq  [edi+ecx-24],mm5
			movntq  [edi+ecx-16],mm6
			movntq  [edi+ecx- 8],mm7
memoptU_pre8done:
			add   ecx,64
			cmp   ecx,edx
			ja    memoptU_done64
			movq    mm0,[esi+ecx-64]
			movq    mm1,[esi+ecx-56]
			movq    mm2,[esi+ecx-48]
			movq    mm3,[esi+ecx-40]
			movq    mm4,[esi+ecx-32]
			movq    mm5,[esi+ecx-24]
			movq    mm6,[esi+ecx-16]
			movq    mm7,[esi+ecx- 8]
			jmp   memoptU_write64loop
memoptU_done64:
			sub     ecx,64
			align 16
memoptU_write8loop:
			add     ecx,8
			cmp     ecx,edx
			ja      memoptU_done8
			movq    mm0,[esi+ecx-8]
			movntq  [edi+ecx-8],mm0
			jmp   memoptU_write8loop
memoptU_done8:
			movq    mm1,[esi+edx-8]
			movntq  [edi+edx-8],mm1
			sub   esi,src_pitch
			sub   edi,dst_pitch
			dec   ebx
			jne   memoptU_rowloop
			sfence
			emms
		};
		return;
	}
	else 
	{
		_asm 
		{
			mov   esi,srcStart
			mov   edi,dstStart
			mov   ebx,height
			mov   edx,row_size
			align 16
memoptA_rowloop:
			mov   ecx,edx
			dec   ecx
			add   ecx,esi
			and   ecx,~63
			align 16
memoptA_prefetchloop:
			mov   AX,[ecx]
			sub   ecx,64
			cmp   ecx,esi
			jae   memoptA_prefetchloop
			mov   eax,edi
			xor   ecx,ecx
			neg   eax
			and   eax,63
			align 16
memoptA_prewrite8loop:
			cmp   ecx,eax
			jz    memoptA_pre8done
			movq    mm7,[esi+ecx]
			movntq  [edi+ecx],mm7
			add   ecx,8
			jmp   memoptA_prewrite8loop
			align 16
			memoptA_write64loop:
			movntq  [edi+ecx-64],mm0
			movntq  [edi+ecx-56],mm1
			movntq  [edi+ecx-48],mm2
			movntq  [edi+ecx-40],mm3
			movntq  [edi+ecx-32],mm4
			movntq  [edi+ecx-24],mm5
			movntq  [edi+ecx-16],mm6
			movntq  [edi+ecx- 8],mm7
memoptA_pre8done:
			add   ecx,64
			cmp   ecx,edx
			ja    memoptA_done64
			movq    mm0,[esi+ecx-64]
			movq    mm1,[esi+ecx-56]
			movq    mm2,[esi+ecx-48]
			movq    mm3,[esi+ecx-40]
			movq    mm4,[esi+ecx-32]
			movq    mm5,[esi+ecx-24]
			movq    mm6,[esi+ecx-16]
			movq    mm7,[esi+ecx- 8]
			jmp   memoptA_write64loop
memoptA_done64:
			sub   ecx,64
			align 16
memoptA_write8loop:
			add   ecx,8
			cmp   ecx,edx
			ja    memoptA_done8
			movq    mm7,[esi+ecx-8]
			movntq  [edi+ecx-8],mm7
			jmp   memoptA_write8loop
memoptA_done8:
			sub   esi,src_pitch
			sub   edi,dst_pitch
			dec   ebx
			jne   memoptA_rowloop
			sfence
			emms
		};
		return;
	}
}