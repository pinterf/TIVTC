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
#include "info.h"

AVSValue TFM::ConditionalIsCombedTIVTC(int n, IScriptEnvironment* env)
{
  if (n < 0) n = 0;
  else if (n > nfrms) n = nfrms;
  int xblocks;
  int mics[5] = { -20, -20, -20, -20, -20 };
  int blockN[5] = { -20, -20, -20, -20, -20 };
  PVideoFrame frame = child->GetFrame(n, env);
  return checkCombed(frame, n, vi, 1, blockN, xblocks, mics, false, chroma, cthresh);
}

AVSValue __cdecl Create_IsCombedTIVTC(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  AVSValue cnt = env->GetVar("current_frame");
  if (!cnt.IsInt())
    env->ThrowError("IsCombedTIVTC:  This filter can only be used within ConditionalFilter!");
  int n = cnt.AsInt();

  bool chroma = args[3].AsBool(false);
  VideoInfo vi = args[0].AsClip()->GetVideoInfo();
  if (vi.IsY()) chroma = false;

  TFM *f = new TFM(args[0].AsClip(), -1, -1, 1, 5, "", "", "", "", false, false, false, false,
    15, args[1].AsInt(9), args[2].AsInt(80), chroma, args[4].AsInt(16),
    args[5].AsInt(16), 0, 0, "", 0, 0, 12.0, 0, 0, "", false, args[6].AsInt(0), false, false, false,
    args[7].AsInt(4), env);
  AVSValue IsCombedTIVTC = f->ConditionalIsCombedTIVTC(n, env);
  delete f;
  return IsCombedTIVTC;
}

#ifdef VERSION
#undef VERSION
#endif

#define VERSION "v1.4"

class ShowCombedTIVTC : public GenericVideoFilter
{
private:
  bool has_at_least_v8;
  int cpuFlags;

  char buf[512];
  int cthresh, MI, blockx, blocky, display, opt, metric;
  bool debug, chroma, fill;
  int yhalf, xhalf, yshift, xshift, nfrms, *cArray;
  PlanarFrame *cmask;
  void fillCombedYUY2(PVideoFrame &src, int &MICount,
    int &b_over, int &c_over, IScriptEnvironment *env);

  void fillCombedPlanar(const VideoInfo &vi, PVideoFrame& src, int& MICount, int& b_over, int& c_over, IScriptEnvironment* env);

  template<typename pixel_t>
  void fillCombedPlanar_core(PVideoFrame &src, int &MICount, int &b_over, int &c_over, int bits_per_pixel, IScriptEnvironment *env);


public:
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) override;
  ShowCombedTIVTC(PClip _child, int _cthresh, bool _chroma, int _MI,
    int _blockx, int _blocky, int _metric, bool _debug, int _display, bool _fill,
    int _opt, IScriptEnvironment *env);
  ~ShowCombedTIVTC();
};

