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

#include "RequestLinear.h"

PVideoFrame __stdcall RequestLinear::GetFrame(int n, IScriptEnvironment *env)
{
	n = mapn(n);
	if (debug)
	{
		sprintf(buf, "RequestLinear:  frame %d -- last_request = %d\n", n, last_request);
		OutputDebugString(buf);
	}
	if (clim <= 0)
	{
		if (n > last_request && n-last_request <= rlim)
		{
			for (int i=last_request+1; i<n; ++i)
				requestFrame(i, env);
		}
		else if (n <= last_request && n <= rlim)
		{
			for (int i=0; i<n; ++i)
				requestFrame(i, env);
		}
		else if (rall)
		{
			int start = n > last_request ? last_request+1 : 0;
			for (int i=start; i<n; ++i)
				requestFrame(i, env);
		}
		else if (elim > 0)
		{
			for (int i=max(0,n-elim); i<n; ++i)
				requestFrame(i, env);
		}
		last_request = n;
		return requestFrame(n, env);
	}
	if (n > last_request)
	{
		if (n-last_request <= rlim || rall)
		{
			for (int i=last_request+1; i<=n; ++i)
				insertCacheFrame(i, env);
		}
		else 
		{
			clearCache(n, env);
			for (int i=max(0,n-elim); i<=n; ++i)
				insertCacheFrame(i, env);
		}
		last_request = n;
		return findCachedFrame(n, env);
	}
	if (last_request-n < clim)
		return findCachedFrame(n, env);
	if (n <= rlim || rall)
	{
		for (int i=0; i<=n; ++i)
			insertCacheFrame(i, env);
	}
	else 
	{
		clearCache(n, env);
		for (int i=max(0,n-elim); i<=n; ++i)
			insertCacheFrame(i, env);
	}
	last_request = n;
	return findCachedFrame(n, env);
}

RequestLinear::RequestLinear(PClip _child, int _rlim, int _clim, int _elim, bool _rall, 
	bool _debug, IScriptEnvironment *env) : GenericVideoFilter(_child), rlim(_rlim),
	clim(_clim), elim(_elim), rall(_rall), debug(_debug)
{
	frames = NULL;
	start_pos = 0;
	if (clim < 0)
		env->ThrowError("RequestLinear:  clim must be >= 0!");
	if (rlim < 0)
		env->ThrowError("RequestLinear:  rlim must be >= 0!");
	if (elim < 0)
		env->ThrowError("RequestLinear:  elim must be >= 0!");
	if (clim)
	{
		frames = (RFrame**)malloc(clim*sizeof(RFrame*));
		if (!frames) env->ThrowError("RequestLinear:  malloc failure (frames)!");
		memset(frames,0,clim*sizeof(RFrame*));
		for (int i=0; i<clim; ++i)
		{
			frames[i] = new RFrame();
			if (!frames[i])
				env->ThrowError("RequestLinear:  malloc failure (frames[i])!");
		}
	}
	last_request = -2098;
	child->SetCacheHints(CACHE_NOTHING, 0);
}

PVideoFrame RequestLinear::requestFrame(int n, IScriptEnvironment *env)
{
	if (debug)
	{
		sprintf(buf, "RequestLinear:  requesting frame %d\n", n);
		OutputDebugString(buf);
	}
	return child->GetFrame(n, env);
}

RequestLinear::~RequestLinear()
{
	if (frames)
	{
		for (int i=0; i<clim; ++i)
		{
			if (frames[i]) delete frames[i];
		}
		free(frames);
	}
}

RFrame::RFrame() 
{
	num = valid = -20;
}

void RequestLinear::clearCache(int n, IScriptEnvironment *env)
{
	if (debug)
	{
		sprintf(buf, "RequestLinear:  clearing cache (%d)\n", n);
		OutputDebugString(buf);
	}
	PVideoFrame nframe;
	for (int i=0; i<clim; ++i)
	{
		frames[i]->data = nframe; // force release
		frames[i]->num = -20;
		frames[i]->valid = -20;
	}
	start_pos = 0;
}

PVideoFrame RequestLinear::findCachedFrame(int pframe, IScriptEnvironment *env)
{
	for (int i=0; i<clim; ++i)
	{
		if (frames[i]->num == pframe && frames[i]->valid == 1)
		{
			if (debug)
			{
				sprintf(buf, "RequestLinear:  found cached frame %d\n", pframe);
				OutputDebugString(buf);
			}
			return frames[i]->data;
		}
	}
	env->ThrowError("RequestLinear:  internal error (frame not cached)!");
	return NULL;
}

void RequestLinear::insertCacheFrame(int pframe, IScriptEnvironment *env)
{
	RFrame *cf;
	PVideoFrame nframe;
	int first = pframe-clim+1;
	start_pos = (start_pos+1)%clim;
	if (debug)
	{
		sprintf(buf, "RequestLinear:  cache inserting frame %d\n", pframe);
		OutputDebugString(buf);
	}
	for (int i=0; i<clim; ++i)
	{
		cf = frames[getCachePos(i)];
		if (first+i == pframe && (cf->num != pframe || cf->valid != 1))
		{
			cf->data = requestFrame(mapn(first+i), env);
			cf->num = first+i;
			cf->valid = 1;
		}
		else if (cf->num != first+i)
		{
			cf->data = nframe; // force release
			cf->num = first+i;
			cf->valid = 0;
		}
	}
}

int RequestLinear::getCachePos(int n)
{
	return (start_pos+n)%clim;
}

int RequestLinear::mapn(int n)
{
	if (n < 0) return 0;
	if (n >= vi.num_frames) return vi.num_frames-1;
	return n;
}

AVSValue __cdecl Create_RequestLinear(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	return new RequestLinear(args[0].AsClip(),args[1].AsInt(50),args[2].AsInt(10),
		args[3].AsInt(5),args[4].AsBool(false),args[5].AsBool(false),env);
}