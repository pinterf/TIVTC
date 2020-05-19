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

#include "TFM.h"
#include "TFMasm.h"
#include "TCommonASM.h"
#include <algorithm>

// used by isCombedTIVTC as well
// mask only, no hbd needed
template<int planarType>
void FillCombedPlanarUpdateCmaskByUV(PlanarFrame* cmask)
{
  uint8_t* cmkp = cmask->GetPtr(0);
  uint8_t* cmkpU = cmask->GetPtr(1);
  uint8_t* cmkpV = cmask->GetPtr(2);
  const int Width = cmask->GetWidth(2);
  const int Height = cmask->GetHeight(2);
  const int cmk_pitch = cmask->GetPitch(0);
  const int cmk_pitchUV = cmask->GetPitch(2);

  // 420 only
  uint8_t* cmkpn = cmkp + cmk_pitch;
  uint8_t* cmkpp = cmkp - cmk_pitch;
  uint8_t* cmkpnn = cmkpn + cmk_pitch;

  uint8_t* cmkppU = cmkpU - cmk_pitchUV;
  uint8_t* cmkpnU = cmkpU + cmk_pitchUV;

  uint8_t* cmkppV = cmkpV - cmk_pitchUV;
  uint8_t* cmkpnV = cmkpV + cmk_pitchUV;
  for (int y = 1; y < Height - 1; ++y)
  {
    if (planarType == 420) {
      cmkp += cmk_pitch * 2;
      cmkpn += cmk_pitch * 2;
      cmkpp += cmk_pitch * 2;
      cmkpnn += cmk_pitch * 2;
    }
    else {
      cmkp += cmk_pitch;
    }
    cmkppV += cmk_pitchUV;
    cmkpV += cmk_pitchUV;
    cmkpnV += cmk_pitchUV;
    cmkppU += cmk_pitchUV;
    cmkpU += cmk_pitchUV;
    cmkpnU += cmk_pitchUV;
    for (int x = 1; x < Width - 1; ++x)
    {
      if (
        (cmkpV[x] == 0xFF &&
          (cmkpV[x - 1] == 0xFF || cmkpV[x + 1] == 0xFF ||
            cmkppV[x - 1] == 0xFF || cmkppV[x] == 0xFF || cmkppV[x + 1] == 0xFF ||
            cmkpnV[x - 1] == 0xFF || cmkpnV[x] == 0xFF || cmkpnV[x + 1] == 0xFF
            )
          ) ||
        (cmkpU[x] == 0xFF &&
          (cmkpU[x - 1] == 0xFF || cmkpU[x + 1] == 0xFF ||
            cmkppU[x - 1] == 0xFF || cmkppU[x] == 0xFF || cmkppU[x + 1] == 0xFF ||
            cmkpnU[x - 1] == 0xFF || cmkpnU[x] == 0xFF || cmkpnU[x + 1] == 0xFF
            )
          )
        )
      {
        if (planarType == 420) {
          ((uint16_t*)cmkp)[x] = (uint16_t)0xFFFF;
          ((uint16_t*)cmkpn)[x] = (uint16_t)0xFFFF;
          if (y & 1)
            ((uint16_t*)cmkpp)[x] = (uint16_t)0xFFFF;
          else
            ((uint16_t*)cmkpnn)[x] = (uint16_t)0xFFFF;
        }
        else if (planarType == 422) {
          ((uint16_t*)cmkp)[x] = (uint16_t)0xFFFF;
        }
        else if (planarType == 444) {
          cmkp[x] = 0xFF;
        }
        else if (planarType == 411) {
          ((uint32_t*)cmkp)[x] = (uint32_t)0xFFFFFFFF;
        }
      }
    }
  }
}
// templatize
template void FillCombedPlanarUpdateCmaskByUV<411>(PlanarFrame* cmask);
template void FillCombedPlanarUpdateCmaskByUV<420>(PlanarFrame* cmask);
template void FillCombedPlanarUpdateCmaskByUV<422>(PlanarFrame* cmask);
template void FillCombedPlanarUpdateCmaskByUV<444>(PlanarFrame* cmask);


