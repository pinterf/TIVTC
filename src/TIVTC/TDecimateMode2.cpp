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

#include "TDecimate.h"
#include <algorithm>
#include "info.h"

PVideoFrame TDecimate::GetFrameMode2(int n, IScriptEnvironment *env, const VideoInfo& vi)
{
  int ret = -20;
  if (mode2_numCycles >= 0)
  {
    int cycleF = -20;
    for (int x = 0; x < mode2_numCycles; ++x)
    {
      if (aLUT[x * 5 + 1] <= n && aLUT[x * 5 + 3] > n)
      {
        cycleF = x;
        break;
      }
    }
    if (cycleF > 0 && prev.frame != aLUT[(cycleF - 1) * 5])
    {
      if (curr.frame == aLUT[(cycleF - 1) * 5]) prev = curr;
      else
      {
        prev.setFrame(aLUT[(cycleF - 1) * 5]);
        getOvrCycle(prev, true);
        calcMetricCycle(prev, env, vi, true, false);
        addMetricCycle(prev);
      }
    }
    else if (cycleF <= 0) prev.setFrame(-prev.length);
    if (curr.frame != aLUT[cycleF * 5])
    {
      if (next.frame == aLUT[cycleF * 5]) curr = next;
      else
      {
        curr.setFrame(aLUT[cycleF * 5]);
        getOvrCycle(curr, true);
        calcMetricCycle(curr, env, vi, true, false);
        addMetricCycle(curr);
      }
    }
    if (cycleF < mode2_numCycles - 1 && next.frame != aLUT[(cycleF + 1) * 5])
    {
      next.setFrame(aLUT[(cycleF + 1) * 5]);
      getOvrCycle(next, true);
      calcMetricCycle(next, env, vi, true, false);
      addMetricCycle(next);
    }
    else if (cycleF >= mode2_numCycles - 1) next.setFrame(-next.length);
    mode2MarkDecFrames(cycleF);
    ret = getNonDecMode2(n - aLUT[cycleF * 5 + 1], aLUT[cycleF * 5], aLUT[cycleF * 5 + 2]);
  }
  else ret = aLUT[n];
  if (ret < 0)
    env->ThrowError("TDecimate:  mode 2 internal error (ret less than 0). " \
      "Please report this to tritical ASAP!");
  if (debug)
  {
    sprintf(buf, "TDecimate:  inframe = %d  useframe = %d  rate = %3.6f\n", n, ret, rate);
    OutputDebugString(buf);
  }

  const VideoInfo vi2 = (!useclip2) ? child->GetVideoInfo() : clip2->GetVideoInfo();

  if (display)
  {
    PVideoFrame dst;
    if (!useclip2) dst = child->GetFrame(ret, env);
    else dst = clip2->GetFrame(ret, env);
    env->MakeWritable(&dst);

    sprintf(buf, "TDecimate %s by tritical", VERSION);
    Draw(dst, 0, 0, buf, vi2);
    sprintf(buf, "Mode: 2  Rate = %3.6f", rate);
    Draw(dst, 0, 1, buf, vi2);
    sprintf(buf, "inframe = %d  useframe = %d", n, ret);
    Draw(dst, 0, 2, buf, vi2);
    return dst;
  }
  if (!useclip2) return child->GetFrame(ret, env);
  return clip2->GetFrame(ret, env);
}

int TDecimate::getNonDecMode2(int n, int start, int stop) const
{
  int count = -1, ret = -1;
  for (int i = start; i < stop; ++i)
  {
    if (mode2_decA[i] == 0) ++count;
    if (count == n) { ret = i; break; }
  }
  return ret;
}

void TDecimate::mode2MarkDecFrames(int cycleF)
{
  for (int i = curr.frame; i < curr.frameEO; ++i)
  {
    if (mode2_decA[i] != -20) return;
    mode2_decA[i] = 0;
  }
  removeMinN(mode2_num, mode2_den, curr.frame, curr.frameEO);
  for (int x = 0; x < 10; ++x)
  {
    if (mode2_cfs[x] <= 0) break;
    if (aLUT[cycleF * 5 + 4] & (1 << x))
    {
      if (mode2_cfs[x] <= curr.length)
        removeMinN(1, mode2_cfs[x], curr.frame, curr.frameEO);
      else
        removeMinN(1, curr.length, curr.frame, curr.frameEO);
    }
  }
}

