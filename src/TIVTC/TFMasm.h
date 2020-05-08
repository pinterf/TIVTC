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
#ifndef __TFMASM_H__
#define __TFMASM_H__

#include <windows.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include "internal.h"

void checkSceneChangeYUY2_1_SSE2(const unsigned char* prvp, const unsigned char* srcp,
  int height, int width, int prv_pitch, int src_pitch, unsigned long& diffp);
void checkSceneChangeYUY2_2_SSE2(const unsigned char* prvp, const unsigned char* srcp,
  const unsigned char* nxtp, int height, int width, int prv_pitch, int src_pitch,
  int nxt_pitch, unsigned long& diffp, unsigned long& diffn);

void checkSceneChangeYV12_1_SSE2(const unsigned char* prvp, const unsigned char* srcp,
  int height, int width, int prv_pitch, int src_pitch, unsigned long& diffp);
void checkSceneChangeYV12_2_SSE2(const unsigned char* prvp, const unsigned char* srcp,
  const unsigned char* nxtp, int height, int width, int prv_pitch, int src_pitch,
  int nxt_pitch, unsigned long& diffp, unsigned long& diffn);

#endif // TFMASM_H__
