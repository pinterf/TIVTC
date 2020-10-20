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

#include <stdio.h>
#include "internal.h"
#define VERSION "v1.4"

class RFrame
{
public:
  int num, valid;
  PVideoFrame data;
  RFrame();
};

class RequestLinear : public GenericVideoFilter
{
private:
  char buf[512];
  int last_request, rlim, clim, elim;
  bool rall, debug;
  int start_pos;
  RFrame **frames;
  int mapn(int n);
  int getCachePos(int n);
  void insertCacheFrame(int pframe, IScriptEnvironment *env);
  PVideoFrame findCachedFrame(int pframe, IScriptEnvironment *env);
  void clearCache(int n, IScriptEnvironment *env);
  PVideoFrame requestFrame(int n, IScriptEnvironment *env);

public:
  RequestLinear(PClip _child, int _rlim, int _clim, int _elim,
    bool _rall, bool _debug, IScriptEnvironment *env);
  ~RequestLinear();
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_SERIALIZED : 0;
  }
};