/*
**                    TIVTC v1.0.14 for Avisynth 2.6 interface
**
**   TIVTC includes a field matching filter (TFM) and a decimation
**   filter (TDecimate) which can be used together to achieve an
**   IVTC or for other uses. TIVTC currently supports YV12 and
**   YUY2 colorspaces.
**
**   Copyright (C) 2004-2008 Kevin Stone, additional work (C) 2017 pinterf
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

#include "FrameDiff.h"

FrameDiff::FrameDiff(PClip _child, int _mode, bool _prevf, int _nt, int _blockx, int _blocky,
  bool _chroma, double _thresh, int _display, bool _debug, bool _norm, bool _predenoise, bool _ssd,
  bool _rpos, int _opt, IScriptEnvironment *env) : GenericVideoFilter(_child),
  predenoise(_predenoise), ssd(_ssd), rpos(_rpos),
  nt(_nt), blockx(_blockx), blocky(_blocky), mode(_mode), display(_display),
  thresh(_thresh),
  opt(_opt), chroma(_chroma), debug(_debug), prevf(_prevf), norm(_norm)
{
  diff = NULL;
  if (vi.BitsPerComponent() != 8)
    env->ThrowError("FrameDiff:  Only 8 bit clip data supported!");
  if (vi.IsRGB() || vi.IsYV411())
    env->ThrowError("FrameDiff:  RGB or 411 data not supported!");
  if (vi.height & 1)
    env->ThrowError("FrameDiff:  height must be mod 2!");
  if (vi.height < 8)
    env->ThrowError("FrameDiff:  height must be at least 8!");
  if (mode != 0 && mode != 1)
    env->ThrowError("FrameDiff:  mode must be set to 0 or 1!");
  if (display < 0 || display > 4)
    env->ThrowError("FrameDiff:  display must be set to 0, 1, 2, or 3!");
  if (blockx != 4 && blockx != 8 && blockx != 16 && blockx != 32 && blockx != 64 &&
    blockx != 128 && blockx != 256 && blockx != 512 && blockx != 1024 && blockx != 2048)
    env->ThrowError("FrameDiff:  illegal blockx size!");
  if (blocky != 4 && blocky != 8 && blocky != 16 && blocky != 32 && blocky != 64 &&
    blocky != 128 && blocky != 256 && blocky != 512 && blocky != 1024 && blocky != 2048)
    env->ThrowError("FrameDiff:  illegal blocky size!");
  if (opt < 0 || opt > 4)
    env->ThrowError("FrameDiff:  opt must be set to 0, 1, 2, 3, or 4!");
  blockx_shift = blockx == 4 ? 2 : blockx == 8 ? 3 : blockx == 16 ? 4 : blockx == 32 ? 5 :
    blockx == 64 ? 6 : blockx == 128 ? 7 : blockx == 256 ? 8 : blockx == 512 ? 9 :
    blockx == 1024 ? 10 : 11; // log2 blockx
  blocky_shift = blocky == 4 ? 2 : blocky == 8 ? 3 : blocky == 16 ? 4 : blocky == 32 ? 5 :
    blocky == 64 ? 6 : blocky == 128 ? 7 : blocky == 256 ? 8 : blocky == 512 ? 9 :
    blocky == 1024 ? 10 : 11; // log2 blocky
  // not chroma block sizes, really blockx / 2
  blocky_half = blocky >> 1; // not yv12 specific, really half
  blockx_half = blockx >> 1;

  /*
  const int blockxC = vi.IsYUY2() ? blocky >> 1 : blocky >> vi.GetPlaneHeightSubsampling(PLANAR_U); // fixme: yv12 specific?
  const int blockYC = vi.IsYUY2() ? blockx >> 1 : blockx >> vi.GetPlaneWidthSubsampling(PLANAR_U); // fixme: vy12/yuy2 specific?
  */
  if (!vi.IsYUY2())
  {
    if (chroma) // Y + U + V
    {
      // fixme: common metrics, like in mvtools? 4:2:2 has greater chroma weight!
      // keep xhalfS and yhalfS if YV16/24 ssd/sad is converted to YV12-like metrics
      // or else use blockXC and blockYC
      
      // Y: 219 = 235-15
      // UV: 224 = 240-16
      const int blockx_chroma = blockx >> vi.GetPlaneWidthSubsampling(PLANAR_U);
      const int blocky_chroma = blocky >> vi.GetPlaneHeightSubsampling(PLANAR_U);
      if (ssd)
        MAX_DIFF = (uint64_t)(sqrt(219.0*219.0*blockx*blocky + 224.0*224.0* blockx_chroma * blocky_chroma *2.0)); // *2: 2 chroma planes
      else 
        MAX_DIFF = (uint64_t)(219.0*blockx*blocky + 224.0* blockx_chroma * blocky_chroma *2.0);
    }
    else
    {
      // luma only
      if (ssd) 
        MAX_DIFF = (uint64_t)(sqrt(219.0*219.0*blockx*blocky));
      else 
        MAX_DIFF = (uint64_t)(219.0*blockx*blocky);
    }
  }
  else
  {
    // YUY2
    if (chroma)
    {
      if (ssd) // fixme: why *4*0.625 (*2.5)
        MAX_DIFF = (uint64_t)(sqrt(219.0*219.0*blockx*blocky + 224.0*224.0*blockx_half*blocky_half*4.0*0.625));
      else 
        MAX_DIFF = (uint64_t)(219.0*blockx*blocky + 224.0*blockx_half*blocky_half*4.0*0.625);
    }
    else
    {
      if (ssd) 
        MAX_DIFF = (uint64_t)(sqrt(219.0*219.0*blockx*blocky));
      else 
        MAX_DIFF = (uint64_t)(219.0*blockx*blocky);
    }
  }
  // diff size: here is for blocks. xhalfS is really blockx >> 1
  diff = (uint64_t*)_aligned_malloc((((vi.width + (blockx >> 1)) >> blockx_shift) + 1) * (((vi.height + (blocky >> 1)) >> blocky_shift) + 1) 
    * 4 // diff blocks are 2x2
    * sizeof(uint64_t), 16);
  //diff = (uint64_t*)_aligned_malloc((((vi.width + xhalfS) >> xshiftS) + 1)*(((vi.height + yhalfS) >> yshiftS) + 1) * 4 * sizeof(uint64_t), 16);
  if (diff == NULL) env->ThrowError("FrameDiff:  malloc failure (diff)!");
  nfrms = vi.num_frames - 1;
  child->SetCacheHints(CACHE_GENERIC, 3);
  threshU = uint64_t(double(MAX_DIFF)*thresh / 100.0 + 0.5);
  if (debug)
  {
    sprintf(buf, "FrameDiff:  %s by tritical\n", VERSION);
    OutputDebugString(buf);
  }
}

