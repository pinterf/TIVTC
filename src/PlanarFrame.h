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

#ifndef __PlanarFrame_H__
#define __PlanarFrame_H__

#include <windows.h>
#include <malloc.h>
#include "internal.h"
#include "memcpy_amd.h"

#define MIN_ALIGNMENT 16
#define CPU_MMX 0x00000001
#define CPU_ISSE 0x00000002
#define CPU_SSE 0x00000004
#define CPU_SSE2 0x00000008
#define CPU_3DNOW 0x00000010
#define CPU_3DNOW2 0x00000020
#define PLANAR_420 1
#define PLANAR_422 2
#define PLANAR_444 3

__declspec(align(16)) const __int64 Ymask[2] = { 0x00FF00FF00FF00FF, 0x00FF00FF00FF00FF };

class PlanarFrame
{
private:
	int cpu;
	bool useSIMD, packed;
	int ypitch, uvpitch;
	int ywidth, uvwidth;
	int yheight, uvheight;
	unsigned char *y, *u, *v;
	bool PlanarFrame::allocSpace(VideoInfo &viInfo);
	bool PlanarFrame::allocSpace(int specs[4]);
	int PlanarFrame::getCPUInfo();
	int PlanarFrame::checkCPU();
	void PlanarFrame::checkSSEOSSupport(int &cput);
	void PlanarFrame::checkSSE2OSSupport(int &cput);
	void PlanarFrame::copyInternalFrom(PVideoFrame &frame, VideoInfo &viInfo);
	void PlanarFrame::copyInternalFrom(PlanarFrame &frame);
	void PlanarFrame::copyInternalTo(PVideoFrame &frame, VideoInfo &viInfo);
	void PlanarFrame::copyInternalTo(PlanarFrame &frame);
	void PlanarFrame::copyInternalPlaneTo(PlanarFrame &frame, int plane);
	static void PlanarFrame::asm_BitBlt_ISSE(unsigned char* dstp, int dst_pitch, 
		const unsigned char* srcp, int src_pitch, int row_size, int height);
	void PlanarFrame::convYUY2to422(const unsigned char *src, unsigned char *py, unsigned char *pu, 
		unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height);
	void PlanarFrame::convYUY2to422_MMX(const unsigned char *src, unsigned char *py, unsigned char *pu, 
		unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height);
	void PlanarFrame::convYUY2to422_SSE2(const unsigned char *src, unsigned char *py, unsigned char *pu, 
		unsigned char *pv, int pitch1, int pitch2Y, int pitch2UV, int width, int height);
	void PlanarFrame::conv422toYUY2(unsigned char *py, unsigned char *pu, unsigned char *pv, 
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);
	void PlanarFrame::conv422toYUY2_SSE2(unsigned char *py, unsigned char *pu, unsigned char *pv, 
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);
	void PlanarFrame::conv422toYUY2_MMX(unsigned char *py, unsigned char *pu, unsigned char *pv, 
		unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);

public:
	PlanarFrame::PlanarFrame();
	PlanarFrame::PlanarFrame(VideoInfo &viInfo);
	PlanarFrame::PlanarFrame(VideoInfo &viInfo, bool _packed);
	PlanarFrame::~PlanarFrame();
	void PlanarFrame::createPlanar(int yheight, int uvheight, int ywidth, int uvwidth);
	void PlanarFrame::createPlanar(int height, int width, int chroma_format);
	void PlanarFrame::createFromProfile(VideoInfo &viInfo);
	void PlanarFrame::createFromFrame(PVideoFrame &frame, VideoInfo &viInfo);
	void PlanarFrame::createFromPlanar(PlanarFrame &frame);
	void PlanarFrame::copyFrom(PVideoFrame &frame, VideoInfo &viInfo);
	void PlanarFrame::copyTo(PVideoFrame &frame, VideoInfo &viInfo);
	void PlanarFrame::copyFrom(PlanarFrame &frame);
	void PlanarFrame::copyTo(PlanarFrame &frame);
	void PlanarFrame::copyChromaTo(PlanarFrame &dst);
	void PlanarFrame::copyToForBMP(PVideoFrame &dst, VideoInfo &viInfo);
	void PlanarFrame::copyPlaneTo(PlanarFrame &dst, int plane);
	void PlanarFrame::freePlanar();
	unsigned char* PlanarFrame::GetPtr(int plane=0);
	int PlanarFrame::GetWidth(int plane=0);
	int PlanarFrame::GetHeight(int plane=0);
	int PlanarFrame::GetPitch(int plane=0);
	void PlanarFrame::BitBlt(unsigned char* dstp, int dst_pitch, const unsigned char* srcp, 
			int src_pitch, int row_size, int height);
	PlanarFrame& PlanarFrame::operator=(PlanarFrame &ob2);
};

#endif