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

#include <windows.h>
#include <stdio.h>
#include "avisynth.h"

class TSwitch : public GenericVideoFilter
{
private:
	char buf[512];
	bool debug;
	PClip c1, c2;
	int TSwitch::getHint(PVideoFrame &src, unsigned int &hint, int &htype);
	void TSwitch::putHint(PVideoFrame &dst, unsigned int hint, int htype);

public:
	PVideoFrame __stdcall TSwitch::GetFrame(int n, IScriptEnvironment *env);
	TSwitch::TSwitch(PClip _child, PClip _c1, PClip _c2, bool _debug, 
		IScriptEnvironment *env);
	TSwitch::~TSwitch();
};