FrameDiff::~FrameDiff()
{
  if (diff) _aligned_free(diff);
}

AVSValue FrameDiff::ConditionalFrameDiff(int n, IScriptEnvironment* env)
{
  if (n < 0) n = 0;
  else if (n > nfrms) n = nfrms;
  int np = vi.IsYUY2() ? 1 : 3;
  int xblocks = ((vi.width + blockx_half) >> blockx_shift) + 1;
  int yblocks = ((vi.height + blocky_half) >> blocky_shift) + 1;
  int arraysize = (xblocks*yblocks) << 2;
  bool lset = false, hset = false;
  uint64_t lowestDiff = ULLONG_MAX, highestDiff = 0;
  int lpos, hpos;
  PVideoFrame src;
  if (prevf && n >= 1)
  {
    PVideoFrame prv = child->GetFrame(n - 1, env);
    src = child->GetFrame(n, env);
    calcMetric(prv, src, np, env);
  }
  else if (!prevf && n < nfrms)
  {
    src = child->GetFrame(n, env);
    PVideoFrame nxt = child->GetFrame(n + 1, env);
    calcMetric(src, nxt, np, env);
  }
  else
  {
    if (norm) return 0.0;
    return 0;
  }
  for (int x = 0; x < arraysize; ++x)
  {
    if (diff[x] > highestDiff || !hset)
    {
      hset = true;
      highestDiff = diff[x];
      hpos = getCoord(x, xblocks * 4);
    }
    if (diff[x] < lowestDiff || !lset)
    {
      lset = true;
      lowestDiff = diff[x];
      lpos = getCoord(x, xblocks * 4);
    }
  }
  if (ssd)
  {
    highestDiff = (uint64_t)(sqrt((double)highestDiff));
    lowestDiff = (uint64_t)(sqrt((double)lowestDiff));
  }
  if (debug)
  {
    sprintf(buf, "FrameDiff:  frame %d to %d  metricH = %3.2f %I64u  " \
      "metricL = %3.2f %I64u  hpos = %d  lpos = %d", n,
      prevf ? mapn(n - 1) : mapn(n + 1), double(highestDiff)*100.0 / double(MAX_DIFF),
      highestDiff, double(lowestDiff)*100.0 / double(MAX_DIFF), lowestDiff,
      hpos, lpos);
    OutputDebugString(buf);
  }
  if (rpos)
  {
    if (mode == 0) return lpos;
    return hpos;
  }
  if (norm)
  {
    if (mode == 0) return (double(lowestDiff)*100.0 / double(MAX_DIFF));
    return (double(highestDiff)*100.0 / double(MAX_DIFF));
  }
  if (mode == 0) return int(lowestDiff);
  return int(highestDiff);
}

