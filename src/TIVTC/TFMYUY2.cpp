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


bool TFM::checkCombedYUY2(PVideoFrame &src, int n, int match,
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

  const bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

  const uint8_t *srcp = src->GetReadPtr();
  const int src_pitch = src->GetPitch();
  const int Width = src->GetRowSize();
  const int Height = src->GetHeight();
  const uint8_t *srcpp = srcp - src_pitch;
  const uint8_t *srcppp = srcpp - src_pitch;
  const uint8_t *srcpn = srcp + src_pitch;
  const uint8_t *srcpnn = srcpn + src_pitch;
  uint8_t *cmkw = cmask->GetPtr();
  const int cmk_pitch = cmask->GetPitch();
  const int inc = chroma ? 1 : 2;
  const int xblocks = ((Width + xhalf) >> xshift) + 1;
  const int xblocks4 = xblocks << 2;
  xblocksi = xblocks4;
  const int yblocks = ((Height + yhalf) >> yshift) + 1;
  const int arraysize = (xblocks*yblocks) << 2;
  if (cthresh < 0) { 
    memset(cmkw, 255, Height*cmk_pitch); 
    goto cjump;
  }
  memset(cmkw, 0, Height*cmk_pitch);
  if (metric == 0)
  {
    const int cthresh6 = cthresh * 6;

    for (int x = 0; x < Width; x += inc)
    {
      const int sFirst = srcp[x] - srcpn[x];
      if (sFirst > cthresh || sFirst < -cthresh)
      {
        if (abs(srcpnn[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpn[x] + srcpn[x]))) > cthresh6)
          cmkw[x] = 0xFF;
      }
    }
    srcppp += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    srcpnn += src_pitch;
    cmkw += cmk_pitch;
    for (int x = 0; x < Width; x += inc)
    {
      const int sFirst = srcp[x] - srcpp[x];
      const int sSecond = srcp[x] - srcpn[x];
      if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
      {
        if (abs(srcpnn[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpp[x] + srcpn[x]))) > cthresh6)
          cmkw[x] = 0xFF;
      }
    }
    srcppp += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    srcpnn += src_pitch;
    cmkw += cmk_pitch;
    if (use_sse2)
    {
      if (chroma)
      {
        check_combing_SSE2(srcp, cmkw, Width, Height - 4, src_pitch, cmk_pitch, cthresh);
        srcppp += src_pitch * (Height - 4);
        srcpp += src_pitch * (Height - 4);
        srcp += src_pitch * (Height - 4);
        srcpn += src_pitch * (Height - 4);
        srcpnn += src_pitch * (Height - 4);
        cmkw += cmk_pitch * (Height - 4);
      }
      else
      {
        check_combing_YUY2LumaOnly_SSE2(srcp, cmkw, Width, Height - 4, src_pitch, cmk_pitch, cthresh);
        srcppp += src_pitch * (Height - 4);
        srcpp += src_pitch * (Height - 4);
        srcp += src_pitch * (Height - 4);
        srcpn += src_pitch * (Height - 4);
        srcpnn += src_pitch * (Height - 4);
        cmkw += cmk_pitch * (Height - 4);
      }
    }
    else
    {
      for (int y = 2; y < Height - 2; ++y)
      {
        for (int x = 0; x < Width; x += inc)
        {
          const int sFirst = srcp[x] - srcpp[x];
          const int sSecond = srcp[x] - srcpn[x];
          if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
          {
            if (abs(srcppp[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpp[x] + srcpn[x]))) > cthresh6)
              cmkw[x] = 0xFF;
          }
        }
        srcppp += src_pitch;
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        srcpnn += src_pitch;
        cmkw += cmk_pitch;
      }
    }
    for (int x = 0; x < Width; x += inc)
    {
      const int sFirst = srcp[x] - srcpp[x];
      const int sSecond = srcp[x] - srcpn[x];
      if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
      {
        if (abs(srcppp[x] + (srcp[x] << 2) + srcppp[x] - (3 * (srcpp[x] + srcpn[x]))) > cthresh6)
          cmkw[x] = 0xFF;
      }
    }
    srcppp += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    srcpnn += src_pitch;
    cmkw += cmk_pitch;
    for (int x = 0; x < Width; x += inc)
    {
      const int sFirst = srcp[x] - srcpp[x];
      if (sFirst > cthresh || sFirst < -cthresh)
      {
        if (abs(srcppp[x] + (srcp[x] << 2) + srcppp[x] - (3 * (srcpp[x] + srcpp[x]))) > cthresh6)
          cmkw[x] = 0xFF;
      }
    }
  }
  else
  {
    const int cthreshsq = cthresh*cthresh;

    // top
    for (int x = 0; x < Width; x += inc)
    {
      if ((srcp[x] - srcpn[x])*(srcp[x] - srcpn[x]) > cthreshsq)
        cmkw[x] = 0xFF;
    }
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    cmkw += cmk_pitch;
    // middle section
    if (use_sse2)
    {
      // height-2: no top, no bottom
      // no "inc" here (chroma: inc=1 lumaonly: inc=2)
      // SSE2 is separated instead
      if (chroma)
        check_combing_SSE2_Metric1(srcp, cmkw, Width, Height - 2, src_pitch, cmk_pitch, cthreshsq);
      else
        check_combing_SSE2_Luma_Metric1(srcp, cmkw, Width, Height - 2, src_pitch, cmk_pitch, cthreshsq);
      srcpp += src_pitch * (Height - 2);
      srcp += src_pitch * (Height - 2);
      srcpn += src_pitch * (Height - 2);
      cmkw += cmk_pitch * (Height - 2);
    }
    else
    {
      // C version
      // no top, no bottom
      // chroma: inc=1 lumaonly: inc=2
      for (int y = 1; y < Height - 1; ++y)
      {
        for (int x = 0; x < Width; x += inc)
        {
          if ((srcp[x] - srcpp[x])*(srcp[x] - srcpn[x]) > cthreshsq)
            cmkw[x] = 0xFF;
        }
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        cmkw += cmk_pitch;
      }
    }
    // bottom
    for (int x = 0; x < Width; x += inc)
    {
      if ((srcp[x] - srcpp[x])*(srcp[x] - srcpp[x]) > cthreshsq)
        cmkw[x] = 0xFF;
    }
  }
