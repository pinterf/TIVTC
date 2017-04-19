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
#include <xmmintrin.h>
#include "avisynth.h"

bool checkForIntelP4();
bool IsIntelP4();
void fmemset(long cpu, unsigned char *p, int sizec, int opt, int val=0);
void fmemset_8_MMX(unsigned char* p, int sizec, __int64 val);
void fmemset_8_iSSE(unsigned char* p, int sizec, __int64 val);
void fmemset_16_MMX(unsigned char* p, int sizec, __int64 val);
void fmemset_16_iSSE(unsigned char* p, int sizec, __int64 val);
void fmemset_16_SSE2(unsigned char* p, int sizec, __m128 val);