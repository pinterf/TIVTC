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
#include <malloc.h>

class TDBuf
{
private:
	unsigned char *y, *u, *v;
	int pitchy, pitchuv;
	int lpitchy, lpitchuv;
	int widthy, widthuv;
	int heighty, heightuv;
	int size, spos;

public:
	int *fnum;
	TDBuf::TDBuf(int _size, int _width, int _height, int _cp);
	TDBuf::~TDBuf();
	const unsigned char* TDBuf::GetReadPtr(int pos, int plane);
	unsigned char* TDBuf::GetWritePtr(int pos, int plane);
	int TDBuf::GetPitch(int plane);
	int TDBuf::GetLPitch(int plane);
	int TDBuf::GetHeight(int plane);
	int TDBuf::GetWidth(int plane);
	int TDBuf::GetPos(int n);
	void TDBuf::resetCacheStart(int n);
};