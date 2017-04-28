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

#include "TSwitch.h"

TSwitch::TSwitch(PClip _child, PClip _c1, PClip _c2, bool _debug, 
	IScriptEnvironment *env) : GenericVideoFilter(_child), c1(_c1), c2(_c2),
	debug(_debug)
{
	if (!c1 || !c2)
		env->ThrowError("TSwitch:  either c1 or c2 was not specified!");
	VideoInfo vic1 = c1->GetVideoInfo();
	VideoInfo vic2 = c2->GetVideoInfo();
	if (vic1.pixel_type != vic2.pixel_type ||
		vic1.width != vic2.width ||
		vic1.height != vic2.height ||
		vic1.num_frames != vic2.num_frames)
		env->ThrowError("TSwitch:  c1 does not match c2!");
	vi = c1->GetVideoInfo();
}

TSwitch::~TSwitch()
{
	// nothing to free
}

PVideoFrame __stdcall TSwitch::GetFrame(int n, IScriptEnvironment *env)
{
	PVideoFrame src = child->GetFrame(n, env);
	unsigned int hint;
	int htype;
	int ret = getHint(src, hint, htype);
	if (ret < 0)
		env->ThrowError("TSwitch:  no hint detected in stream!");
	PVideoFrame dst;
	if (ret == 0)
	{
		if (debug)
		{
			sprintf(buf,"TSwitch:  frame %d - wasn't deinterlaced (using c1 frame)\n", n);
			OutputDebugString(buf);
		}
		dst = c1->GetFrame(n, env);
	}
	else if (ret == 1)
	{
		if (debug)
		{
			sprintf(buf,"TSwitch:  frame %d - was deinterlaced (using c2 frame)\n", n);
			OutputDebugString(buf);
		}
		dst = c2->GetFrame(n, env);
	}
	else
		env->ThrowError("TSwitch:  internal error!");
	env->MakeWritable(&dst);
	putHint(dst, hint, htype);
	return dst;
}

int TSwitch::getHint(PVideoFrame &src, unsigned int &hint, int &htype)
{
	const unsigned char *srcp = src->GetReadPtr(PLANAR_Y);
	unsigned int i, magic_number = 0;
	hint = 0;
	for (i=0; i<32; ++i)
	{
		magic_number |= ((*srcp++ & 1) << i);
	}
	if (magic_number == 0xdeadfeed)
		htype = 0;
	else if (magic_number == 0xdeaddeed)
		htype = 1;
	else if (magic_number == 0xdeadbead)
		htype = 2;
	else
		return -20;
	for (i=0; i<32; ++i)
	{
		hint |= ((*srcp++ & 1) << i);
	}
	if (hint&0xFFFF0000)
		return -20;
	if (hint&0x40)
		return 1;
	return 0;
}

void TSwitch::putHint(PVideoFrame &dst, unsigned int hint, int htype)
{
	unsigned char *p = dst->GetWritePtr(PLANAR_Y);
	unsigned int i;
	if (htype == 1)
	{
		for (int i=0; i<64; ++i) 
			*p++ &= ~1;
		return;
	}
	unsigned int magic_number;
	if (htype == 2 && (hint&0x80))
	{
		magic_number = 0xdeadbeef;
		hint >>= 8;
	}
	else magic_number = 0xdeadfeed;
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		*p++ |= ((magic_number & (1 << i)) >> i);
	}
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		*p++ |= ((hint & (1 << i)) >> i);
	}
}

AVSValue __cdecl Create_TSwitch(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	return new TSwitch(args[0].AsClip(),args[1].IsClip()?args[1].AsClip():NULL,
		args[2].IsClip()?args[2].AsClip():NULL,args[3].AsBool(false),env);
}