/*
//FIXME: see similar
// template<int planarType>
// void FillCombedPlanarUpdateCmaskByUV(PlanarFrame* cmask);
// very similar core to
template<typename pixel_t>
void ShowCombedTIVTC::fillCombedPlanar(PVideoFrame& src, int& MICount,
  int& b_over, int& c_over, IScriptEnvironment* env)
  and TDeInterlace CheckedCombedPlanar
*/
bool TFM::checkCombedPlanar(PVideoFrame &src, int n, IScriptEnvironment *env, int match,
  int *blockN, int &xblocksi, int *mics, bool ddebug, bool chroma, int cthresh)
{
  if (mics[match] != -20)
  {
    if (mics[match] > MI)
    {
      if (debug && !ddebug)
      {
        sprintf(buf, "TFM:  frame %d  - match %c:  Detected As Combed  (ReCheck - not processed)! (%d > %d)\n",
          n, MTC(match), mics[match], MI);
        OutputDebugString(buf);
      }
      return true;
    }
    if (debug && !ddebug)
    {
      sprintf(buf, "TFM:  frame %d  - match %c:  Detected As NOT Combed  (ReCheck - not processed)! (%d <= %d)\n",
        n, MTC(match), mics[match], MI);
      OutputDebugString(buf);
    }
    return false;
  }

  bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

  const int cthresh6 = cthresh * 6;

  // fix in v15:  U and V index inconsistency between PlanarFrames and PVideoFrame
  const int stop = chroma ? 3 : 1;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const uint8_t *srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int Width = src->GetRowSize(plane);
    const int Height = src->GetHeight(plane);
    const uint8_t *srcpp = srcp - src_pitch;
    const uint8_t *srcppp = srcpp - src_pitch;
    const uint8_t *srcpn = srcp + src_pitch;
    const uint8_t *srcpnn = srcpn + src_pitch;
    uint8_t *cmkp = cmask->GetPtr(b);
    const int cmk_pitch = cmask->GetPitch(b);
    if (cthresh < 0) { 
      memset(cmkp, 255, Height*cmk_pitch); 
      continue; 
    }
    memset(cmkp, 0, Height*cmk_pitch);
    if (metric == 0)
    {
      for (int x = 0; x < Width; ++x)
      {
        const int sFirst = srcp[x] - srcpn[x];
        if (sFirst > cthresh || sFirst < -cthresh)
        {
          if (abs(srcpnn[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpn[x] + srcpn[x]))) > cthresh6)
            cmkp[x] = 0xFF;
        }
      }
      srcppp += src_pitch;
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      srcpnn += src_pitch;
      cmkp += cmk_pitch;
      for (int x = 0; x < Width; ++x)
      {
        const int sFirst = srcp[x] - srcpp[x];
        const int sSecond = srcp[x] - srcpn[x];
        if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
        {
          if (abs(srcpnn[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpp[x] + srcpn[x]))) > cthresh6)
            cmkp[x] = 0xFF;
        }
      }
      srcppp += src_pitch;
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      srcpnn += src_pitch;
      cmkp += cmk_pitch;
      // middle
      if (use_sse2)
      {
        check_combing_SSE2(srcp, cmkp, Width, Height - 4, src_pitch, cmk_pitch, cthresh);
      }
      else
      {
        check_combing_c<uint8_t, false>(srcp, cmkp, Width, Height - 4, src_pitch, cmk_pitch, cthresh);
      }
      srcppp += src_pitch * (Height - 4);
      srcpp += src_pitch * (Height - 4);
      srcp += src_pitch * (Height - 4);
      srcpn += src_pitch * (Height - 4);
      srcpnn += src_pitch * (Height - 4);
      cmkp += cmk_pitch * (Height - 4);

      for (int x = 0; x < Width; ++x)
      {
        const int sFirst = srcp[x] - srcpp[x];
        const int sSecond = srcp[x] - srcpn[x];
        if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
        {
          if (abs(srcppp[x] + (srcp[x] << 2) + srcppp[x] - (3 * (srcpp[x] + srcpn[x]))) > cthresh6)
            cmkp[x] = 0xFF;
        }
      }
      srcppp += src_pitch;
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      srcpnn += src_pitch;
      cmkp += cmk_pitch;
      for (int x = 0; x < Width; ++x)
      {
        const int sFirst = srcp[x] - srcpp[x];
        if (sFirst > cthresh || sFirst < -cthresh)
        {
          if (abs(srcppp[x] + (srcp[x] << 2) + srcppp[x] - (3 * (srcpp[x] + srcpp[x]))) > cthresh6)
            cmkp[x] = 0xFF;
        }
      }
    }
    else
    {
      // metric == 1
      const int cthreshsq = cthresh*cthresh;
      // top
      for (int x = 0; x < Width; ++x)
      {
        if ((srcp[x] - srcpn[x])*(srcp[x] - srcpn[x]) > cthreshsq)
          cmkp[x] = 0xFF;
      }
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      cmkp += cmk_pitch;
      // middle
      if (use_sse2)
      {
        check_combing_SSE2_Metric1(srcp, cmkp, Width, Height - 2, src_pitch, cmk_pitch, cthreshsq);
        srcpp += src_pitch * (Height - 2);
        srcp += src_pitch * (Height - 2);
        srcpn += src_pitch * (Height - 2);
        cmkp += cmk_pitch * (Height - 2);
      }
      else
      {
        for (int y = 1; y < Height - 1; ++y)
        {
          for (int x = 0; x < Width; ++x)
          {
            if ((srcp[x] - srcpp[x])*(srcp[x] - srcpn[x]) > cthreshsq)
              cmkp[x] = 0xFF;
          }
          srcpp += src_pitch;
          srcp += src_pitch;
          srcpn += src_pitch;
          cmkp += cmk_pitch;
        }
      }
      // bottom
      for (int x = 0; x < Width; ++x)
      {
        if ((srcp[x] - srcpp[x])*(srcp[x] - srcpp[x]) > cthreshsq)
          cmkp[x] = 0xFF;
      }
    }
  }

  // next block is for mask, no hbd needed
  if (chroma)
  {
    if (vi.Is420())
      FillCombedPlanarUpdateCmaskByUV<420>(cmask);
    else if (vi.Is422())
      FillCombedPlanarUpdateCmaskByUV<422>(cmask);
    else if (vi.Is444())
      FillCombedPlanarUpdateCmaskByUV<444>(cmask);
    else if (vi.IsYV411())
      FillCombedPlanarUpdateCmaskByUV<411>(cmask);
  }

  const int cmk_pitch = cmask->GetPitch(0);
  const uint8_t *cmkp = cmask->GetPtr(0) + cmk_pitch;
  const uint8_t *cmkpp = cmkp - cmk_pitch;
  const uint8_t *cmkpn = cmkp + cmk_pitch;
  const int Width = cmask->GetWidth(0);
  const int Height = cmask->GetHeight(0);
  const int xblocks = ((Width + xhalf) >> xshift) + 1;
  const int xblocks4 = xblocks << 2;
  xblocksi = xblocks4;
  const int yblocks = ((Height + yhalf) >> yshift) + 1;
  const int arraysize = (xblocks*yblocks) << 2;
  memset(cArray, 0, arraysize * sizeof(int));
  int Heighta = (Height >> (yshift - 1)) << (yshift - 1);
  if (Heighta == Height) Heighta = Height - yhalf;
  const int Widtha = (Width >> (xshift - 1)) << (xshift - 1);
  const bool use_sse2_sum = (use_sse2 && xhalf == 8 && yhalf == 8) ? true : false; // 8x8: no alignment
  for (int y = 1; y < yhalf; ++y)
  {
    const int temp1 = (y >> yshift)*xblocks4;
    const int temp2 = ((y + yhalf) >> yshift)*xblocks4;
    for (int x = 0; x < Width; ++x)
    {
      if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF)
      {
        const int box1 = (x >> xshift) << 2;
        const int box2 = ((x + xhalf) >> xshift) << 2;
        ++cArray[temp1 + box1 + 0];
        ++cArray[temp1 + box2 + 1];
        ++cArray[temp2 + box1 + 2];
        ++cArray[temp2 + box2 + 3];
      }
    }
    cmkpp += cmk_pitch;
    cmkp += cmk_pitch;
    cmkpn += cmk_pitch;
  }
  for (int y = yhalf; y < Heighta; y += yhalf)
  {
    const int temp1 = (y >> yshift)*xblocks4;
    const int temp2 = ((y + yhalf) >> yshift)*xblocks4;
    if (use_sse2_sum)
    {
      for (int x = 0; x < Widtha; x += xhalf)
      {
        int sum = 0;
        compute_sum_8xN_sse2<8>(cmkpp + x, cmk_pitch, sum);
        if (sum)
        {
          const int box1 = (x >> xshift) << 2;
          const int box2 = ((x + xhalf) >> xshift) << 2;
          cArray[temp1 + box1 + 0] += sum;
          cArray[temp1 + box2 + 1] += sum;
          cArray[temp2 + box1 + 2] += sum;
          cArray[temp2 + box2 + 3] += sum;
        }
      }
    }
    else
    {
      for (int x = 0; x < Widtha; x += xhalf)
      {
        const uint8_t *cmkppT = cmkpp;
        const uint8_t *cmkpT = cmkp;
        const uint8_t *cmkpnT = cmkpn;
        int sum = 0;
        for (int u = 0; u < yhalf; ++u)
        {
          for (int v = 0; v < xhalf; ++v)
          {
            if (cmkppT[x + v] == 0xFF && cmkpT[x + v] == 0xFF &&
              cmkpnT[x + v] == 0xFF) ++sum;
          }
          cmkppT += cmk_pitch;
          cmkpT += cmk_pitch;
          cmkpnT += cmk_pitch;
        }
        if (sum)
        {
          const int box1 = (x >> xshift) << 2;
          const int box2 = ((x + xhalf) >> xshift) << 2;
          cArray[temp1 + box1 + 0] += sum;
          cArray[temp1 + box2 + 1] += sum;
          cArray[temp2 + box1 + 2] += sum;
          cArray[temp2 + box2 + 3] += sum;
        }
      }
    }
    // rest
    for (int x = Widtha; x < Width; ++x)
    {
      const uint8_t *cmkppT = cmkpp;
      const uint8_t *cmkpT = cmkp;
      const uint8_t *cmkpnT = cmkpn;
      int sum = 0;
      for (int u = 0; u < yhalf; ++u)
      {
        if (cmkppT[x] == 0xFF && cmkpT[x] == 0xFF &&
          cmkpnT[x] == 0xFF) ++sum;
        cmkppT += cmk_pitch;
        cmkpT += cmk_pitch;
        cmkpnT += cmk_pitch;
      }
      if (sum)
      {
        const int box1 = (x >> xshift) << 2;
        const int box2 = ((x + xhalf) >> xshift) << 2;
        cArray[temp1 + box1 + 0] += sum;
        cArray[temp1 + box2 + 1] += sum;
        cArray[temp2 + box1 + 2] += sum;
        cArray[temp2 + box2 + 3] += sum;
      }
    }
    cmkpp += cmk_pitch*yhalf;
    cmkp += cmk_pitch*yhalf;
    cmkpn += cmk_pitch*yhalf;
  }
  for (int y = Heighta; y < Height - 1; ++y)
  {
    const int temp1 = (y >> yshift)*xblocks4;
    const int temp2 = ((y + yhalf) >> yshift)*xblocks4;
    for (int x = 0; x < Width; ++x)
    {
      if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF)
      {
        const int box1 = (x >> xshift) << 2;
        const int box2 = ((x + xhalf) >> xshift) << 2;
        ++cArray[temp1 + box1 + 0];
        ++cArray[temp1 + box2 + 1];
        ++cArray[temp2 + box1 + 2];
        ++cArray[temp2 + box2 + 3];
      }
    }
    cmkpp += cmk_pitch;
    cmkp += cmk_pitch;
    cmkpn += cmk_pitch;
  }
  for (int x = 0; x < arraysize; ++x)
  {
    if (cArray[x] > mics[match])
    {
      mics[match] = cArray[x];
      blockN[match] = x;
    }
  }
  if (mics[match] > MI)
  {
    if (debug && !ddebug)
    {
      sprintf(buf, "TFM:  frame %d  - match %c:  Detected As Combed! (%d > %d)\n",
        n, MTC(match), mics[match], MI);
      OutputDebugString(buf);
    }
    return true;
  }
  if (debug && !ddebug)
  {
    sprintf(buf, "TFM:  frame %d  - match %c:  Detected As NOT Combed! (%d <= %d)\n",
      n, MTC(match), mics[match], MI);
    OutputDebugString(buf);
  }
  return false;
}

