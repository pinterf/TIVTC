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
#include "avisynth.h"

class CacheFrame
{
public:
	int num, valid;
	PVideoFrame data;
	CacheFrame::CacheFrame();
};

class CacheFilter : public GenericVideoFilter
{
private:
	int size, mode, start_pos, ctframe, cycle;
	CacheFrame **frames;
	int mapn(int n);
	void CacheFilter::clearCache();

public:
	PVideoFrame __stdcall CacheFilter::GetFrame(int n, IScriptEnvironment *env);
	CacheFilter::~CacheFilter();
	CacheFilter::CacheFilter(PClip _child, int _size, int _mode, int _cycle, 
		IScriptEnvironment *env);
	void CacheFilter::resetCacheStart(int first, int last);
	int CacheFilter::getCachePos(int n);
	bool CacheFilter::copyToFrame(PVideoFrame &dst, int pframe, IScriptEnvironment *env);
	void CacheFilter::processCache(int cframe, int pframe, IScriptEnvironment *env);
	void __stdcall CacheFilter::SetCacheHints(int cachehints, int frame_range);
};