int FrameDiff::getCoord(int blockN, int xblocks)
{
  int cordy = blockN / xblocks;
  int cordx = blockN - (cordy*xblocks);
  int temp = cordx % 4;
  cordx = (cordx >> 2);
  cordy *= blocky;
  cordx *= blockx;
  if (temp == 1) cordx -= (blockx >> 1);
  else if (temp == 2) cordy -= (blocky >> 1);
  else if (temp == 3) { cordx -= (blockx >> 1); cordy -= (blocky >> 1); }
  return cordy*vi.width + cordx;
}

PVideoFrame __stdcall FrameDiff::GetFrame(int n, IScriptEnvironment *env)
{
  if (n < 0) n = 0;
  else if (n > nfrms) n = nfrms;
  int np = vi.IsYUY2() ? 1 : 3;
  int xblocks = ((vi.width + blockx_half) >> blockx_shift) + 1; // same as in constructor
  int xblocks4 = xblocks << 2;
  int yblocks = ((vi.height + blocky_half) >> blocky_shift) + 1; // same as in constructor
  int arraysize = (xblocks*yblocks) << 2;
  PVideoFrame src;
  if (prevf && n >= 1)
  {
    PVideoFrame prv = child->GetFrame(n - 1, env);
    src = child->GetFrame(n, env);
    calcMetric(prv, src, np, env);
  }
  else if (!prevf && n < nfrms)
  {
    src = child->GetFrame(n, env);
    PVideoFrame nxt = child->GetFrame(n + 1, env);
    calcMetric(src, nxt, np, env);
  }
  else
  {
    src = child->GetFrame(n, env);
    memset(diff, 0, arraysize * sizeof(uint64_t));
  }
  env->MakeWritable(&src);
  int hpos, lpos;
  int blockH = -20, blockL = -20;
  uint64_t lowestDiff = ULLONG_MAX, highestDiff = 0, difft;
  if (display > 2)
    setBlack(src);
  for (int x = 0; x < arraysize; ++x)
  {
    difft = diff[x];
    if (ssd) difft = (uint64_t)(sqrt((double)difft));
    if ((difft > highestDiff || blockH == -20) && checkOnImage(x, xblocks4))
    {
      highestDiff = difft;
      blockH = x;
      hpos = getCoord(x, xblocks * 4);
    }
    if ((difft < lowestDiff || blockL == -20) && checkOnImage(x, xblocks4))
    {
      lowestDiff = difft;
      blockL = x;
      lpos = getCoord(x, xblocks * 4);
    }
    if ((display == 2 || display == 4) && mode == 0 && difft <= threshU)
      fillBox(src, x, xblocks4, display == 2 ? true : false);
    else if ((display == 2 || display == 4) && mode == 1 && difft >= threshU)
      fillBox(src, x, xblocks4, display == 2 ? true : false);
  }
  if (display > 0 && display < 3)
  {
    sprintf(buf, "FrameDiff %s by tritical", VERSION);
    Draw(src, 0, 0, buf, np);
    sprintf(buf, "chroma = %c  denoise = %c  ssd = %c", chroma ? 'T' : 'F',
      predenoise ? 'T' : 'F', ssd ? 'T' : 'F');
    Draw(src, 0, 1, buf, np);
    sprintf(buf, "blockx = %d  blocky = %d", blockx, blocky);
    Draw(src, 0, 2, buf, np);
    sprintf(buf, "hpos = %d  lpos = %d", hpos, lpos);
    Draw(src, 0, 3, buf, np);
    if (norm)
    {
      sprintf(buf, "Frame %d to %d:  thresh = %3.2f", n, prevf ? mapn(n - 1) : mapn(n + 1), thresh);
      Draw(src, 0, 4, buf, np);
      sprintf(buf, "H = %3.2f  L = %3.2f", double(highestDiff)*100.0 / double(MAX_DIFF),
        double(lowestDiff)*100.0 / double(MAX_DIFF));
      Draw(src, 0, 5, buf, np);
    }
    else
    {
      sprintf(buf, "Frame %d to %d:  thresh = %I64u", n, prevf ? mapn(n - 1) : mapn(n + 1), threshU);
      Draw(src, 0, 4, buf, np);
      sprintf(buf, "H = %I64u  L = %I64u", highestDiff, lowestDiff);
      Draw(src, 0, 5, buf, np);
    }
  }
  if (display == 1)
  {
    if (mode == 0) drawBox(src, blockL, xblocks4, np);
    else drawBox(src, blockH, xblocks4, np);
  }
  else if (display == 3)
  {
    if (mode == 0) fillBox(src, blockL, xblocks4, false);
    else fillBox(src, blockH, xblocks4, false);
  }
  if (debug)
  {
    sprintf(buf, "FrameDiff:  frame %d to %d  metricH = %3.2f %I64u  metricL = %3.2f %I64u", n,
      prevf ? mapn(n - 1) : mapn(n + 1), double(highestDiff)*100.0 / double(MAX_DIFF),
      highestDiff, double(lowestDiff)*100.0 / double(MAX_DIFF), lowestDiff);
    OutputDebugString(buf);
  }
  return src;
}

