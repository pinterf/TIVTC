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

#include "Cycle.h"
#include "avisynth.h"
#include "stdint.h"
#include <inttypes.h>
#ifdef _WIN32
#include <windows.h> // OutputDebugString
#endif

#include <algorithm>
#include <cstring>
#include "internal.h"

void Cycle::setFrame(int frameIn)
{
  if (frame == frameIn) return;
  clearAll();
  frame = frameIn;
  frameE = frame + length;
  cycleS = frame > 0 ? 0 : 0 - frame;
  if (cycleS > length) cycleS = length;
  cycleE = frameE <= maxFrame ? length : length - (frameE - maxFrame - 1);
  if (cycleE < 0) cycleE = 0;
  offE = length - cycleE;
  frameSO = frame + cycleS;
  frameEO = frame + cycleE;
}

void Cycle::setDecimateLow(int num, IScriptEnvironment *env)
{
  if (decSet) return;
  if (!lowSet || !mSet)
  {
    for (int x = 0; x < length; ++x) decimate[x] = decimate2[x] = -20;
    return;
  }
  for (int i = 0; i < cycleS; ++i) decimate[i] = decimate2[i] = -20;
  int ovrDec = 0;
  for (int i = cycleS; i < cycleE; ++i)
  {
    if (decimate[i] != 1) decimate[i] = decimate2[i] = 0;
    else ++ovrDec;
  }
  for (int i = std::max(cycleE, 0); i < length; ++i) decimate[i] = decimate2[i] = -20;
  const int istop = cycleE - cycleS;
  int asd = abs(sdlim);
  if (sdlim < 0)
  {
    memcpy(dect, decimate, cycleSize * sizeof(int));
    memcpy(dect2, decimate2, cycleSize * sizeof(int));
  }
  int v = 0;
mrestart:
  for (int i = 0; v < num - ovrDec && i < istop; ++i)
  {
    bool update = true;
    for (int c = std::max(cycleS, lowest[i] - asd); c <= std::min(cycleE - 1, lowest[i] + asd); ++c)
    {
      if (decimate[c] == 1)
      {
        update = false;
        break;
      }
    }
    if (update)
    {
      decimate[lowest[i]] = 1;
      int u = lowest[i];
      while (decimate2[u] == 1) ++u;
      decimate2[u] = 1;
      ++v;
    }
  }
  if (v != num - ovrDec)
  {
    int remain = 0;
    for (int i = 0; i < istop; ++i)
    {
      if (decimate[lowest[i]] != 1)
        ++remain;
    }
    if (remain > 0 && asd > 0)
    {
      if (sdlim < 0)
      {
        --asd;
        memcpy(decimate, dect, cycleSize * sizeof(int));
        memcpy(decimate2, dect2, cycleSize * sizeof(int));
        v = 0;
      }
      else asd = 0;
      goto mrestart;
    }
    env->ThrowError("TIVTC-Cycle:  unable to mark the required number of frames " \
      "for decimation (1).");
  }
  decSet = true;
}

void Cycle::setDecimateLowP(int num, IScriptEnvironment *env)
{
  if (!lowSet || !mSet || !decSet)
  {
    for (int x = 0; x < length; ++x) decimate[x] = decimate2[x] = -20;
    return;
  }
  const int istop = cycleE - cycleS;
  int asd = abs(sdlim);
  if (sdlim < 0)
  {
    memcpy(dect, decimate, cycleSize * sizeof(int));
    memcpy(dect2, decimate2, cycleSize * sizeof(int));
  }
  int v = 0;
mrestart:
  for (int i = 0; v < num && i < istop; ++i)
  {
    bool update = true;
    for (int c = std::max(cycleS, lowest[i] - asd); c <= std::min(cycleE - 1, lowest[i] + asd); ++c)
    {
      if (decimate[c] == 1)
      {
        update = false;
        break;
      }
    }
    if (update)
    {
      decimate[lowest[i]] = 1;
      int u = lowest[i];
      while (decimate2[u] == 1) ++u;
      decimate2[u] = 1;
      ++v;
    }
  }
  if (v != num)
  {
    int remain = 0;
    for (int i = 0; i < istop; ++i)
    {
      if (decimate[lowest[i]] != 1)
        ++remain;
    }
    if (remain > 0 && asd > 0)
    {
      if (sdlim < 0)
      {
        --asd;
        memcpy(decimate, dect, cycleSize * sizeof(int));
        memcpy(decimate2, dect2, cycleSize * sizeof(int));
        v = 0;
      }
      else asd = 0;
      goto mrestart;
    }
    env->ThrowError("TIVTC-Cycle:  unable to mark the required number of frames " \
      "for decimation (2).");
  }
}