ShowCombedTIVTC::ShowCombedTIVTC(PClip _child, int _cthresh, bool _chroma, int _MI,
  int _blockx, int _blocky, int _metric, bool _debug, int _display, bool _fill,
  int _opt, IScriptEnvironment *env) : GenericVideoFilter(_child),
  cthresh(_cthresh), MI(_MI), blockx(_blockx), blocky(_blocky),
  display(_display), opt(_opt), metric(_metric),
  debug(_debug), chroma(_chroma), fill(_fill)
{
  cArray = NULL;
  cmask = NULL;

  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  cpuFlags = env->GetCPUFlags();
  if (opt == 0) cpuFlags = 0;

  if (vi.BitsPerComponent() > 16)
    env->ThrowError("ShowCombedTIVTC:  only 8-16 bit formats supported!");
  if (!vi.IsYUV())
    env->ThrowError("ShowCombedTIVTC:  only YUV input supported!");
  if (vi.height & 1)
    env->ThrowError("ShowCombedTIVTC:  height must be mod 2!");
  if (display < 0 || display > 5)
    env->ThrowError("ShowCombedTIVTC:  display must be set to 0, 1, 2, 3, 4, or 5!");
  if (blockx != 4 && blockx != 8 && blockx != 16 && blockx != 32 && blockx != 64 &&
    blockx != 128 && blockx != 256 && blockx != 512 && blockx != 1024 && blockx != 2048)
    env->ThrowError("ShowCombedTIVTC:  illegal blockx size!");
  if (blocky != 4 && blocky != 8 && blocky != 16 && blocky != 32 && blocky != 64 &&
    blocky != 128 && blocky != 256 && blocky != 512 && blocky != 1024 && blocky != 2048)
    env->ThrowError("ShowCombedTIVTC:  illegal blocky size!");
  if (MI < 0 || MI > blockx*blocky)
    env->ThrowError("ShowCombedTIVTC:  MI must be >= 0 and <= blockx*blocky!");
  if (!debug && display == 5)
    env->ThrowError("ShowCombedTIVTC:  either debug or display output must be enabled!");
  if (opt < 0 || opt > 4)
    env->ThrowError("ShowCombedTIVTC:  opt must be set to either 0, 1, 2, 3, or 4!");
  if (metric != 0 && metric != 1)
    env->ThrowError("ShowCombedTIVTC:  metric must be set to 0 or 1!");
  xhalf = blockx >> 1;
  yhalf = blocky >> 1;
  xshift = blockx == 4 ? 2 : blockx == 8 ? 3 : blockx == 16 ? 4 : blockx == 32 ? 5 :
    blockx == 64 ? 6 : blockx == 128 ? 7 : blockx == 256 ? 8 : blockx == 512 ? 9 :
    blockx == 1024 ? 10 : 11;
  yshift = blocky == 4 ? 2 : blocky == 8 ? 3 : blocky == 16 ? 4 : blocky == 32 ? 5 :
    blocky == 64 ? 6 : blocky == 128 ? 7 : blocky == 256 ? 8 : blocky == 512 ? 9 :
    blocky == 1024 ? 10 : 11;
  cArray = (int *)_aligned_malloc((((vi.width + xhalf) >> xshift) + 1)*(((vi.height + yhalf) >> yshift) + 1) * 4 * sizeof(int), 16);
  if (!cArray)
    env->ThrowError("ShowCombedTIVTC:  malloc failure (cArray)!");

  cmask = new PlanarFrame(vi, true, cpuFlags);
  if (vi.IsYUY2())
  {
    xhalf *= 2;
    ++xshift;
  }
  nfrms = vi.num_frames - 1;
  child->SetCacheHints(CACHE_NOTHING, 0);
  if (debug)
  {
    sprintf(buf, "ShowCombedTIVTC:  %s by tritical\n", VERSION);
    OutputDebugString(buf);
    sprintf(buf, "ShowCombedTIVTC:  MI = %d  cthresh = %d  chroma = %c", MI,
      cthresh, chroma ? 'T' : 'F');
    OutputDebugString(buf);
  }
}

ShowCombedTIVTC::~ShowCombedTIVTC()
{
  if (cArray) _aligned_free(cArray);
  if (cmask) delete cmask;
}

PVideoFrame __stdcall ShowCombedTIVTC::GetFrame(int n, IScriptEnvironment *env)
{
  if (n < 0) n = 0;
  else if (n > nfrms) n = nfrms;
  PVideoFrame src = child->GetFrame(n, env);
  int MICount, b_over, c_over;
  if (vi.IsPlanar())
    fillCombedPlanar(vi, src, MICount, b_over, c_over, env);
  else
    fillCombedYUY2(src, MICount, b_over, c_over, env);

  if (debug)
  {
    sprintf(buf, "ShowCombedTIVTC:  frame %d -  MIC = %d  b_above = %d  c_above = %d", n,
      MICount, b_over, c_over);
    OutputDebugString(buf);
    if (MICount > MI)
    {
      sprintf(buf, "ShowCombedTIVTC:  frame %d -  COMBED FRAME!", n);
      OutputDebugString(buf);
    }
  }
  if (display != 5)
  {
    sprintf(buf, "ShowCombedTIVTC %s by tritical", VERSION);
    Draw(src, 0, 0, buf, vi);
    sprintf(buf, "MI = %d  cthresh = %d  chroma = %c", MI, cthresh, chroma ? 'T' : 'F');
    Draw(src, 0, 1, buf, vi);
    sprintf(buf, "MIC = %d  b_above = %d  c_above = %d", MICount, b_over, c_over);
    Draw(src, 0, 2, buf, vi);
    if (MICount > MI)
    {
      sprintf(buf, "COMBED FRAME!");
      Draw(src, 0, 3, buf, vi);
    }
  }
  return src;
}