void TFM::buildDiffMapPlane_Planar(const uint8_t *prvp, const uint8_t *nxtp,
  uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int tpitch, IScriptEnvironment *env)
{
  buildABSDiffMask(prvp - prv_pitch, nxtp - nxt_pitch, prv_pitch, nxt_pitch, tpitch, Width, Height >> 1, env);
  AnalyzeDiffMask_Planar<uint8_t>(dstp, dst_pitch, tbuffer, tpitch, Width, Height, 8);
}

void TFM::DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s)
{
  int x, y = y1 * 20, num, tx, ty;
  int pitchY = dst->GetPitch(PLANAR_Y), pitchUV = dst->GetPitch(PLANAR_V);
  uint8_t *dpY, *dpU, *dpV;
  unsigned int width = dst->GetRowSize(PLANAR_Y);
  int height = dst->GetHeight(PLANAR_Y);
  if (y + 20 >= height) return;
  for (int xx = 0; *s; ++s, ++xx)
  {
    x = (x1 + xx) * 10;
    if (x + 10 >= (int)(width)) return;
    num = *s - ' ';
    for (tx = 0; tx < 10; tx++)
    {
      for (ty = 0; ty < 20; ty++)
      {
        dpY = &dst->GetWritePtr(PLANAR_Y)[(x + tx) + (y + ty) * pitchY];
        if (font[num][ty] & (1 << (15 - tx))) *dpY = 255;
        else *dpY = (unsigned char)(*dpY >> 1);
      }
    }
    for (tx = 0; tx < 10; tx++)
    {
      for (ty = 0; ty < 20; ty++)
      {
        dpU = &dst->GetWritePtr(PLANAR_U)[((x + tx) / 2) + ((y + ty) / 2) * pitchUV];
        dpV = &dst->GetWritePtr(PLANAR_V)[((x + tx) / 2) + ((y + ty) / 2) * pitchUV];
        if (font[num][ty] & (1 << (15 - tx)))
        {
          *dpU = 128;
          *dpV = 128;
        }
        else
        {
          *dpU = (unsigned char)((*dpU + 128) >> 1);
          *dpV = (unsigned char)((*dpV + 128) >> 1);
        }
      }
    }
  }
}