void FrameDiff::setBlack(PVideoFrame &d)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsYUY2() ? 1 : 3;
  for (int i = 0; i < stop; ++i) { // PF 20200412 was: 3 even for YUY2?
    const int plane = planes[i];
    memset(d->GetWritePtr(plane), 0,
      d->GetPitch(plane) * d->GetHeight(plane));
  }
}

bool FrameDiff::checkOnImage(int x, int xblocks4)
{
  int cordy = x / xblocks4;
  int cordx = x - (cordy*xblocks4);
  int temp = cordx % 4;
  cordx = (cordx >> 2);
  cordy *= blocky;
  cordx *= blockx;
  if (temp == 1) cordx -= (blockx >> 1);
  else if (temp == 2) cordy -= (blocky >> 1);
  else if (temp == 3) { cordx -= (blockx >> 1); cordy -= (blocky >> 1); }
  if (cordy >= vi.height) return false;
  if (cordx >= vi.width) return false;
  if (mode == 1) return true;
  if (cordy < 0 || cordy + blocky_half >= vi.height) return false;
  if (cordx < 0 || cordx + blockx_half >= vi.width) return false;
  return true;
}

int FrameDiff::mapn(int n)
{
  if (n < 0) return 0;
  if (n > nfrms) return nfrms;
  return n;
}

void FrameDiff::calcMetric(PVideoFrame &prevt, PVideoFrame &currt, int np, IScriptEnvironment *env)
{
  struct CalcMetricData d;
  d.np = np;
  d.predenoise = predenoise;
  d.vi = vi;
  d.chroma = chroma;
  d.opt = opt;
  d.blockx_half = blockx;
  d.blockx_half = blockx_half;
  d.blockx_shift = blockx_shift;
  d.blocky = blocky;
  d.blocky_half = blocky_half;
  d.blocky_shift = blocky_shift;
  d.diff = diff;
  d.nt = nt;
  d.ssd = ssd;

  d.metricF_needed = false;
  // only for TDecimate:
  uint64_t dummy_metricF;
  d.metricF = &dummy_metricF;
  d.scene = false; // n/a

  CalcMetricsExtracted(env, prevt, currt, d);
}