void ShowCombedTIVTC::fillCombedYUY2(PVideoFrame &src, int &MICount,
  int &b_over, int &c_over, IScriptEnvironment *env)
{
  bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

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
      if (chroma) // YUY2 luma-chroma in one pass
        check_combing_SSE2(srcp, cmkw, Width, Height - 4, src_pitch, cmk_pitch, cthresh);
      else
        check_combing_YUY2LumaOnly_SSE2(srcp, cmkw, Width, Height - 4, src_pitch, cmk_pitch, cthresh);
      srcppp += src_pitch * (Height - 4);
      srcpp += src_pitch * (Height - 4);
      srcp += src_pitch * (Height - 4);
      srcpn += src_pitch * (Height - 4);
      srcpnn += src_pitch * (Height - 4);
      cmkw += cmk_pitch * (Height - 4);
    }
    else
    {
      for (int y = 2; y < Height - 2; ++y)
      {
        for (int x = 0; x < Width; x += inc) // 'inc' automatically handles luma+chroma/luma only
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
    // metric == 1

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
    // middle
    if (use_sse2)
    {
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
    for (int y = 1; y < Height - 1; ++y)
    {
      for (int x = 4; x < Width - 4; x += 4)
      {
        if (
          (cmkp[x + 1] == 0xFF && // U
            (cmkpp[x - 3] == 0xFF || cmkpp[x + 1] == 0xFF || cmkpp[x + 5] == 0xFF ||
              cmkp[x - 3] == 0xFF || cmkp[x + 5] == 0xFF ||
              cmkpn[x - 3] == 0xFF || cmkpn[x + 1] == 0xFF || cmkpn[x + 5] == 0xFF)
            )
          ||
          (cmkp[x + 3] == 0xFF && // V
            (cmkpp[x - 1] == 0xFF || cmkpp[x + 3] == 0xFF || cmkpp[x + 7] == 0xFF ||
              cmkp[x - 1] == 0xFF || cmkp[x + 7] == 0xFF ||
              cmkpn[x - 1] == 0xFF || cmkpn[x + 3] == 0xFF || cmkpn[x + 7] == 0xFF)
            )
          )
        {
          cmkp[x] = cmkp[x + 2] = 0xFF;
        }
      }
      cmkpp += cmk_pitch;
      cmkp += cmk_pitch;
      cmkpn += cmk_pitch;
    }
  }
  const uint8_t *cmkp = cmask->GetPtr() + cmk_pitch;
  const uint8_t *cmkpp = cmkp - cmk_pitch;
  const uint8_t *cmkpn = cmkp + cmk_pitch;
  memset(cArray, 0, arraysize * sizeof(int));
  env->MakeWritable(&src);
  uint8_t *dstp = src->GetWritePtr();
  const int dst_pitch = src->GetPitch();
  dstp += dst_pitch;
  c_over = 0;
  for (int y = 1; y < Height - 1; ++y)
  {
    const int temp1 = (y >> yshift)*xblocks4;
    const int temp2 = ((y + yhalf) >> yshift)*xblocks4;
    for (int x = 0; x < Width; x += 2)
    {
      if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF)
      {
        const int box1 = (x >> xshift) << 2;
        const int box2 = ((x + xhalf) >> xshift) << 2;
        ++cArray[temp1 + box1 + 0];
        ++cArray[temp1 + box2 + 1];
        ++cArray[temp2 + box1 + 2];
        ++cArray[temp2 + box2 + 3];
        ++c_over;
        if (display == 0 || (display > 2 && display != 5))
          dstp[x] = 0xFF;
      }
    }
    cmkpp += cmk_pitch;
    cmkp += cmk_pitch;
    cmkpn += cmk_pitch;
    dstp += dst_pitch;
  }

  MICount = -1;
  b_over = 0;
  int high_block = 0;
  for (int x = 0; x < arraysize; ++x)
  {
    if (cArray[x] > MICount)
    {
      MICount = cArray[x];
      high_block = x;
    }
    if (cArray[x] > MI)
    {
      ++b_over;
      if (display == 2 || display == 4)
      {
        if (fill) fillBox(src, blockx, blocky, x, xblocks4, false, vi);
        else drawBox(src, blockx, blocky, x, xblocks4, vi);
      }
    }
  }
  if (display == 1 || display == 3)
  {
    if (fill) fillBox(src, blockx, blocky, high_block, xblocks4, false, vi);
    else drawBox(src, blockx, blocky, high_block, xblocks4, vi);
  }
}