void TDecimate::removeMinN(int m, int n, int start, int stop)
{
  for (int x = start; x < stop; x += n)
  {
    int dec = 0, t = 0, stop2 = n;
    if (x + n - 1 > nfrms)
    {
      m = (int)(double((nfrms - x + 1)*m) / double(n) + 0.5);
      if (m < 1) continue;
      stop2 = nfrms - x + 1;
    }
    if (curr.dupCount > 0)
    {
      int b = x - start;
      for (int i = 0; i < stop2; ++i, ++b)
      {
        if (curr.dupArray[b] == 1 && dec < m)
        {
          mode2_decA[x + i] = 1;
          --curr.dupCount;
          curr.dupArray[b] = 0;
          ++dec;
        }
      }
      if (dec >= m) continue;
    }
    for (int i = 0; i < stop2; ++i)
    {
      if (mode2_decA[x + i] == 0)
      {
        int v = 1;
        double cM = (metricsOutArray[(x + i) << 1] * 100.0) / MAX_DIFF;
        double pM = -20.0, nM = -20.0;
        while (pM < 0 || nM < 0)
        {
          if (pM < 0)
          {
            if (x + i - v >= 0)
            {
              if (mode2_decA[x + i - v] == 0 || mode2_decA[x + i - v] == -20)
                pM = (metricsOutArray[(x + i - v) << 1] * 100.0) / MAX_DIFF;
            }
            else pM = 1.0;
          }
          if (nM < 0)
          {
            if (x + i + v <= nfrms)
            {
              if (mode2_decA[x + i + v] == 0 || mode2_decA[x + i + v] == -20)
                nM = (metricsOutArray[(x + i + v) << 1] * 100.0) / MAX_DIFF;
            }
            else nM = 1.0;
          }
          ++v;
        }
        if (pM >= 3.0 && nM >= 3.0 && cM < 3.0 && pM*0.5 > cM && nM*0.5 > cM)
        {
          mode2_order[t] = i;
          mode2_metrics[t] = (int)(std::min(pM - cM, nM - cM)*10000.0 + 0.5);
          ++t;
        }
      }
    }
    if (t > 0)
    {
      sortMetrics(mode2_metrics.data(), mode2_order.data(), t);
      for (int i = 0; i < t && dec < m; ++i)
      {
        if (mode2_decA[x + mode2_order[t - 1 - i]] != 1)
        {
          mode2_decA[x + mode2_order[t - 1 - i]] = 1;
          ++dec;
        }
      }
    }
    if (dec >= m) continue;
    for (int i = 0; i < stop2; ++i)
    {
      mode2_order[i] = i;
      mode2_metrics[i] = metricsOutArray[(x + i) << 1];
    }
    sortMetrics(mode2_metrics.data(), mode2_order.data(), n);
    for (int i = 0; i < stop2 && dec < m; ++i)
    {
      if (mode2_decA[x + mode2_order[i]] != 1)
      {
        mode2_decA[x + mode2_order[i]] = 1;
        ++dec;
      }
    }
  }
}

void TDecimate::removeMinN(int m, int n, uint64_t *metricsT, int *orderT, int &ovrC)
{
  for (int x = 0; x < vi.num_frames; x += n)
  {
    int dec = 0, t = 0, stop2 = n;
    if (x + n - 1 > nfrms)
    {
      m = (int)(double((nfrms - x + 1)*m) / double(n) + 0.5);
      if (m < 1) continue;
      stop2 = nfrms - x + 1;
    }
    if (ovrC > 0 && ovrArray.size())
    {
      for (int i = 0; i < stop2; ++i)
      {
        if ((ovrArray[x + i] & DROP_FRAME) && dec < m)
        {
          mode2_decA[x + i] = 1;
          --ovrC;
          ovrArray[x + i] &= ~DROP_FRAME;
          ++dec;
        }
      }
      if (dec >= m) continue;
    }
    for (int i = 0; i < stop2; ++i)
    {
      if (mode2_decA[x + i] == 0)
      {
        int v = 1;
        double cM = (metricsArray[(x + i) << 1] * 100.0) / MAX_DIFF;
        double pM = -20.0, nM = -20.0;
        while (pM < 0 || nM < 0)
        {
          if (pM < 0)
          {
            if (x + i - v >= 0)
            {
              if (mode2_decA[x + i - v] != 1)
                pM = (metricsArray[(x + i - v) << 1] * 100.0) / MAX_DIFF;
            }
            else pM = 1.0;
          }
          if (nM < 0)
          {
            if (x + i + v <= nfrms)
            {
              if (mode2_decA[x + i + v] != 1)
                nM = (metricsArray[(x + i + v) << 1] * 100.0) / MAX_DIFF;
            }
            else nM = 1.0;
          }
          ++v;
        }
        if (pM >= 3.0 && nM >= 3.0 && cM < 3.0 && pM*0.5 > cM && nM*0.5 > cM)
        {
          orderT[t] = i;
          metricsT[t] = (int)(std::min(pM - cM, nM - cM)*10000.0 + 0.5);
          ++t;
        }
      }
    }
    if (t > 0)
    {
      sortMetrics(metricsT, orderT, t);
      for (int i = 0; i < t && dec < m; ++i)
      {
        if (mode2_decA[x + orderT[t - 1 - i]] != 1)
        {
          mode2_decA[x + orderT[t - 1 - i]] = 1;
          ++dec;
        }
      }
    }
    if (dec >= m) continue;
    for (int i = 0; i < stop2; ++i)
    {
      orderT[i] = i;
      metricsT[i] = metricsArray[(x + i) << 1];
    }
    sortMetrics(metricsT, orderT, stop2);
    for (int i = 0; i < stop2 && dec < m; ++i)
    {
      if (mode2_decA[x + orderT[i]] != 1)
      {
        mode2_decA[x + orderT[i]] = 1;
        ++dec;
      }
    }
  }
}

