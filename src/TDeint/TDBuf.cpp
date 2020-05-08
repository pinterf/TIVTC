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

#include "TDBuf.h"

TDBuf::TDBuf(int _size, int _width, int _height, int _cp, int planarType) : size(_size)
{
  constexpr int FRAME_ALIGN = 64;
  y = u = v = NULL;
  fnum.resize(size);
  if (_cp == 3) // count of planes YV12, YV16, YV24 planar
  {
    widthy = _width;
    widthuv = planarType == 444 ? _width : planarType == 411 ? _width / 4 : _width / 2;
    heighty = _height;
    heightuv = planarType == 420 ? _height / 2 : _height;
    lpitchy = ((widthy + (FRAME_ALIGN - 1)) / FRAME_ALIGN) * FRAME_ALIGN;
    lpitchuv = ((widthuv + (FRAME_ALIGN - 1)) / FRAME_ALIGN) * FRAME_ALIGN;
    pitchy = lpitchy*size;
    pitchuv = lpitchuv*size;
  }
  else
  {
    // YUY2
    widthy = _width * 2; // *2 row size, no chroma plane, interleaved YUYV YUYV...
    heighty = _height;
    lpitchy = ((widthy + (FRAME_ALIGN - 1)) / FRAME_ALIGN) * FRAME_ALIGN;
    pitchy = lpitchy*size;
    heightuv = widthuv = pitchuv = lpitchuv = 0;
  }
  if (size)
  {
    for (int i = 0; i < size; ++i) fnum[i] = -999999999;
    y = (unsigned char*)_aligned_malloc(pitchy*heighty, FRAME_ALIGN);
    if (_cp == 3)
    {
      u = (unsigned char*)_aligned_malloc(pitchuv*heightuv, FRAME_ALIGN);
      v = (unsigned char*)_aligned_malloc(pitchuv*heightuv, FRAME_ALIGN);
    }
  }
  spos = 0;
}

TDBuf::~TDBuf()
{
  if (y) _aligned_free(y);
  if (u) _aligned_free(u);
  if (v) _aligned_free(v);
}

const unsigned char* TDBuf::GetReadPtr(int pos, int plane)
{
  if (plane == 0) return y + pos*lpitchy;
  if (plane == 1) return u + pos*lpitchuv;
  return v + pos*lpitchuv;
}

unsigned char* TDBuf::GetWritePtr(int pos, int plane)
{
  if (plane == 0) return y + pos*lpitchy;
  if (plane == 1) return u + pos*lpitchuv;
  return v + pos*lpitchuv;
}

int TDBuf::GetPitch(int plane)
{
  if (plane == 0) return pitchy;
  return pitchuv;
}

int TDBuf::GetLPitch(int plane)
{
  if (plane == 0) return lpitchy;
  return lpitchuv;
}

int TDBuf::GetHeight(int plane)
{
  if (plane == 0) return heighty;
  return heightuv;
}

int TDBuf::GetWidth(int plane)
{
  if (plane == 0) return widthy;
  return widthuv;
}

int TDBuf::GetPos(int n)
{
  return (spos + n) % size;
}

void TDBuf::resetCacheStart(int n)
{
  for (int i = 0; i < size; ++i)
  {
    if (fnum[i] == n)
    {
      spos = i;
      return;
    }
  }
  for (int i = 0; i < size; ++i)
    fnum[i] = -999999999;
  spos = 0;
}