// the same core is in calcMetricCycle
void CalcMetricsExtracted(IScriptEnvironment* env, PVideoFrame& prevt, PVideoFrame& currt, CalcMetricData& d)
{
  PVideoFrame prev, curr;

  if (d.predenoise)
  {
    prev = env->NewVideoFrame(d.vi);
    curr = env->NewVideoFrame(d.vi);
    blurFrame(prevt, prev, d.np, 2, d.chroma, env, d.vi, d.opt);
    blurFrame(currt, curr, d.np, 2, d.chroma, env, d.vi, d.opt);
  }
  else
  {
    prev = prevt;
    curr = currt;
  }

  // core start

  const unsigned char* prvp, * curp;
  int prv_pitch, cur_pitch, width, height;

  int xblocks = ((d.vi.width + d.blockx_half) >> d.blockx_shift) + 1;
  int xblocks4 = xblocks << 2;
  int yblocks = ((d.vi.height + d.blocky_half) >> d.blocky_shift) + 1;
  int arraysize = (xblocks * yblocks) << 2;

  long cpu = env->GetCPUFlags();
  if (d.opt == 0) cpu = 0;

  memset(d.diff, 0, arraysize * sizeof(uint64_t));

  const bool IsYUY2 = d.vi.IsYUY2();
  const int stop = d.chroma ? d.np : 1; // chroma: full 3 planes at planar, 1 at YUY2 or planar luma-only
  const int inc = IsYUY2 ? (d.chroma ? 1 : 2) : 1; // 2 is YUY2 luma only, otherwise 1

  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };

  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    prvp = prev->GetReadPtr(plane);
    prv_pitch = prev->GetPitch(plane);
    width = prev->GetRowSize(plane);
    height = prev->GetHeight(plane);
    curp = curr->GetReadPtr(plane);
    cur_pitch = curr->GetPitch(plane);

    // fixme: correct chroma diff for yv16 and yv24 to the yv12 metrics (div by 2 for yv16, div by 4 for yv24)?
    // or use Chromaweight correction factor like in mvtools2
    // sum is gathered in uint64_t diff

    if (d.blockx == 32 && d.blocky == 32 && d.nt <= 0)
    {
      if (d.ssd && (cpu & CPUF_SSE2))
        calcDiffSSD_32x32_SSE2(prvp, curp, prv_pitch, cur_pitch, width, height, plane, xblocks4, d.np, d.diff, d.chroma, d.vi);
      else if (!d.ssd && (cpu & CPUF_SSE2))
        calcDiffSAD_32x32_SSE2(prvp, curp, prv_pitch, cur_pitch, width, height, plane, xblocks4, d.np, d.diff, d.chroma, d.vi);
      else { goto use_c; }
    }
    else if (((!IsYUY2 && d.blockx >= 16 && d.blocky >= 16) || (IsYUY2 && d.blockx >= 8 && d.blocky >= 8)) && d.nt <= 0)
    {
      // YUY2 block size 8 is really 16 in width because luma + chroma
      if (d.ssd && (cpu & CPUF_SSE2))
        calcDiffSSD_Generic_SSE2(prvp, curp, prv_pitch, cur_pitch, width, height, plane, xblocks4, d.np, d.diff, d.chroma, d.blockx_shift, d.blocky_shift, d.blockx_half, d.blocky_half, d.vi);
      else if (!d.ssd && (cpu & CPUF_SSE2))
        calcDiffSAD_Generic_SSE2(prvp, curp, prv_pitch, cur_pitch, width, height, plane, xblocks4, d.np, d.diff, d.chroma, d.blockx_shift, d.blocky_shift, d.blockx_half, d.blocky_half, d.vi);
      else { goto use_c; }
    }
    else
    {
    use_c:
      if (!d.ssd) {
        // SAD
        if (inc == 2) // YUY2 luma only
          calcDiff_SADorSSD_Generic_c<true, 2>(prvp, curp, prv_pitch, cur_pitch, width, height, plane, xblocks4, d.np, d.diff, d.chroma, d.blockx_shift, d.blocky_shift, d.blockx_half, d.blocky_half, d.nt, d.vi);
        else
          calcDiff_SADorSSD_Generic_c<true, 1>(prvp, curp, prv_pitch, cur_pitch, width, height, plane, xblocks4, d.np, d.diff, d.chroma, d.blockx_shift, d.blocky_shift, d.blockx_half, d.blocky_half, d.nt, d.vi);
      }
      else {
        // SSD
        if (inc == 2) // YUY2 luma only
          calcDiff_SADorSSD_Generic_c<false, 2>(prvp, curp, prv_pitch, cur_pitch, width, height, plane, xblocks4, d.np, d.diff, d.chroma, d.blockx_shift, d.blocky_shift, d.blockx_half, d.blocky_half, d.nt, d.vi);
        else
          calcDiff_SADorSSD_Generic_c<false, 1>(prvp, curp, prv_pitch, cur_pitch, width, height, plane, xblocks4, d.np, d.diff, d.chroma, d.blockx_shift, d.blocky_shift, d.blockx_half, d.blocky_half, d.nt, d.vi);
      }
    }

    if (d.metricF_needed) { // called from TDecimate. from FrameDiff:false
      if (b == 0) // luma
      {
        d.metricF = 0;
        if (d.scene)
        {
          if (d.np == 3 || !d.chroma) // planar or YUY2 luma+chroma
          {
            for (int x = 0; x < arraysize; x += 4)
              d.metricF += d.diff[x];
          }
          else
          {
            // YUY2 only, luma only chroma skip
            if (d.ssd)
              *d.metricF = calcLumaDiffYUY2_SSD(prev->GetReadPtr(), curr->GetReadPtr(),
                prev->GetRowSize(), prev->GetHeight(), prev->GetPitch(), curr->GetPitch(), d.nt, d.opt, env);
            else
              *d.metricF = calcLumaDiffYUY2_SAD(prev->GetReadPtr(), curr->GetReadPtr(),
                prev->GetRowSize(), prev->GetHeight(), prev->GetPitch(), curr->GetPitch(), d.nt, d.opt, env);
          }
        }
      }
    }
  }
}