void TDecimate::sortMetrics(uint64_t *metrics, int *order, int length) const
{
  for (int i = 1; i < length; ++i)
  {
    int j = i;
    const uint64_t temp1 = metrics[j];
    const int temp2 = order[j];
    while (j > 0 && metrics[j - 1] > temp1)
    {
      metrics[j] = metrics[j - 1];
      order[j] = order[j - 1];
      --j;
    }
    metrics[j] = temp1;
    order[j] = temp2;
  }
}

int TDecimate::findDivisor(double decRatio, int min_den) const
{
  int ret = -20;
  double num = 1.0, lowest = 5.0;
  double offset = 0.00000001;
  for (int x = min_den; x <= 100; ++x)
  {
    double temp = num / ((double)x);
    if (temp > decRatio + offset) continue;
    if (fabs(temp - decRatio) < lowest)
    {
      lowest = fabs(temp - decRatio);
      ret = x;
    }
  }
  return ret;
}

int TDecimate::findNumerator(double decRatio, int divisor) const
{
  int ret = -20;
  double den = (double)divisor, lowest = 5.0;
  double offset = 0.00000001;
  for (int x = 1; x < divisor; ++x)
  {
    double temp = ((double)x) / den;
    if (temp > decRatio + offset) continue;
    if (fabs(temp - decRatio) < lowest)
    {
      lowest = fabs(temp - decRatio);
      ret = x;
    }
  }
  return ret;
}

double TDecimate::findCorrectionFactors(double decRatio, int num, int den, int rc[10], IScriptEnvironment *env) const
{
  double approx = ((double)num) / ((double)den);
  memset(rc, 0, 10 * sizeof(int));
  for (int x = 0; x < 10; ++x)
  {
    double error = decRatio - approx;
    if (error <= 0.0) break;
    double length = 1.0 / error;
    if (length > vi.num_frames) break;
    int multof = x == 0 ? den : rc[x - 1];
    rc[x] = (int)(length + 0.5);
    if (rc[x] % multof) rc[x] += multof - (rc[x] % multof);
    if (rc[x] > vi.num_frames) rc[x] = vi.num_frames;
    approx += 1.0 / ((double)rc[x]);
  }
  if ((1.0 / fabs(decRatio - approx)) < vi.num_frames)
    env->ThrowError("TDecimate:  mode 2 error, unable to achieve a completely synced result!");
  return approx;
}