void Cycle::setLowest(bool excludeD)
{
  if (lowSet) return;
  if (!mSet)
  {
    for (int x = 0; x < length; ++x) lowest[x] = -20;
    return;
  }
  int i, j, temp2, f = cycleS;
  uint64_t temp1;
  if (frame == 0) ++f;
  for (i = 0; i < length; ++i) lowest[i] = i;
  for (i = 0; i < length; ++i) tArray[i] = diffMetricsU[i];
  for (i = 0; i < f; ++i) tArray[i] = UINT64_MAX;
  for (i = std::max(cycleE, 0); i < length; ++i) tArray[i] = UINT64_MAX;
  if ((excludeD && decSet) || !decSet)
  {
    for (i = cycleS; i < cycleE; ++i)
    {
      if (decimate[i] == 1) tArray[i] = UINT64_MAX;
    }
  }
  for (i = 1; i < length; ++i)
  {
    j = i;
    temp1 = tArray[j];
    temp2 = lowest[j];
    while (j > 0 && tArray[j - 1] > temp1)
    {
      tArray[j] = tArray[j - 1];
      lowest[j] = lowest[j - 1];
      --j;
    }
    tArray[j] = temp1;
    lowest[j] = temp2;
  }
  lowSet = true;
}

void Cycle::setDups(double thresh)
{
  if (dupsSet) return;
  if (!mSet)
  {
    for (int x = 0; x < length; ++x) dupArray[x] = -20;
    dupCount = -20;
    return;
  }
  int i;
  dupCount = 0;
  for (i = 0; i < cycleS; ++i) dupArray[i] = -20;
  for (i = cycleS; i < cycleE; ++i)
  {
    if (diffMetricsN[i] <= thresh)
    {
      dupArray[i] = 1;
      ++dupCount;
    }
    else dupArray[i] = 0;
  }
  for (i = std::max(cycleE, 0); i < length; ++i) dupArray[i] = -20;
  if (frame == 0 && dupArray[cycleS] == 1)
  {
    --dupCount;
    dupArray[cycleS] = 0;
  }
  dupsSet = true;
}

void Cycle::setDupsMatches(Cycle &p, const std::vector<uint8_t> &marray)
{
  if (dupsSet) return;
  bool skip = false;
  int i, mp, mc;
  for (i = cycleS; i < cycleE; ++i)
  {
    if (match[i] < 0 || match[i] > 6) skip = true;
  }
  if (skip)
  {
    for (i = 0; i < length; ++i) dupArray[i] = -20;
    dupCount = -20;
    return;
  }
  dupCount = 0;
  for (i = 0; i < cycleS; ++i) dupArray[i] = -20;
  mp = (p.cycleE > 0 && p.frame != frame) ? p.match[p.cycleE - 1] : -20;
  if (mp == -20 && marray.size())
  {
    int n = (p.frame == frame) ? frameSO - 1 : p.cycleE - 1;
    if (!(n < 0 || n > maxFrame || (n >= frameSO && n < frameEO) || n != frameSO - 1))
    {
      int value = marray[n];
      if ((value&ISMATCH) != 0x70)
      {
        value = (value&ISMATCH) >> 4;
        if (value == ISC) mp = ISC;
        else if (value == ISP) mp = ISP;
        else if (value == ISN) mp = ISN;
        else if (value == ISB) mp = ISB;
        else if (value == ISU) mp = ISU;
        else if (value == ISDB) mp = ISDB;
        else if (value == ISDT) mp = ISDT;
      }
    }
  }
  mc = match[cycleS];
  for (i = cycleS; i < cycleE; ++i)
  {
    if (checkMatchDup(mp, mc))
    {
      dupArray[i] = 1;
      ++dupCount;
    }
    else dupArray[i] = 0;
    mp = mc;
    if (i < cycleE - 1) mc = match[i + 1];
  }
  for (i = std::max(cycleE, 0); i < length; ++i) dupArray[i] = -20;
  dupsSet = true;
}