void FrameDiff::fillBox(PVideoFrame &dst, int blockN, int xblocks, bool dot)
{
  if (vi.IsPlanar()) return fillBoxPlanar(dst, blockN, xblocks, dot);
  else return fillBoxYUY2(dst, blockN, xblocks, dot);
}

void FrameDiff::fillBoxYUY2(PVideoFrame &dst, int blockN, int xblocks, bool dot)
{
  unsigned char *dstp = dst->GetWritePtr();
  int pitch = dst->GetPitch();
  int width = dst->GetRowSize();
  int height = dst->GetHeight();
  int cordy, cordx, x, y, temp, xlim, ylim;
  cordy = blockN / xblocks;
  cordx = blockN - (cordy*xblocks);
  temp = cordx % 4;
  cordx = (cordx >> 2);
  cordy *= blocky;
  cordx *= (blockx << 1);
  if (temp == 1) cordx -= blockx;
  else if (temp == 2) cordy -= (blocky >> 1);
  else if (temp == 3) { cordx -= blockx; cordy -= (blocky >> 1); }
  xlim = cordx + 2 * blockx;
  ylim = cordy + blocky;
  int ymid = max(min(cordy + ((ylim - cordy) >> 1), height - 1), 0);
  int xmid = max(min(cordx + ((xlim - cordx) >> 1), width - 2), 0);
  if (xlim > width) xlim = width;
  if (ylim > height) ylim = height;
  cordy = max(cordy, 0);
  cordx = max(cordx, 0);
  dstp = dstp + cordy*pitch;
  for (y = cordy; y < ylim; ++y)
  {
    for (x = cordx; x < xlim; x += 4)
    {
      if (y == ymid && (x == xmid || x + 2 == xmid) && dot)
      {
        dstp[x] = 0;
        dstp[x + 2] = 0;
      }
      else if (!(dstp[x] == 0 && dstp[x + 2] == 0 &&
        (x <= 1 || dstp[x - 2] == 255) &&
        (x >= width - 4 || dstp[x + 4] == 255) &&
        (y == 0 || dstp[x - pitch] == 255) &&
        (y == height - 1 || dstp[x + pitch] == 255)) || !dot)
      {
        dstp[x] = 255;
        dstp[x + 2] = 255;
      }
    }
    dstp += pitch;
  }
}