cjump:
  if (chroma)
  {
    uint8_t *cmkp = cmask->GetPtr() + cmk_pitch;
 
    uint8_t *cmkpp = cmkp - cmk_pitch;
    uint8_t *cmkpn = cmkp + cmk_pitch;
    // middle section
    for (int y = 1; y < Height - 1; ++y)
    {
      // no left, no right
      for (int x = 4; x < Width - 4; x += 4)
      {
        if ((cmkp[x + 1] == 0xFF && (cmkpp[x - 3] == 0xFF || cmkpp[x + 1] == 0xFF || cmkpp[x + 5] == 0xFF ||
          cmkp[x - 3] == 0xFF || cmkp[x + 5] == 0xFF || cmkpn[x - 3] == 0xFF || cmkpn[x + 1] == 0xFF ||
          cmkpn[x + 5] == 0xFF)) || (cmkp[x + 3] == 0xFF && (cmkpp[x - 1] == 0xFF || cmkpp[x + 3] == 0xFF ||
            cmkpp[x + 7] == 0xFF || cmkp[x - 1] == 0xFF || cmkp[x + 7] == 0xFF || cmkpn[x - 1] == 0xFF ||
            cmkpn[x + 3] == 0xFF || cmkpn[x + 7] == 0xFF))) cmkp[x] = cmkp[x + 2] = 0xFF;
      }
      cmkpp += cmk_pitch;
      cmkp += cmk_pitch;
      cmkpn += cmk_pitch;
    }
  }
  const uint8_t *cmkp = cmask->GetPtr() + cmk_pitch;
  const uint8_t *cmkpp = cmkp - cmk_pitch;
  const uint8_t *cmkpn = cmkp + cmk_pitch;
  memset(cArray.get(), 0, arraysize * sizeof(int));
  int Heighta = (Height >> (yshift - 1)) << (yshift - 1);
  if (Heighta == Height) Heighta = Height - yhalf;
  const int Widtha = (Width >> (xshift - 1)) << (xshift - 1); // whole blocks
  const bool use_sse2_sum = (use_sse2 && xhalf == 16 && yhalf == 8) ? true : false;
  for (int y = 1; y < yhalf; ++y)
  {
    const int temp1 = (y >> yshift)*xblocks4;
    const int temp2 = ((y + yhalf) >> yshift)*xblocks4;
    for (int x = 0; x < Width; x += 2)
    {
      if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF)
      {
        const int box1 = (x >> xshift) << 2;
        const int box2 = ((x + xhalf) >> xshift) << 2;
        ++cArray.get()[temp1 + box1 + 0];
        ++cArray.get()[temp1 + box2 + 1];
        ++cArray.get()[temp2 + box1 + 2];
        ++cArray.get()[temp2 + box2 + 3];
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
      // aligned
      for (int x = 0; x < Widtha; x += xhalf)
      {
        int sum = 0;
        compute_sum_16x8_sse2_luma(cmkpp + x, cmk_pitch, sum);
        if (sum)
        {
          const int box1 = (x >> xshift) << 2;
          const int box2 = ((x + xhalf) >> xshift) << 2;
          cArray.get()[temp1 + box1 + 0] += sum;
          cArray.get()[temp1 + box2 + 1] += sum;
          cArray.get()[temp2 + box1 + 2] += sum;
          cArray.get()[temp2 + box2 + 3] += sum;
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
          for (int v = 0; v < xhalf; v += 2)
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
          cArray.get()[temp1 + box1 + 0] += sum;
          cArray.get()[temp1 + box2 + 1] += sum;
          cArray.get()[temp2 + box1 + 2] += sum;
          cArray.get()[temp2 + box2 + 3] += sum;
        }
      }
    }
    // rest, after the aligned (whole-block) part
    for (int x = Widtha; x < Width; x += 2)
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
        cArray.get()[temp1 + box1 + 0] += sum;
        cArray.get()[temp1 + box2 + 1] += sum;
        cArray.get()[temp2 + box1 + 2] += sum;
        cArray.get()[temp2 + box2 + 3] += sum;
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
    for (int x = 0; x < Width; x += 2)
    {
      if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF)
      {
        const int box1 = (x >> xshift) << 2;
        const int box2 = ((x + xhalf) >> xshift) << 2;
        ++cArray.get()[temp1 + box1 + 0];
        ++cArray.get()[temp1 + box2 + 1];
        ++cArray.get()[temp2 + box1 + 2];
        ++cArray.get()[temp2 + box2 + 3];
      }
    }
    cmkpp += cmk_pitch;
    cmkp += cmk_pitch;
    cmkpn += cmk_pitch;
  }
  for (int x = 0; x < arraysize; ++x)
  {
    if (cArray.get()[x] > mics[match])
    {
      mics[match] = cArray.get()[x];
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

void TFM::buildDiffMapPlaneYUY2(const uint8_t *prvp, const uint8_t *nxtp,
  uint8_t *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int tpitch)
{
  buildABSDiffMask<uint8_t>(prvp - prv_pitch, nxtp - nxt_pitch, prv_pitch, nxt_pitch, tpitch, Width, Height >> 1);
  AnalyzeDiffMask_YUY2(dstp, dst_pitch, tbuffer.get(), tpitch, Width, Height, mChroma);
}