void Cycle::setIsFilmD2V()
{
  isfilmd2v = false;
  for (int i = cycleS; i < cycleE; ++i)
  {
    if (filmd2v[i] == 1)
    {
      isfilmd2v = true;
      return;
    }
  }
}

bool Cycle::checkMatchDup(int mp, int mc)
{
  if (mp == 0 && mc == 3) return true;
  else if (mp == 1 && (mc == 0 || mc == 3)) return true;
  else if (mp == 2 && (mc == 1 || mc == 3 || mc == 4 || mc == 6)) return true;
  else if (mp == 3 && mc == 0) return true;
  else if (mp == 4 && (mc == 0 || mc == 1 || mc == 2 || mc == 5)) return true;
  else if (mp == 5 && mc == 3) return true;
  else if (mp == 6 && mc == 0) return true;
  else if (mc < 0) return true;
  return false;
}

int Cycle::getNonDec(int n)
{
  if (!decSet) return -1;
  int i, count, ret;
  for (count = -1, ret = -1, i = cycleS; i < cycleE; ++i)
  {
    if (decimate[i] == 0) ++count;
    if (count == n) { ret = i; break; }
  }
  return ret;
}

void Cycle::clearAll()
{
  mSet = lowSet = dupsSet = decSet = isfilmd2v = false;
  frame = frameE = cycleS = cycleE = offE = -20;
  frameSO = frameEO = dupCount = blend = -20;
  type = -1;
  for (int x = 0; x < length; ++x)
  {
    dupArray[x] = lowest[x] = decimate[x] = match[x] = decimate2[x] = filmd2v[x] = -20;
    diffMetricsU[x] = diffMetricsUF[x] = UINT64_MAX;
    diffMetricsN[x] = -20.0;
  }
}

int Cycle::sceneDetect(uint64_t thresh)
{
  if (!mSet) return -20;
  int i, f, v;
  for (f = 0, v = -1, i = cycleS; i < cycleE; ++i)
  {
    if (diffMetricsUF[i] > thresh)
    {
      ++f;
      v = i;
    }
  }
  if (f == 1)
  {
    if (v > 0) return v - 1;
    else return v;
  }
  return -20;
}

int Cycle::sceneDetect(Cycle &prev, Cycle &next, uint64_t thresh)
{
  if (!mSet || !prev.mSet || !next.mSet) return -20;
  int i, f, v;
  if (length > 10) return sceneDetect(thresh);
  for (v = prev.cycleS; v < prev.cycleE; ++v)
  {
    if (prev.diffMetricsUF[v] > thresh) return -20;
  }
  for (v = next.cycleS; v < next.cycleE; ++v)
  {
    if (next.diffMetricsUF[v] > thresh) return -20;
  }
  for (f = 0, v = -1, i = cycleS; i < cycleE; ++i)
  {
    if (diffMetricsUF[i] > thresh)
    {
      ++f;
      v = i;
    }
  }
  if (f == 1)
  {
    if (v > 0) return v - 1;
    else return v;
  }
  return -20;
}