void FrameDiff::fillBoxPlanar(PVideoFrame &dst, int blockN, int xblocks, bool dot)
{
  unsigned char *dstp = dst->GetWritePtr(PLANAR_Y);
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
  ylim = cordy + blocky;
  int ymid = max(min(cordy + ((ylim - cordy) >> 1), height - 1), 0);
  int xmid = max(min(cordx + ((xlim - cordx) >> 1), width - 1), 0);
  if (xlim > width) xlim = width;
  if (ylim > height) ylim = height;
  cordy = max(cordy, 0);
  cordx = max(cordx, 0);
  dstp = dstp + cordy*pitch;
  for (y = cordy; y < ylim; ++y)
  {
    for (x = cordx; x < xlim; ++x)
    {
      if (y == ymid && x == xmid && dot) dstp[x] = 0;
      else if (!(dstp[x] == 0 &&
        (x == width - 1 || dstp[x + 1] == 255) &&
        (x == 0 || dstp[x - 1] == 255) &&
        (y == height - 1 || dstp[x + pitch] == 255) &&
        (y == 0 || dstp[x - pitch] == 255)) || !dot) dstp[x] = 255;
    }
    dstp += pitch;
  }
}

void FrameDiff::drawBox(PVideoFrame &dst, int blockN, int xblocks, int np)
{
  if (np == 3) drawBoxYV12(dst, blockN, xblocks);
  else drawBoxYUY2(dst, blockN, xblocks);
}

void FrameDiff::Draw(PVideoFrame &dst, int x1, int y1, const char *s, int np)
{
  if (np == 3) DrawYV12(dst, x1, y1, s);
  else DrawYUY2(dst, x1, y1, s);
}

void FrameDiff::drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks)
{
  unsigned char *dstp = dst->GetWritePtr();
  int pitch = dst->GetPitch();
  int width = dst->GetRowSize();
  int height = dst->GetHeight();
  int cordy, cordx, x, y, temp, xlim, ylim;
  cordy = blockN / xblocks;
  cordx = blockN - (cordy*xblocks);
  temp = cordx % 4;
  cordx = (cordx >> 2);
  cordy *= blocky;
  cordx *= (blockx << 1);
  if (temp == 1) cordx -= blockx;
  else if (temp == 2) cordy -= (blocky >> 1);
  else if (temp == 3) { cordx -= blockx; cordy -= (blocky >> 1); }
  xlim = cordx + 2 * blockx;
  if (xlim > width) xlim = width;
  ylim = cordy + blocky;
  if (ylim > height) ylim = height;
  for (y = max(cordy, 0), temp = cordx + 2 * (blockx - 1); y < ylim; ++y)
  {
    if (cordx >= 0) (dstp + y*pitch)[cordx] = (dstp + y*pitch)[cordx] <= 128 ? 255 : 0;
    if (temp < width) (dstp + y*pitch)[temp] = (dstp + y*pitch)[temp] <= 128 ? 255 : 0;
  }
  for (x = max(cordx, 0), temp = cordy + blocky - 1; x < xlim; x += 4)
  {
    if (cordy >= 0)
    {
      (dstp + cordy*pitch)[x] = (dstp + cordy*pitch)[x] <= 128 ? 255 : 0;
      (dstp + cordy*pitch)[x + 1] = 128;
      (dstp + cordy*pitch)[x + 2] = (dstp + cordy*pitch)[x + 2] <= 128 ? 255 : 0;
      (dstp + cordy*pitch)[x + 3] = 128;
    }
    if (temp < height)
    {
      (dstp + temp*pitch)[x] = (dstp + temp*pitch)[x] <= 128 ? 255 : 0;
      (dstp + temp*pitch)[x + 1] = 128;
      (dstp + temp*pitch)[x + 2] = (dstp + temp*pitch)[x + 2] <= 128 ? 255 : 0;
      (dstp + temp*pitch)[x + 3] = 128;
    }
  }
}

