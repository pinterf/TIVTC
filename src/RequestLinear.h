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

#include <windows.h>
#include <stdio.h>
#include "internal.h"
#define VERSION "v1.2"

class RFrame
{
public:
	int num, valid;
	PVideoFrame data;
	RFrame::RFrame();
};

class RequestLinear : public GenericVideoFilter
{
private:
	char buf[512];
	bool debug, rall;
	int last_request, rlim, clim, elim, start_pos;
	RFrame **frames;
	int RequestLinear::mapn(int n);
	int RequestLinear::getCachePos(int n);
	void RequestLinear::insertCacheFrame(int pframe, IScriptEnvironment *env);
	PVideoFrame RequestLinear::findCachedFrame(int pframe, IScriptEnvironment *env);
	void RequestLinear::clearCache(int n, IScriptEnvironment *env);
	PVideoFrame RequestLinear::requestFrame(int n, IScriptEnvironment *env);

public:
	RequestLinear::RequestLinear(PClip _child, int _rlim, int _clim, int _elim, 
		bool _rall, bool _debug, IScriptEnvironment *env);
	RequestLinear::~RequestLinear();
	PVideoFrame __stdcall RequestLinear::GetFrame(int n, IScriptEnvironment *env);
};