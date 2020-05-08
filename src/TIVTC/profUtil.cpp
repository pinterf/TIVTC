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

#if 0

#include "profUtil.h"

static void getTimeRDTSC(int64_t &time)
{
  int timeL, timeH;
  __asm
  {
    rdtsc;
    mov timeL, eax;
    mov timeH, edx;
  }
  time = timeH; time <<= 32;
  time += timeL;
}

static void warmUpTimeRDTSC()
{
  int starttime, totaltime;
  __asm
  {
    cpuid
    rdtsc
    mov starttime, eax
    cpuid
    rdtsc
    sub eax, starttime
    mov starttime, eax
    cpuid
    rdtsc
    mov starttime, eax
    cpuid
    rdtsc
    sub eax, starttime
    mov starttime, eax
    cpuid
    rdtsc
    mov starttime, eax
    cpuid
    rdtsc
    sub eax, starttime
    mov totaltime, eax
  }
}
#endif