void FrameDiff::DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s)
{
  int x, y = y1 * 20, num, pitch = dst->GetPitch();
  unsigned char *dp;
  unsigned int width = dst->GetRowSize();
  int height = dst->GetHeight();
  if (y + 20 >= height) return;
  for (int xx = 0; *s; ++s, ++xx)
  {
    x = (x1 + xx) * 10;
    if ((x + 10) * 2 >= (int)(width)) return;
    num = *s - ' ';
    for (int tx = 0; tx < 10; tx++)
    {
      for (int ty = 0; ty < 20; ty++)
      {
        dp = &dst->GetWritePtr()[(x + tx) * 2 + (y + ty) * pitch];
        if (font[num][ty] & (1 << (15 - tx)))
        {
          if (tx & 1)
          {
            dp[0] = 255;
            dp[-1] = 128;
            dp[1] = 128;
          }
          else
          {
            dp[0] = 255;
            dp[1] = 128;
            dp[3] = 128;
          }
        }
        else
        {
          if (tx & 1)
          {
            dp[0] = (unsigned char)(dp[0] >> 1);
            dp[-1] = (unsigned char)((dp[-1] + 128) >> 1);
            dp[1] = (unsigned char)((dp[1] + 128) >> 1);
          }
          else
          {
            dp[0] = (unsigned char)(dp[0] >> 1);
            dp[1] = (unsigned char)((dp[1] + 128) >> 1);
            dp[3] = (unsigned char)((dp[3] + 128) >> 1);
          }
        }
      }
    }
  }
}

void FrameDiff::drawBoxYV12(PVideoFrame &dst, int blockN, int xblocks)
{
  unsigned char *dstp = dst->GetWritePtr(PLANAR_Y);
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
  for (y = max(cordy, 0), temp = cordx + blockx - 1; y < ylim; ++y)
  {
    if (cordx >= 0) (dstp + y*pitch)[cordx] = (dstp + y*pitch)[cordx] <= 128 ? 255 : 0;
    if (temp < width) (dstp + y*pitch)[temp] = (dstp + y*pitch)[temp] <= 128 ? 255 : 0;
  }
  for (x = max(cordx, 0), temp = cordy + blocky - 1; x < xlim; ++x)
  {
    if (cordy >= 0) (dstp + cordy*pitch)[x] = (dstp + cordy*pitch)[x] <= 128 ? 255 : 0;
    if (temp < height) (dstp + temp*pitch)[x] = (dstp + temp*pitch)[x] <= 128 ? 255 : 0;
  }
}

// fixme: not only for YV12
void FrameDiff::DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s)
{
  int x, y = y1 * 20, num, tx, ty;
  int pitchY = dst->GetPitch(PLANAR_Y), pitchUV = dst->GetPitch(PLANAR_V);
  unsigned char *dpY, *dpU, *dpV;
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

AVSValue __cdecl Create_CFrameDiff(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  AVSValue cnt = env->GetVar("current_frame");
  if (!cnt.IsInt())
    env->ThrowError("CFrameDiff:  This filter can only be used within ConditionalFilter!");
  int n = cnt.AsInt();
  FrameDiff *f = new FrameDiff(args[0].AsClip(), args[1].AsInt(1), args[2].AsBool(true), args[3].AsInt(0),
    args[4].AsInt(32), args[5].AsInt(32), args[6].AsBool(false), 2.0, 0, args[7].AsBool(false),
    args[8].AsBool(true), args[9].AsBool(false), args[10].AsBool(false), args[11].AsBool(false),
    args[12].AsInt(4), env);
  AVSValue CFrameDiff = f->ConditionalFrameDiff(n, env);
  delete f;
  return CFrameDiff;
}

AVSValue __cdecl Create_FrameDiff(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  return new FrameDiff(args[0].AsClip(), args[1].AsInt(1), args[2].AsBool(true), args[3].AsInt(0),
    args[4].AsInt(32), args[5].AsInt(32), args[6].AsBool(false), args[7].AsFloat(2.0),
    args[8].AsInt(0), args[9].AsBool(false), args[10].AsBool(true), args[11].AsBool(false),
    args[12].AsBool(false), false, args[13].AsInt(4), env);
}