double TDecimate::buildDecStrategy(IScriptEnvironment *env)
{
  double frRatio = fps / rate;
  double rfRatio = rate / fps;
  if (rfRatio >= 99.0 / 100.0 || rfRatio <= 1.0 / 100.0)
    env->ThrowError("TDecimate:  mode 2 error, unable to achieve desired decimation ratio!");
  double decRatio = 1.0 - rfRatio;
  if (frRatio < 3.0)
  {
    mode2_num = 1;
    mode2_den = findDivisor(decRatio, maxndl > 0 ? maxndl < 99 ? maxndl + 1 : 2 : 2);
  }
  else
  {
    mode2_den = (int)frRatio;
    mode2_num = findNumerator(decRatio, mode2_den);
    if (maxndl > 0 && maxndl < 99 && mode2_num - mode2_den < maxndl) mode2_den = mode2_num + maxndl;
  }
  if (mode2_den <= 0 || mode2_num <= 0 || mode2_num > 100 || mode2_den > 100 || mode2_num >= mode2_den)
    env->ThrowError("TDecimate:  mode 2 invalid num and den results!");
  int clength = mode2_den, rc[10], arc[10];
  double aRate = fps*(1.0 - findCorrectionFactors(decRatio, mode2_num, mode2_den, rc, env));
  for (int x = 0; x < 10; ++x)
  {
    if (rc[x] > 0 && (rc[x] <= 100 || m2PA) && rc[x] > clength)
      clength = rc[x];
  }
  if (clength == mode2_den && rc[0] > 0 && (mode2_den - mode2_num <= 1 || mode2_den <= 25))
  {
    while (clength <= 50) clength *= 2;
  }
  int cdrop = ((int)(clength / mode2_den))*mode2_num;
  int rstart = -20;
  for (int x = 0; x < 10; ++x)
  {
    if (rc[x] > 0 && rc[x] > clength) { rstart = x; break; }
    if (rc[x] > 0 && rc[x] <= clength) cdrop += (int)(clength / rc[x]);
  }
  if (rstart == -20) rstart = 11;
  mode2_numCycles = (int)((double)vi.num_frames / (double)clength + 1.0);
  bool allMetrics = true;
  if (metricsArray.size())
  {
    for (int h = 0; h < vi.num_frames * 2; h += 2)
    {
      if (metricsArray[h] == UINT64_MAX) { allMetrics = false; break; }
    }
  }
  else allMetrics = false;
  if (aLUT.size()) aLUT.resize(0);
  if (allMetrics)
  {
    aLUT.resize((int)(vi.num_frames*rate / fps), 0);

    std::vector<int> orderT(vi.num_frames, 0);
    std::vector<uint64_t> metricsT(vi.num_frames, 0);
    memset(mode2_decA.data(), 0, vi.num_frames * sizeof(int));


    memset(mode2_decA.data(), 0, vi.num_frames * sizeof(int));
    int ovrC = 0;
    if (ovrArray.size())
    {
      for (int i = 0; i < vi.num_frames; ++i)
      {
        if (ovrArray[i] & DROP_FRAME) ++ovrC;
      }
    }
    removeMinN(mode2_num, mode2_den, metricsT.data(), orderT.data(), ovrC);
    for (int x = 0; x < 10; ++x)
    {
      if (rc[x] > 0)
        removeMinN(1, rc[x], metricsT.data(), orderT.data(), ovrC);
    }
    int v = 0, tc = (int)(vi.num_frames*aRate / fps);
    for (int i = 0; i < vi.num_frames && v < tc; ++i)
    {
      if (mode2_decA[i] != 1)
      {
        aLUT[v] = i;
        ++v;
      }
    }
    mode2_decA.resize(0);
    if (debug)
    {
      sprintf(buf, "drop count = %d  expected = %d\n", vi.num_frames - v,
        vi.num_frames - (int)(vi.num_frames*aRate / fps));
      OutputDebugString(buf);
    }
    mode2_numCycles = -20;
  }
  else
  {
    aLUT.resize(mode2_numCycles * 5, 0);
    int temp = 0;
    for (int x = 0; x < 10; ++x)
    {
      if (rc[x] > 0 && rc[x] <= clength)
        temp |= (1 << x);
    }
    for (int x = 0; x < mode2_numCycles; ++x)
      aLUT[x * 5 + 4] = temp;
    memset(arc, 0, 10 * sizeof(int));
    int dropCount = 0;
    for (int x = 0; x < mode2_numCycles; ++x)
    {
      int add = (x + 1)*clength >= vi.num_frames ? vi.num_frames - x*clength : clength;
      aLUT[x * 5 + 0] = x*clength;
      aLUT[x * 5 + 1] = x*clength - dropCount;
      aLUT[x * 5 + 2] = x*clength + add;
      dropCount += add == clength ? cdrop : (int)(double(add*cdrop) / double(clength) + 0.5);
      for (int i = rstart; i < 10; ++i)
      {
        int mt = rc[i] >> 1;
        if (mt%clength) mt += clength - (mt%clength);
        if (rc[i] > 0 && arc[i] + add >= mt)
        {
          if (add >= ((clength + 1) >> 1))
          {
            ++dropCount;
            aLUT[x * 5 + 4] |= (1 << i);
          }
          arc[i] = mt - rc[i];
        }
        else if (rc[i] > 0) arc[i] += add;
      }
      aLUT[x * 5 + 3] = x*clength + add - dropCount;
    }
    if (debug)
    {
      sprintf(buf, "drop count = %d  expected = %d\n", dropCount,
        vi.num_frames - (int)(vi.num_frames*aRate / fps));
      OutputDebugString(buf);
    }
    if (clength != 5)
    {
      prev.setSize(clength);
      curr.setSize(clength);
      next.setSize(clength);
    }
    prev.length = curr.length = next.length = clength;
  }
  memcpy(mode2_cfs, rc, 10 * sizeof(int));
  if (debug)
  {
    sprintf(buf, "rate = %f  actual rate = %f\n", rate, aRate);
    OutputDebugString(buf);
    sprintf(buf, "mode2_num = %d  mode2_den = %d  numCycles = %d  clength = %d\n", mode2_num, mode2_den, mode2_numCycles, clength);
    OutputDebugString(buf);
    for (int x = 0; x < 10; ++x)
    {
      if (mode2_cfs[x] <= 0) break;
      sprintf(buf, "mode2_cfs %d = %d\n", x, mode2_cfs[x]);
      OutputDebugString(buf);
    }
  }
  return aRate;
}