void TFM::drawBoxYV12(PVideoFrame &dst, int blockN, int xblocks)
{
  uint8_t *dstp = dst->GetWritePtr(PLANAR_Y);
  int width = dst->GetRowSize(PLANAR_Y);
  int height = dst->GetHeight(PLANAR_Y);
  int pitch = dst->GetPitch(PLANAR_Y);
  int cordy, cordx, x, y, temp, xlim, ylim;
  cordy = blockN / xblocks;
  cordx = blockN - (cordy*xblocks);
  temp = cordx % 4;
  cordx = (cordx >> 2);
  cordy *= blocky;
  cordx *= blockx;
  if (temp == 1) cordx -= (blockx >> 1);
  else if (temp == 2) cordy -= (blocky >> 1);
  else if (temp == 3) { cordx -= (blockx >> 1); cordy -= (blocky >> 1); }
  xlim = cordx + blockx;
  if (xlim > width) xlim = width;
  ylim = cordy + blocky;
  if (ylim > height) ylim = height;
  for (y = std::max(cordy, 0), temp = cordx + blockx - 1; y < ylim; ++y)
  {
    (dstp + y*pitch)[cordx] = (dstp + y*pitch)[cordx] <= 128 ? 255 : 0;
    if (temp < width) (dstp + y*pitch)[temp] = (dstp + y*pitch)[temp] <= 128 ? 255 : 0;
  }
  for (x = std::max(cordx, 0), temp = cordy + blocky - 1; x < xlim; ++x)
  {
    (dstp + cordy*pitch)[x] = (dstp + cordy*pitch)[x] <= 128 ? 255 : 0;
    if (temp < height) (dstp + temp*pitch)[x] = (dstp + temp*pitch)[x] <= 128 ? 255 : 0;
  }
}