void Cycle::debugOutput()
{
  char temp[256];
  sprintf(temp, "Cycle:  length = %d  maxFrame = %d  size = %d\n", length, maxFrame, cycleSize);
  OutputDebugString(temp);
  sprintf(temp, "Cycle:  frame = %d   frameE = %d\n", frame, frameE);
  OutputDebugString(temp);
  sprintf(temp, "Cycle:  cycleS = %d  cycleE = %d\n", cycleS, cycleE);
  OutputDebugString(temp);
  sprintf(temp, "Cycle:  frameSO = %d frameEO = %d\n", frameSO, frameEO);
  OutputDebugString(temp);
  sprintf(temp, "Cycle:  offE = %d    type = %d  blend = %d  dupCount = %d\n", offE, type, blend, dupCount);
  OutputDebugString(temp);
  sprintf(temp, "Cycle:  dupSet = %c  mSet = %c  lowSet = %c  decSet = %c  isfilmd2v = %c\n",
    dupsSet ? 'T' : 'F', mSet ? 'T' : 'F', lowSet ? 'T' : 'F', decSet ? 'T' : 'F',
    isfilmd2v ? 'T' : 'F');
  OutputDebugString(temp);
}

void Cycle::debugMetrics(int _length)
{
  char temp[256];
  for (int x = 0; x < _length; ++x)
  {
    sprintf(temp, "Cycle:  %d - %3.2f  %" PRIu64 "  %" PRIu64 "\n", x, diffMetricsN[x], diffMetricsU[x],
      diffMetricsUF[x]);
    OutputDebugString(temp);
    sprintf(temp, "Cycle:  %d - dup = %d  lowest = %d  decimate = %d  decimate2 = %d  match = %d  filmd2v = %d\n", x,
      dupArray[x], lowest[x], decimate[x], decimate2[x], match[x], filmd2v[x]);
    OutputDebugString(temp);
  }
}

Cycle::Cycle(int _size, int _sdlim)
{
  mSet = lowSet = dupsSet = decSet = isfilmd2v = false;
  length = frame = frameE = cycleS = cycleE = offE = -20;
  frameSO = frameEO = maxFrame = dupCount = blend = -20;
  type = -1;
  dupArray = lowest = match = decimate = decimate2 = filmd2v = nullptr;
  dect = dect2 = nullptr;
  diffMetricsU = diffMetricsUF = tArray = nullptr;
  diffMetricsN = nullptr;
  cycleSize = std::max(0, _size);
  sdlim = _sdlim;
  allocSpace();
  for (int x = 0; x < cycleSize; ++x)
  {
    dupArray[x] = lowest[x] = decimate[x] = match[x] = decimate2[x] = filmd2v[x] = -20;
    diffMetricsU[x] = diffMetricsUF[x] = tArray[x] = UINT64_MAX;
    diffMetricsN[x] = -20.0;
  }
}

Cycle::~Cycle()
{
  if (dupArray != nullptr) { free(dupArray); dupArray = nullptr; }
  if (lowest != nullptr) { free(lowest); lowest = nullptr; }
  if (match != nullptr) { free(match); match = nullptr; }
  if (filmd2v != nullptr) { free(filmd2v); filmd2v = nullptr; }
  if (decimate != nullptr) { free(decimate); decimate = nullptr; }
  if (decimate2 != nullptr) { free(decimate2); decimate2 = nullptr; }
  if (dect != nullptr) { free(dect); dect = nullptr; }
  if (dect2 != nullptr) { free(dect2); dect2 = nullptr; }
  if (diffMetricsU != nullptr) { free(diffMetricsU); diffMetricsU = nullptr; }
  if (diffMetricsUF != nullptr) { free(diffMetricsUF); diffMetricsUF = nullptr; }
  if (tArray != nullptr) { free(tArray); tArray = nullptr; }
  if (diffMetricsN != nullptr) { free(diffMetricsN); diffMetricsN = nullptr; }
}