void ShowCombedTIVTC::fillCombedPlanar(const VideoInfo& vi, PVideoFrame& src, int& MICount,
  int& b_over, int& c_over, IScriptEnvironment* env)
{
  const int bits_per_pixel = vi.BitsPerComponent();
  if (vi.ComponentSize() == 1) {
    checkCombedPlanarAnalyze_core<uint8_t>(vi, cthresh, chroma, cpuFlags, metric, src, cmask);
    fillCombedPlanar_core<uint8_t>(src, MICount, b_over, c_over, bits_per_pixel, env);
  }
  else {
    checkCombedPlanarAnalyze_core<uint16_t>(vi, cthresh, chroma, cpuFlags, metric, src, cmask);
    fillCombedPlanar_core<uint16_t>(src, MICount, b_over, c_over, bits_per_pixel, env);
  }
}


template<typename pixel_t>
void ShowCombedTIVTC::fillCombedPlanar_core(PVideoFrame &src, int &MICount,
  int &b_over, int &c_over, int bits_per_pixel, IScriptEnvironment *env)
{

  const int cmk_pitch = cmask->GetPitch(0);
  const uint8_t *cmkp = cmask->GetPtr(0) + cmk_pitch;
  const uint8_t *cmkpp = cmkp - cmk_pitch;
  const uint8_t *cmkpn = cmkp + cmk_pitch;
  const int Width = cmask->GetWidth(0);
  const int Height = cmask->GetHeight(0);
  const int xblocks = ((Width + xhalf) >> xshift) + 1;
  const int xblocks4 = xblocks << 2;
  const int yblocks = ((Height + yhalf) >> yshift) + 1;
  const int arraysize = (xblocks*yblocks) << 2;
  memset(cArray, 0, arraysize * sizeof(int));

  env->MakeWritable(&src);

  // hbd OK
  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  pixel_t *dstp = reinterpret_cast<pixel_t *>(src->GetWritePtr(PLANAR_Y));
  const int dst_pitch = src->GetPitch(PLANAR_Y) / sizeof(pixel_t);
  dstp += dst_pitch;
  c_over = 0;
  for (int y = 1; y < Height - 1; ++y)
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
        ++c_over;
        if (display == 0 || (display > 2 && display != 5))
          dstp[x] = max_pixel_value;
      }
    }
    cmkpp += cmk_pitch;
    cmkp += cmk_pitch;
    cmkpn += cmk_pitch;
    dstp += dst_pitch;
  }

  MICount = -1;
  b_over = 0;
  int high_block = 0;
  for (int x = 0; x < arraysize; ++x)
  {
    if (cArray[x] > MICount)
    {
      MICount = cArray[x];
      high_block = x;
    }
    if (cArray[x] > MI)
    {
      ++b_over;
      if (display == 2 || display == 4)
      {
        if (fill) fillBox(src, blockx, blocky, x, xblocks4, false, vi);
        else drawBox(src, blockx, blocky, x, xblocks4, vi);
      }
    }
  }
  if (display == 1 || display == 3)
  {
    if (fill) fillBox(src, blockx, blocky, high_block, xblocks4, false, vi);
    else drawBox(src, blockx, blocky, high_block, xblocks4, vi);
  }
}


AVSValue __cdecl Create_ShowCombedTIVTC(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  bool chroma = args[2].AsBool(false);
  VideoInfo vi = args[0].AsClip()->GetVideoInfo();
  if (vi.IsY()) chroma = false;

  return new ShowCombedTIVTC(args[0].AsClip(), args[1].AsInt(9), chroma,
    args[3].AsInt(80), args[4].AsInt(16), args[5].AsInt(16), args[6].AsInt(0),
    args[7].AsBool(false), args[8].AsInt(3), args[9].AsBool(false), args[10].AsInt(4), env);
}

// These are just copied from TFMASM.cpp.  One day I'll make it
// so I don't have duplicate code everywhere in this pos...
// PF 20170419: done :)
