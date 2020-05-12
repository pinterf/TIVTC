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

#include "TDeinterlace.h"
#include "TCommonASM.h"
#include "TDeintASM.h"
#include "emmintrin.h"

// HBD ready inside
void TDeinterlace::absDiff(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, int pos, IScriptEnvironment *env)
{
  const bool use_sse2 = cpuFlags & CPUF_SSE2;

  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int pixelsize = vi.ComponentSize();
  const int bits_per_pixel = vi.BitsPerComponent();
  // dst or db is always 8 bit buffer
  // fixme: dst is never used in the project, remove it?
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const uint8_t *srcp1 = src1->GetReadPtr(plane);
    const int src1_pitch = src1->GetPitch(plane);
    const int height = src1->GetHeight(plane);
    const int rowsize = src1->GetRowSize(plane);
    const int width = rowsize / pixelsize;
    const uint8_t *srcp2 = src2->GetReadPtr(plane);
    const int src2_pitch = src2->GetPitch(plane);
    // pos: planarTools plane index
    uint8_t *dstp = pos == -1 ? dst->GetWritePtr(plane) : db->GetWritePtr(pos, b);
    const int dst_pitch = pos == -1 ? dst->GetPitch(plane) : db->GetPitch(b);

    if (vi.IsYUY2()) {
      // YUY2 special: two thresholds for interleaved Luma-chroma check
      if (use_sse2)
        absDiff_SSE2(srcp1, srcp2, dstp, src1_pitch, src2_pitch, dst_pitch, width, height, mthreshL, mthreshC);
      else
        absDiff_c(srcp1, srcp2, dstp, src1_pitch, src2_pitch, dst_pitch, width, height, mthreshL, mthreshC);
    }
    else {
      const int mthresh = (b == 0 ? mthreshL : mthreshC) << (bits_per_pixel - 8);

      if (pixelsize == 1) {
        // the two threshold parameters the same
        if (use_sse2)
          absDiff_SSE2(srcp1, srcp2, dstp, src1_pitch, src2_pitch, dst_pitch, width, height, mthresh, mthresh);
        else
          absDiff_c(srcp1, srcp2, dstp, src1_pitch, src2_pitch, dst_pitch, width, height, mthresh, mthresh);
      }
      else if (pixelsize == 2) {
        absDiff_uint16_c(srcp1, srcp2, dstp, src1_pitch, src2_pitch, dst_pitch, width, height, mthresh);
      }
    }
  }
}



template<typename pixel_t>
void TDeinterlace::buildABSDiffMask(const uint8_t *prvp, const uint8_t *nxtp,
  int prv_pitch, int nxt_pitch, int tpitch, int width, int height, IScriptEnvironment *env)
{
  do_buildABSDiffMask<pixel_t>(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, width, height, false, cpuFlags);
}

template<typename pixel_t>
void TDeinterlace::buildDiffMapPlane_Planar(const uint8_t *prvp, const uint8_t *nxtp,
  uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int tpitch, int bits_per_pixel, IScriptEnvironment *env)
{
  buildABSDiffMask<pixel_t>(prvp - prv_pitch, nxtp - nxt_pitch, prv_pitch, nxt_pitch, tpitch, Width, Height >> 1, env);
  AnalyzeDiffMask_Planar<pixel_t>(dstp, dst_pitch, tbuffer, tpitch, Width, Height, bits_per_pixel);
}
// instantiate
template void TDeinterlace::buildDiffMapPlane_Planar<uint8_t>(const uint8_t* prvp, const uint8_t* nxtp,
  uint8_t* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int tpitch, int bits_per_pixel, IScriptEnvironment* env);
template void TDeinterlace::buildDiffMapPlane_Planar<uint16_t>(const uint8_t* prvp, const uint8_t* nxtp,
  uint8_t* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int tpitch, int bits_per_pixel, IScriptEnvironment* env);


void TDeinterlace::buildDiffMapPlaneYUY2(const uint8_t *prvp, const uint8_t *nxtp,
  uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int tpitch, IScriptEnvironment *env)
{
  // Original v1.1: YUY2 was with chroma, but planar with no chroma.
  // v1.2: make it consistent with planar case.
  const bool YUY2_LumaOnly = true; // fix value, unlike in TFM where we have an mChroma parameter
  const bool mChroma = !YUY2_LumaOnly; // in TFM it's a real parameter

  do_buildABSDiffMask<uint8_t>(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, Width, Height >> 1, YUY2_LumaOnly, cpuFlags);
  AnalyzeDiffMask_YUY2(dstp, dst_pitch, tbuffer, tpitch, Width, Height, mChroma);

}

