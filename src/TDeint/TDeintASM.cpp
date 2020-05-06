/*
**                TDeinterlace v1.2 for Avisynth 2.6 interface
**
**   TDeinterlace is a bi-directionally motion adaptive deinterlacer.
**   It also uses a couple modified forms of ela interpolation which
**   help to reduce "jaggy" edges in places where interpolation must
**   be used. TDeinterlace currently supports YV12 and YUY2 colorspaces.
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

#include "..\TDeint\TDeinterlace.h"
#include "TCommonASM.h"
#include "TDeintASM.h"
#include "emmintrin.h"

void TDeinterlace::absDiff(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, int pos, IScriptEnvironment *env)
{
  long cpu = env->GetCPUFlags();
  if (opt == 0) cpu=0;

  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsYUY2() ? 1 : 3;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const unsigned char *srcp1 = src1->GetReadPtr(plane);
    const int src1_pitch = src1->GetPitch(plane);
    const int height = src1->GetHeight(plane);
    const int width = src1->GetRowSize(plane);
    const unsigned char *srcp2 = src2->GetReadPtr(plane);
    const int src2_pitch = src2->GetPitch(plane);
    // pos: planarTools plane index
    unsigned char *dstp = pos == -1 ? dst->GetWritePtr(plane) : db->GetWritePtr(pos, b);
    const int dst_pitch = pos == -1 ? dst->GetPitch(plane) : db->GetPitch(b);
    const int mthresh1 = b == 0 ? mthreshL : mthreshC;
    const int mthresh2 = vi.IsYUY2() ? mthreshC : (b == 0 ? mthreshL : mthreshC);
    if (cpu&CPUF_SSE2)
      absDiff_SSE2(srcp1, srcp2, dstp, src1_pitch, src2_pitch, dst_pitch, width, height, mthresh1, mthresh2);
    else
      absDiff_c(srcp1, srcp2, dstp, src1_pitch, src2_pitch, dst_pitch, width, height, mthresh1, mthresh2);
  }
}

void TDeinterlace::buildDiffMapPlane2(const unsigned char* prvp, const unsigned char* nxtp,
  unsigned char* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int optt, IScriptEnvironment* env)
{
  long cpu = env->GetCPUFlags();
  if (optt == 0) cpu = 0;

  const bool YUY2_LumaOnly = false; // no mChroma like in TFM
  do_buildABSDiffMask2(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch, Width, Height, YUY2_LumaOnly, cpu);

}



void TDeinterlace::buildABSDiffMask(const unsigned char *prvp, const unsigned char *nxtp,
  int prv_pitch, int nxt_pitch, int tpitch, int width, int height, IScriptEnvironment *env)
{
  long cpu = env->GetCPUFlags();
  if (opt == 0) cpu = 0;

  do_buildABSDiffMask(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, width, height, false, cpu);

}


void TDeinterlace::buildDiffMapPlane_Planar(const unsigned char *prvp, const unsigned char *nxtp,
  unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int tpitch, IScriptEnvironment *env)
{
  buildABSDiffMask(prvp - prv_pitch, nxtp - nxt_pitch, prv_pitch, nxt_pitch, tpitch, Width, Height >> 1, env);
  AnalyzeDiffMask_Planar(dstp, dst_pitch, tbuffer, tpitch, Width, Height);
}


void TDeinterlace::buildDiffMapPlaneYUY2(const unsigned char *prvp, const unsigned char *nxtp,
  unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int tpitch, IScriptEnvironment *env)
{
  long cpu = env->GetCPUFlags();
  if (opt == 0) cpu = 0;

  // Original v1.1: YUY2 was with chroma, but planar with no chroma.
  // v1.2: make it consistent with planar case.
  const bool YUY2_LumaOnly = true; // fix value, unlike in TFM where we have an mChroma parameter
  const bool mChroma = !YUY2_LumaOnly; // in TFM it's a real parameter

  do_buildABSDiffMask(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, Width, Height >> 1, YUY2_LumaOnly, cpu);
  AnalyzeDiffMask_YUY2(dstp, dst_pitch, tbuffer, tpitch, Width, Height, mChroma);

}

