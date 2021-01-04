/*
**                TDeinterlace for AviSynth 2.6 interface
**
**   TDeinterlace is a bi-directionally motion adaptive deinterlacer.
**   It also uses a couple modified forms of ela interpolation which
**   help to reduce "jaggy" edges in places where interpolation must
**   be used. TDeinterlace currently supports 8 bit planar YUV and YUY2 colorspaces.
**
**   Copyright (C) 2004-2007 Kevin Stone, additional work (C) 2020 pinterf
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

#include <stdlib.h>
#include <stdint.h>
#include <vector>

class TDBuf
{
private:
  uint8_t *y, *u, *v;
  int pitchy, pitchuv;
  int lpitchy, lpitchuv;
  int widthy, widthuv;
  int heighty, heightuv;
  int size, spos;

public:
  std::vector<int> fnum;
  TDBuf(int _size, int _width, int _height, int _cp, int planarType);
  ~TDBuf();
  const uint8_t* GetReadPtr(int pos, int plane);
  uint8_t* GetWritePtr(int pos, int plane);
  int GetPitch(int plane);
  int GetLPitch(int plane);
  int GetHeight(int plane);
  int GetWidth(int plane);
  int GetPos(int n);
  void resetCacheStart(int n);
};