bool Cycle::allocSpace()
{
  if (dupArray != nullptr) { free(dupArray); dupArray = nullptr; }
  if (lowest != nullptr) { free(lowest); lowest = nullptr; }
  if (match != nullptr) { free(match); match = nullptr; }
  if (filmd2v != nullptr) { free(filmd2v); filmd2v = nullptr; }
  if (decimate != nullptr) { free(decimate); decimate = nullptr; }
  if (decimate2 != nullptr) { free(decimate2); decimate2 = nullptr; }
  if (dect != nullptr) { free(dect); dect = nullptr; }
  if (dect2 != nullptr) { free(dect2); dect2 = nullptr; }
  if (diffMetricsU != nullptr) { free(diffMetricsU); diffMetricsU = nullptr; }
  if (diffMetricsUF != nullptr) { free(diffMetricsUF); diffMetricsUF = nullptr; }
  if (tArray != nullptr) { free(tArray); tArray = nullptr; }
  if (diffMetricsN != nullptr) { free(diffMetricsN); diffMetricsN = nullptr; }
  dupArray = (int *)malloc(cycleSize * sizeof(int));
  lowest = (int *)malloc(cycleSize * sizeof(int));
  match = (int *)malloc(cycleSize * sizeof(int));
  filmd2v = (int *)malloc(cycleSize * sizeof(int));
  decimate = (int *)malloc(cycleSize * sizeof(int));
  decimate2 = (int *)malloc(cycleSize * sizeof(int));
  dect = (int *)malloc(cycleSize * sizeof(int));
  dect2 = (int *)malloc(cycleSize * sizeof(int));
  diffMetricsU = (uint64_t *)malloc(cycleSize * sizeof(uint64_t));
  diffMetricsUF = (uint64_t *)malloc(cycleSize * sizeof(uint64_t));
  tArray = (uint64_t *)malloc(cycleSize * sizeof(uint64_t));
  diffMetricsN = (double *)malloc(cycleSize * sizeof(double));
  if (dupArray == nullptr || lowest == nullptr || match == nullptr || filmd2v == nullptr ||
    decimate == nullptr || decimate2 == nullptr || diffMetricsU == nullptr ||
    diffMetricsUF == nullptr || diffMetricsN == nullptr || tArray == nullptr ||
    dect == nullptr || dect2 == nullptr) return false;
  return true;
}

void Cycle::setSize(int _size)
{
  cycleSize = std::max(0, _size);
  allocSpace();
  for (int x = 0; x < cycleSize; ++x)
  {
    dupArray[x] = lowest[x] = decimate[x] = match[x] = decimate2[x] = filmd2v[x] = -20;
    diffMetricsU[x] = diffMetricsUF[x] = tArray[x] = UINT64_MAX;
    diffMetricsN[x] = -20.0;
  }
}

Cycle& Cycle::operator=(Cycle& ob2)
{
  length = ob2.length;
  maxFrame = ob2.maxFrame;
  frame = ob2.frame;
  frameE = ob2.frameE;
  offE = ob2.offE;
  cycleS = ob2.cycleS;
  cycleE = ob2.cycleE;
  frameSO = ob2.frameSO;
  frameEO = ob2.frameEO;
  type = ob2.type;
  dupsSet = ob2.dupsSet;
  mSet = ob2.mSet;
  lowSet = ob2.lowSet;
  decSet = ob2.decSet;
  dupCount = ob2.dupCount;
  blend = ob2.blend;
  isfilmd2v = ob2.isfilmd2v;
  cycleSize = std::min(cycleSize, ob2.cycleSize);
  if (length > cycleSize) length = cycleSize;
  memcpy(dupArray, ob2.dupArray, cycleSize * sizeof(int));
  memcpy(lowest, ob2.lowest, cycleSize * sizeof(int));
  memcpy(match, ob2.match, cycleSize * sizeof(int));
  memcpy(filmd2v, ob2.filmd2v, cycleSize * sizeof(int));
  memcpy(decimate, ob2.decimate, cycleSize * sizeof(int));
  memcpy(decimate2, ob2.decimate2, cycleSize * sizeof(int));
  memcpy(diffMetricsU, ob2.diffMetricsU, cycleSize * sizeof(uint64_t));
  memcpy(diffMetricsUF, ob2.diffMetricsUF, cycleSize * sizeof(uint64_t));
  memcpy(diffMetricsN, ob2.diffMetricsN, cycleSize * sizeof(double));
  return *this;
}
