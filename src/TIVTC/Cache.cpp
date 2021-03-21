/*
**                    TIVTC for AviSynth 2.6 interface
**
**   TIVTC includes a field matching filter (TFM) and a decimation
**   filter (TDecimate) which can be used together to achieve an
**   IVTC or for other uses. TIVTC currently supports 8 bit planar YUV and
**   YUY2 colorspaces.
**
**   Copyright (C) 2004-2008 Kevin Stone, additional work (C) 2020 pinterf
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

#include "Cache.h"
#include <memory>
#include <cstdlib>
#include <cstring>

// used from TDecimate
CacheFrame::CacheFrame()
{
  num = valid = -20;
}

CacheFilter::CacheFilter(PClip _child, int _size, int _mode, int _cycle, IScriptEnvironment *env) :
  GenericVideoFilter(_child), size(_size), mode(_mode), start_pos(0), cycle(_cycle), frames(NULL)
{
  child->SetCacheHints(CACHE_NOTHING, 0);
  ctframe = -20;
  if (size < 0)
    env->ThrowError("CacheFilter:  size must be >= 0!");
  if (mode != 0 && mode != 1)
    env->ThrowError("CacheFilter:  mode must be set to 0 or 1!");
  if (cycle < 0)
    env->ThrowError("CacheFilter:  cycle must be >= 0!");
  if (size)
  {
    frames = (CacheFrame**)malloc(size * sizeof(CacheFrame*));
    if (!frames) env->ThrowError("CacheFilter:  malloc failure (1)!");
    memset(frames, 0, size * sizeof(CacheFrame*));
    for (int i = 0; i < size; ++i)
    {
      frames[i] = new CacheFrame();
      if (!frames[i])
        env->ThrowError("CacheFilter:  malloc failure (2)!");
    }
  }
}

CacheFilter::~CacheFilter()
{
  if (frames)
  {
    for (int i = 0; i < size; ++i)
    {
      if (frames[i]) delete frames[i];
    }
    free(frames);
  }
}

int CacheFilter::getCachePos(int n)
{
  return (start_pos + n) % size;
}

void CacheFilter::resetCacheStart(int first, int last)
{
  for (int j = first; j <= last; ++j)
  {
    for (int i = 0; i < size; ++i)
    {
      if (frames[i]->num == j && frames[i]->valid == 1)
      {
        start_pos = i - j + first;
        if (start_pos < 0) start_pos += size;
        else if (start_pos >= size) start_pos -= size;
        return;
      }
    }
  }
  clearCache();
}

void CacheFilter::clearCache()
{
  PVideoFrame nframe;
  for (int i = 0; i < size; ++i)
  {
    frames[i]->data = nframe; // force release
    frames[i]->num = -20;
    frames[i]->valid = -20;
  }
  start_pos = 0;
}

PVideoFrame __stdcall CacheFilter::GetFrame(int n, IScriptEnvironment *env)
{
  if (!size) return child->GetFrame(n, env);
  if (ctframe < 0 || ctframe >= vi.num_frames)
    env->ThrowError("CacheFilter:  invalid cframe!");
  processCache(ctframe, n, env);
  PVideoFrame dst;
  if (!copyToFrame(dst, n, env))
    dst = child->GetFrame(n, env);
  return dst;
}

bool CacheFilter::copyToFrame(PVideoFrame &dst, int pframe, IScriptEnvironment *env)
{
  for (int i = 0; i < size; ++i)
  {
    if (frames[i]->num == pframe && frames[i]->valid == 1)
    {
      dst = frames[i]->data;
      return true;
    }
  }
  return false;
}

void CacheFilter::processCache(int cframe, int pframe, IScriptEnvironment *env)
{
  int first;
  if (mode == 0) first = cframe - (size >> 1);
  else first = cframe - 1 - cycle;
  resetCacheStart(first, first + size);
  CacheFrame *cf;
  PVideoFrame nframe;
  for (int i = 0; i < size; ++i)
  {
    cf = frames[getCachePos(i)];
    if (first + i == pframe && (cf->num != pframe || cf->valid != 1))
    {
      cf->data = child->GetFrame(mapn(first + i), env);
      cf->num = first + i;
      cf->valid = 1;
    }
    else if (cf->num != first + i)
    {
      cf->data = nframe; // force release
      cf->num = first + i;
      cf->valid = 0;
    }
  }
}

int CacheFilter::mapn(int n)
{
  if (n < 0) return 0;
  if (n >= vi.num_frames) return vi.num_frames - 1;
  return n;
}

int __stdcall CacheFilter::SetCacheHints(int cachehints, int frame_range)
{
  if (frame_range != -20) ctframe = -20;
  else ctframe = cachehints;
  return 0;
}