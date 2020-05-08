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

PVideoFrame TDeinterlace::GetFrameYUY2(int n, IScriptEnvironment* env, bool &wdtd)
{
  int n_saved = n;
  if (mode < 0)
  {
    PVideoFrame src2up = child->GetFrame(n, env);
    PVideoFrame dst2up = env->NewVideoFrame(vi_saved);
    PVideoFrame msk2up = env->NewVideoFrame(vi_saved);
    copyForUpsize(dst2up, src2up, 1, env);
    setMaskForUpsize(msk2up, 1);
    if (mode == -2) smartELADeintYUY2(dst2up, msk2up, dst2up, dst2up, dst2up);
    else if (mode == -1) ELADeintYUY2(dst2up, msk2up, dst2up, dst2up, dst2up);
    return dst2up;
  }
  if (mode == 1)
  {
    if (autoFO) order = child->GetParity(n >> 1) ? 1 : 0;
    if (n & 1) field = order == 1 ? 0 : 1;
    else field = order;
    n >>= 1;
  }
  else if (autoFO)
  {
    order = child->GetParity(n) ? 1 : 0;
    if (fieldS == -1) field = order;
  }
  PVideoFrame prv2, prv, nxt, nxt2, dst, mask;
  PVideoFrame src = child->GetFrame(n, env);
  bool found = false, fieldOVR = false;
  int x, hintField = -1;
  passHint = 0xFFFFFFFF;
  if (input.size() > 0 && *ovr)
  {
    if (mode != 1)
    {
      if (fieldS != -1) field = fieldS;
      if (!autoFO) order = orderS;
    }
    mthreshL = mthreshLS;
    mthreshC = mthreshCS;
    type = typeS;
    for (x = 0; x < countOvr; x += 4)
    {
      if (n >= input[x + 1] && n <= input[x + 2])
      {
        if (input[x] == 45 && mode != 1) // -
        {
          if (debug)
          {
            sprintf(buf, "TDeint2y:  frame %d:  not deinterlacing\n", n);
            OutputDebugString(buf);
          }
          if (map > 0)
            return createMap(src, 0, env, 0);
          return src;
        }
        else if (input[x] == 43 && mode != 1) found = true;  // +
        else if (input[x] == 102 && mode != 1) { field = input[x + 3]; fieldOVR = true; }// f
        else if (input[x] == 111 && mode != 1) order = input[x + 3]; // o
        else if (input[x] == 108) mthreshL = input[x + 3]; // l
        else if (input[x] == 99) mthreshC = input[x + 3]; // c
        else if (input[x] == 116) type = input[x + 3]; // t
      }
    }
    if (!found && ovrDefault == 1 && mode != 1)
    {
      if (debug)
      {
        sprintf(buf, "TDeint2y:  frame %d:  not deinterlacing\n", n);
        OutputDebugString(buf);
      }
      if (map > 0)
        return createMap(src, 0, env, 0);
      return src;
    }
  }
  if (mode == 0 && hints && TDeinterlace::getHint(src, passHint, hintField) == 0 && !found)
  {
    if (debug)
    {
      sprintf(buf, "TDeint2y:  frame %d:  not deinterlacing (HINTS)\n", n);
      OutputDebugString(buf);
    }
    if (map > 0)
      return createMap(src, 0, env, 0);
    return src;
  }
  if (mode == 0 && !full && !found)
  {
    int MIC;
    if (!checkCombedYUY2(src, MIC, env))
    {
      if (debug)
      {
        sprintf(buf, "TDeint2y:  frame %d:  not deinterlacing (full = false, MIC = %d)\n", n, MIC);
        OutputDebugString(buf);
      }
      if (map > 0)
        return createMap(src, 0, env, 0);
      return src;
    }
    else if (debug)
    {
      sprintf(buf, "TDeint2y:  frame %d:  deinterlacing (full = false, MIC = %d)\n", n, MIC);
      OutputDebugString(buf);
    }
  }
  if (!fieldOVR && hintField >= 0)
  {
    int tempf = field;
    field = hintField;
    hintField = tempf;
  }
  if (!useClip2)
  {
    prv2 = child->GetFrame(n > 1 ? n - 2 : n > 0 ? n - 1 : 0, env);
    prv = child->GetFrame(n > 0 ? n - 1 : 0, env);
    nxt = child->GetFrame(n < nfrms ? n + 1 : nfrms, env);
    nxt2 = child->GetFrame(n < nfrms - 1 ? n + 2 : n < nfrms ? n + 1 : nfrms, env);
  }
  else
  {
    prv2 = clip2->GetFrame(n > 1 ? n - 2 : n > 0 ? n - 1 : 0, env);
    prv = clip2->GetFrame(n > 0 ? n - 1 : 0, env);
    src = clip2->GetFrame(n, env);
    nxt = clip2->GetFrame(n < nfrms ? n + 1 : nfrms, env);
    nxt2 = clip2->GetFrame(n < nfrms - 1 ? n + 2 : n < nfrms ? n + 1 : nfrms, env);
  }
  dst = env->NewVideoFrame(vi_saved);
  if (type == 2 || mtnmode > 1 || tryWeave)
  {
    subtractFields(prv, src, nxt, vi_saved, accumPn, accumNn, accumPm, accumNm,
      field, order, opt, false, slow, env);
    if (sa.size() > 0) 
      insertCompStats(n_saved, accumPn, accumNn, accumPm, accumNm);
    rmatch = getMatch(accumPn, accumNn, accumPm, accumNm);
    if (debug)
    {
      sprintf(buf, "TDeint2y:  frame %d:  accumPn = %u  accumNn = %u\n", n, accumPn, accumNn);
      OutputDebugString(buf);
      sprintf(buf, "TDeint2y:  frame %d:  accumPm = %u  accumNm = %u\n", n, accumPm, accumNm);
      OutputDebugString(buf);
    }
  }
  if (tryWeave && (mode != 0 || full || found || (field^order && rmatch == 1) ||
    (!(field^order) && rmatch == 0)))
  {
    createWeaveFrameYUY2(dst, prv, src, nxt, env);
    int MIC;
    if (!checkCombedYUY2(dst, MIC, env))
    {
      if (debug)
      {
        sprintf(buf, "TDeint2y:  frame %d:  weaved with %s (tryWeave, MIC = %d)\n", n,
          field^order ? (rmatch == 0 ? "CURR" : "NEXT") :
          (rmatch == 1 ? "CURR" : "PREV"), MIC);
        OutputDebugString(buf);
      }
      int tf = field;
      if (hintField >= 0 && !fieldOVR) field = hintField;
      if (map > 0)
        return createMap(dst, 1, env, tf);
      return dst;
    }
    else if (debug)
    {
      sprintf(buf, "TDeint2y:  frame %d:  not weaving (tryWeave, MIC = %d)\n", n, MIC);
      OutputDebugString(buf);
    }
  }
  wdtd = true;
  mask = env->NewVideoFrame(vi_saved);
  if (emask) mask = emask->GetFrame(n_saved, env);
  else
  {
    if (mthreshL <= 0 && mthreshC <= 0) setMaskForUpsize(mask, 1);
    else if (mtnmode >= 0 && mtnmode <= 3)
    {
      if (emtn)
      {
        PVideoFrame prv2e = emtn->GetFrame(n > 1 ? n - 2 : n > 0 ? n - 1 : 0, env);
        PVideoFrame prve = emtn->GetFrame(n > 0 ? n - 1 : 0, env);
        PVideoFrame srce = emtn->GetFrame(n, env);
        PVideoFrame nxte = emtn->GetFrame(n < nfrms ? n + 1 : nfrms, env);
        PVideoFrame nxt2e = emtn->GetFrame(n < nfrms - 1 ? n + 2 : n < nfrms ? n + 1 : nfrms, env);
        if (mtnmode == 0 || mtnmode == 2)
          createMotionMap4_PlanarOrYUY2(prv2e, prve, srce, nxte, nxt2e, mask, n, true /*yuy2*/, env);
        else
          createMotionMap5_PlanarOrYUY2(prv2e, prve, srce, nxte, nxt2e, mask, n, true /*yuy2*/, env);
      }
      else
      {
        if (mtnmode == 0 || mtnmode == 2)
          createMotionMap4_PlanarOrYUY2(prv2, prv, src, nxt, nxt2, mask, n, true /*yuy2*/, env);
        else
          createMotionMap5_PlanarOrYUY2(prv2, prv, src, nxt, nxt2, mask, n, true /* yuy2*/, env);
      }
    }
    else env->ThrowError("TDeint:  an unknown error occured!");
    if (denoise) denoiseYUY2(mask);
    if (expand > 0) expandMap_YUY2(mask);
    if (link == 1) linkFULL_YUY2(mask);
    else if (link == 2) linkYtoUV_YUY2(mask);
    else if (link == 3) linkUVtoY_YUY2(mask);
    else if (link != 0) env->ThrowError("TDeint:  an unknown error occured (link)!");
  }
  PVideoFrame efrm = NULL;
  if (edeint) efrm = edeint->GetFrame(n_saved, env);
  PVideoFrame dmap = map ? env->NewVideoFrame(vi_saved) : NULL;
  if (map == 1 || map == 3) mapColorsYUY2(dmap, mask);
  else if (map == 2 || map == 4) mapMergeYUY2(dmap, mask, prv, src, nxt);
  const bool uap = (AP >= 0 && AP < 255) ? true : false;
  if (map == 0 || uap || map > 2)
  {
    if (edeint) eDeintYUY2(dst, mask, prv, src, nxt, efrm);
    else if (type == 0) cubicDeintYUY2(dst, mask, prv, src, nxt);
    else if (type == 1) smartELADeintYUY2(dst, mask, prv, src, nxt);
    else if (type == 2) kernelDeintYUY2(dst, mask, prv, src, nxt);
    else if (type == 3) ELADeintYUY2(dst, mask, prv, src, nxt);
    else if (type == 4) blendDeint(dst, mask, prv, src, nxt, env);
    else if (type == 5) blendDeint2(dst, mask, prv, src, nxt, env);
    else env->ThrowError("TDeint:  an unknown error occured!");
  }
  else
  {
    if (hintField >= 0 && !fieldOVR) field = hintField;
    return dmap;
  }
  if (uap)
  {
    apPostCheck(dst, mask, efrm, env);
    if (map) updateMapAP(dmap, mask, env);
    if (map > 0 && map < 3)
    {
      if (hintField >= 0 && !fieldOVR) field = hintField;
      return dmap;
    }
  }
  if (map != 1 && map != 2)
    TDeinterlace::putHint(dst, passHint, field);
  if (debug)
  {
    sprintf(buf, "TDeint2y:  frame %d:  field = %s (%d)  order = %s (%d)\n", n,
      field == 1 ? "interp bottom" : "interp top", field, order == 1 ? "tff" : "bff", order);
    OutputDebugString(buf);
    sprintf(buf, "TDeint2y:  frame %d:  mthreshL = %d  mthreshC = %d  type = %d\n", n,
      mthreshL, mthreshC, type);
    OutputDebugString(buf);
  }
  if (hintField >= 0 && !fieldOVR) field = hintField;
  if (map > 2)
  {
    PVideoFrame dst2 = env->NewVideoFrame(vi);
    stackVertical(dst2, dst, dmap, env);
    return dst2;
  }
  return dst;
}


void TDeinterlace::expandMap_YUY2(PVideoFrame &mask)
{
  unsigned char *maskp = mask->GetWritePtr();
  const int mask_pitch = mask->GetPitch();
  const int mask_pitch2 = mask_pitch << 1;
  const int Height = mask->GetHeight();
  const int Width = mask->GetRowSize();
  const int dis = expand * 2;
  maskp += mask_pitch*field;
  for (int y = field; y < Height; y += 2)
  {
    int ncl = 0, ncu = 0, ncv = 0;
    for (int x = 0; x < Width; ++x)
    {
      if (maskp[x] == 0x3C && x >= ncl)
      {
        int xt = x - 2;
        while (xt >= 0 && xt >= x - dis)
        {
          maskp[xt] = 0x3C;
          xt -= 2;
        }
        xt = x + 2;
        ncl = x + dis + 2;
        while (xt < Width && xt <= x + dis)
        {
          if (maskp[xt] == 0x3C)
          {
            ncl = xt;
            break;
          }
          else maskp[xt] = 0x3C;
          xt += 2;
        }
      }
      ++x;
      if (maskp[x] == 0x3C)
      {
        int &ncc = (x & 2) ? ncu : ncv;
        if (x >= ncc)
        {
          int xt = x - 4;
          while (xt >= 0 && xt >= x - dis)
          {
            maskp[xt] = 0x3C;
            xt -= 4;
          }
          xt = x + 4;
          ncc = x + dis + 4;
          while (xt < Width && xt <= x + dis)
          {
            if (maskp[xt] == 0x3C)
            {
              ncc = xt;
              break;
            }
            else maskp[xt] = 0x3C;
            xt += 4;
          }
        }
      }
    }
    maskp += mask_pitch2;
  }
}

void TDeinterlace::linkFULL_YUY2(PVideoFrame &mask)
{
  unsigned char *maskp = mask->GetWritePtr();
  const int mask_pitch = mask->GetPitch();
  const int mask_pitch2 = mask_pitch << 1;
  const int Height = mask->GetHeight();
  const int Width = mask->GetRowSize();
  maskp += mask_pitch*field;
  for (int y = field; y < Height; y += 2)
  {
    for (int x = 0; x < Width; x += 4)
    {
      if ((maskp[x] == 0x3C && maskp[x + 2] == 0x3C) ||
        maskp[x + 1] == 0x3C || maskp[x + 3] == 0x3C)
      {
        maskp[x] = maskp[x + 1] = maskp[x + 2] = maskp[x + 3] = 0x3C;
      }
    }
    maskp += mask_pitch2;
  }
}

void TDeinterlace::linkYtoUV_YUY2(PVideoFrame &mask)
{
  unsigned char *maskp = mask->GetWritePtr();
  const int mask_pitch = mask->GetPitch();
  const int mask_pitch2 = mask_pitch << 1;
  const int Height = mask->GetHeight();
  const int Width = mask->GetRowSize();
  maskp += mask_pitch*field;
  for (int y = field; y < Height; y += 2)
  {
    for (int x = 0; x < Width; x += 4)
    {
      if (maskp[x] == 0x3C && maskp[x + 2] == 0x3C)
      {
        maskp[x + 1] = maskp[x + 3] = 0x3C;
      }
    }
    maskp += mask_pitch2;
  }
}

void TDeinterlace::linkUVtoY_YUY2(PVideoFrame &mask)
{
  unsigned char *maskp = mask->GetWritePtr();
  const int mask_pitch = mask->GetPitch();
  const int mask_pitch2 = mask_pitch << 1;
  const int Height = mask->GetHeight();
  const int Width = mask->GetRowSize();
  maskp += mask_pitch*field;
  for (int y = field; y < Height; y += 2)
  {
    for (int x = 0; x < Width; x += 4)
    {
      if (maskp[x + 1] == 0x3C || maskp[x + 3] == 0x3C)
      {
        maskp[x] = maskp[x + 2] = 0x3C;
      }
    }
    maskp += mask_pitch2;
  }
}

// not the same as in TFMPP. Here 0x3C instead of 0xFF
void TDeinterlace::denoiseYUY2(PVideoFrame &mask)
{
  unsigned char *maskp = mask->GetWritePtr();
  const int mask_pitch = mask->GetPitch();
  const int mask_pitch2 = mask_pitch << 1;
  const int Height = mask->GetHeight();
  const int Width = mask->GetRowSize();
  maskp += mask_pitch*(2 + field);
  unsigned char *maskpp = maskp - mask_pitch2;
  unsigned char *maskpn = maskp + mask_pitch2;
  for (int y = 2; y < Height - 2; y += 2)
  {
    for (int x = 4; x < Width - 4; ++x)
    {
      if (maskp[x] == 0x3C)
      {
        if (maskpp[x - 2] == 0x3C) goto chroma_check;
        if (maskpp[x] == 0x3C) goto chroma_check;
        if (maskpp[x + 2] == 0x3C) goto chroma_check;
        if (maskp[x - 2] == 0x3C) goto chroma_check;
        if (maskp[x + 2] == 0x3C) goto chroma_check;
        if (maskpn[x - 2] == 0x3C) goto chroma_check;
        if (maskpn[x] == 0x3C) goto chroma_check;
        if (maskpn[x + 2] == 0x3C) goto chroma_check;
        maskp[x] = (maskp[x - 2] == maskp[x + 2]) ? maskp[x - 2] :
          (maskpp[x] == maskpn[x]) ? maskpp[x] : maskp[x - 2];
      }
    chroma_check:
      ++x;
      if (maskp[x] == 0x3C)
      {
        if (maskpp[x - 4] == 0x3C) continue;
        if (maskpp[x] == 0x3C) continue;
        if (maskpp[x + 4] == 0x3C) continue;
        if (maskp[x - 4] == 0x3C) continue;
        if (maskp[x + 4] == 0x3C) continue;
        if (maskpn[x - 4] == 0x3C) continue;
        if (maskpn[x] == 0x3C) continue;
        if (maskpn[x + 4] == 0x3C) continue;
        maskp[x] = (maskp[x - 4] == maskp[x + 4]) ? maskp[x - 4] :
          (maskpp[x] == maskpn[x]) ? maskpp[x] : maskp[x - 4];
      }
    }
    maskpp += mask_pitch2;
    maskp += mask_pitch2;
    maskpn += mask_pitch2;
  }
}

bool TDeinterlace::checkCombedYUY2(PVideoFrame &src, int &MIC, IScriptEnvironment *env)
{
  long cpu = env->GetCPUFlags();
  if (opt == 0) cpu = 0;
  bool use_sse2 = (cpu&CPUF_SSE2) ? true : false;

  const unsigned char *srcp = src->GetReadPtr();
  const int src_pitch = src->GetPitch();
  const int Width = src->GetRowSize();
  const int Height = src->GetHeight();
  const unsigned char *srcpp = srcp - src_pitch;
  const unsigned char *srcppp = srcpp - src_pitch;
  const unsigned char *srcpn = srcp + src_pitch;
  const unsigned char *srcpnn = srcpn + src_pitch;
  PVideoFrame cmask = env->NewVideoFrame(vi_saved);
  unsigned char *cmkw = cmask->GetWritePtr();
  const int cmk_pitch = cmask->GetPitch();
  const int inc = chroma ? 1 : 2;
  const int xblocks = ((Width + blockx_half) >> blockx_shift) + 1;
  const int xblocks4 = xblocks << 2;
  //xblocksi = xblocks4;
  const int yblocks = ((Height + blocky_half) >> blocky_shift) + 1;
  const int arraysize = (xblocks*yblocks) << 2;
  if (cthresh < 0) { memset(cmkw, 255, Height*cmk_pitch); goto cjump; }
  memset(cmkw, 0, Height*cmk_pitch);
  if (metric == 0)
  {
    const int cthresh6 = cthresh * 6;
    __m128i cthreshb_m128i;
    __m128i cthresh6w_m128i;
    if (use_sse2)
    {
      unsigned int cthresht = min(max(255 - cthresh - 1, 0), 255);
      cthreshb_m128i = _mm_set1_epi8(cthresht);
      unsigned int cthresh6t = min(max(65535 - cthresh * 6 - 1, 0), 65535);
      cthresh6w_m128i = _mm_set1_epi16(cthresh6t);
    }
    // top 1
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
    // top 2nd
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
    // middle Height - 4
    if (use_sse2)
    {
      if (chroma)
      {
        check_combing_SSE2(srcp, cmkw, Width, Height - 4, src_pitch, src_pitch * 2, cmk_pitch, cthreshb_m128i, cthresh6w_m128i);
      }
      else
      {
        check_combing_SSE2_Luma(srcp, cmkw, Width, Height - 4, src_pitch, src_pitch * 2, cmk_pitch, cthreshb_m128i, cthresh6w_m128i);
      }
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
    // Bottom -2
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
    // Bottom - 1
    for (int x = 0; x < Width; x += inc)
    {
      const int sFirst = srcp[x] - srcpp[x];
      if (sFirst > cthresh || sFirst < -cthresh)
      {
        if (abs(srcppp[x] + (srcp[x] << 2) + srcppp[x] - (3 * (srcpp[x] + srcpp[x]))) > cthresh6)
          cmkw[x] = 0xFF;
      }
    }
    // end of metric == 0
  }
  else
  {
    // metric != 0
    const int cthreshsq = cthresh*cthresh;
    __m128i cthreshb_m128i = _mm_set1_epi32(cthreshsq);
    // top 1
    for (int x = 0; x < Width; x += inc)
    {
      if ((srcp[x] - srcpn[x])*(srcp[x] - srcpn[x]) > cthreshsq)
        cmkw[x] = 0xFF;
    }
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    cmkw += cmk_pitch;
    // Middle Height - 2
    if (use_sse2)
    {
      if (chroma)
      {
        check_combing_SSE2_M1(srcp, cmkw, Width, Height - 2, src_pitch, cmk_pitch, cthreshb_m128i);
      }
      else
      {
        check_combing_SSE2_Luma_M1(srcp, cmkw, Width, Height - 2, src_pitch, cmk_pitch, cthreshb_m128i);
      }
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
    // bottom 1
    for (int x = 0; x < Width; x += inc)
    {
      if ((srcp[x] - srcpp[x])*(srcp[x] - srcpp[x]) > cthreshsq)
        cmkw[x] = 0xFF;
    }
  }
cjump:
  if (chroma)
  {
    unsigned char *cmkp = cmask->GetWritePtr() + cmk_pitch;
    unsigned char *cmkpp = cmkp - cmk_pitch;
    unsigned char *cmkpn = cmkp + cmk_pitch;
    for (int y = 1; y < Height - 1; ++y)
    {
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
  const unsigned char *cmkp = cmask->GetReadPtr() + cmk_pitch;
  const unsigned char *cmkpp = cmkp - cmk_pitch;
  const unsigned char *cmkpn = cmkp + cmk_pitch;
  memset(cArray, 0, arraysize * sizeof(int));
  int Heighta = (Height >> (blocky_shift - 1)) << (blocky_shift - 1);
  if (Heighta == Height) Heighta = Height - blocky_half;
  const int Widtha = (Width >> (blockx_shift - 1)) << (blockx_shift - 1);
  const bool use_16x8_sse2_sum = (use_sse2 && blockx_half == 16 && blocky_half == 8) ? true : false;
  // top y block
  for (int y = 1; y < blocky_half; ++y)
  {
    const int temp1 = (y >> blocky_shift)*xblocks4;
    const int temp2 = ((y + blocky_half) >> blocky_shift)*xblocks4;
    for (int x = 0; x < Width; x += 2)
    {
      if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF)
      {
        const int box1 = (x >> blockx_shift) << 2;
        const int box2 = ((x + blockx_half) >> blockx_shift) << 2;
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
  // middle y blocks
  for (int y = blocky_half; y < Heighta; y += blocky_half)
  {
    const int temp1 = (y >> blocky_shift)*xblocks4;
    const int temp2 = ((y + blocky_half) >> blocky_shift)*xblocks4;
    if (use_16x8_sse2_sum)
    {
      // use_sse2_sum is checked agains 16x8 blocksize
      for (int x = 0; x < Widtha; x += blockx_half)
      {
        int sum = 0;
        compute_sum_16x8_sse2_luma(cmkpp + x, cmk_pitch, sum);
        if (sum)
        {
          const int box1 = (x >> blockx_shift) << 2;
          const int box2 = ((x + blockx_half) >> blockx_shift) << 2;
          cArray[temp1 + box1 + 0] += sum;
          cArray[temp1 + box2 + 1] += sum;
          cArray[temp2 + box1 + 2] += sum;
          cArray[temp2 + box2 + 3] += sum;
        }
      }
    }
    else
    {
      for (int x = 0; x < Widtha; x += blockx_half)
      {
        const unsigned char *cmkppT = cmkpp;
        const unsigned char *cmkpT = cmkp;
        const unsigned char *cmkpnT = cmkpn;
        int sum = 0;
        for (int u = 0; u < blocky_half; ++u)
        {
          for (int v = 0; v < blockx_half; v += 2) // luma: x2
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
          const int box1 = (x >> blockx_shift) << 2;
          const int box2 = ((x + blockx_half) >> blockx_shift) << 2;
          cArray[temp1 + box1 + 0] += sum;
          cArray[temp1 + box2 + 1] += sum;
          cArray[temp2 + box1 + 2] += sum;
          cArray[temp2 + box2 + 3] += sum;
        }
      }
    }
    // rest unaligned on the right
    for (int x = Widtha; x < Width; x += 2)
    {
      const unsigned char *cmkppT = cmkpp;
      const unsigned char *cmkpT = cmkp;
      const unsigned char *cmkpnT = cmkpn;
      int sum = 0;
      for (int u = 0; u < blocky_half; ++u)
      {
        if (cmkppT[x] == 0xFF && cmkpT[x] == 0xFF &&
          cmkpnT[x] == 0xFF) ++sum;
        cmkppT += cmk_pitch;
        cmkpT += cmk_pitch;
        cmkpnT += cmk_pitch;
      }
      if (sum)
      {
        const int box1 = (x >> blockx_shift) << 2;
        const int box2 = ((x + blockx_half) >> blockx_shift) << 2;
        cArray[temp1 + box1 + 0] += sum;
        cArray[temp1 + box2 + 1] += sum;
        cArray[temp2 + box1 + 2] += sum;
        cArray[temp2 + box2 + 3] += sum;
      }
    }
    cmkpp += cmk_pitch*blocky_half;
    cmkp += cmk_pitch*blocky_half;
    cmkpn += cmk_pitch*blocky_half;
  }
  // bottom non-whole y block
  for (int y = Heighta; y < Height - 1; ++y)
  {
    const int temp1 = (y >> blocky_shift)*xblocks4;
    const int temp2 = ((y + blocky_half) >> blocky_shift)*xblocks4;
    for (int x = 0; x < Width; x += 2)
    {
      if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF)
      {
        const int box1 = (x >> blockx_shift) << 2;
        const int box2 = ((x + blockx_half) >> blockx_shift) << 2;
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
  MIC = 0;
  for (int x = 0; x < arraysize; ++x)
  {
    if (cArray[x] > MIC)
      MIC = cArray[x];
  }
  if (MIC > MI) return true;
  return false;
}

void TDeinterlace::mapColorsYUY2(PVideoFrame &dst, PVideoFrame &mask)
{
  const unsigned char *maskp = mask->GetReadPtr();
  const int mask_pitch = mask->GetPitch();
  const int Height = mask->GetHeight();
  const int Width = mask->GetRowSize();
  unsigned char *dstp = dst->GetWritePtr();
  const int dst_pitch = dst->GetPitch();
  for (int y = 0; y < Height; ++y)
  {
    for (int x = 0; x < Width; ++x)
    {
      if (maskp[x] == 10) dstp[x] = 0;
      else if (maskp[x] == 20) dstp[x] = 51;
      else if (maskp[x] == 30) dstp[x] = 102;
      else if (maskp[x] == 40) dstp[x] = 153;
      else if (maskp[x] == 50) dstp[x] = 204;
      else if (maskp[x] == 60) dstp[x] = 255;
      else if (maskp[x] == 70) dstp[x] = 230;
    }
    maskp += mask_pitch;
    dstp += dst_pitch;
  }
}

void TDeinterlace::mapMergeYUY2(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const unsigned char *maskp = mask->GetReadPtr();
  const int mask_pitch = mask->GetPitch();
  const int Height = mask->GetHeight();
  const int Width = mask->GetRowSize();
  unsigned char *dstp = dst->GetWritePtr();
  const int dst_pitch = dst->GetPitch();
  const unsigned char *prvp = prv->GetReadPtr();
  const int prv_pitch = prv->GetPitch();
  const unsigned char *srcp = src->GetReadPtr();
  const int src_pitch = src->GetPitch();
  const unsigned char *nxtp = nxt->GetReadPtr();
  const int nxt_pitch = nxt->GetPitch();
  for (int y = 0; y < Height; ++y)
  {
    for (int x = 0; x < Width; ++x)
    {
      if (maskp[x] == 10) dstp[x] = srcp[x];
      else if (maskp[x] == 20) dstp[x] = prvp[x];
      else if (maskp[x] == 30) dstp[x] = nxtp[x];
      else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
      else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
      else if (maskp[x] == 60) dstp[x] = 255;
      else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
    }
    prvp += prv_pitch;
    srcp += src_pitch;
    nxtp += nxt_pitch;
    maskp += mask_pitch;
    dstp += dst_pitch;
  }
}

void TDeinterlace::updateMapAP(PVideoFrame &dst, PVideoFrame &mask, IScriptEnvironment *env)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsPlanar() ? 3 : 1;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const int width = mask->GetRowSize(plane);
    const int height = mask->GetHeight(plane);
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        if (maskp[x] == 60)
          dstp[x] = 255;
      }
      maskp += mask_pitch;
      dstp += dst_pitch;
    }
  }
}

void TDeinterlace::eDeintYUY2(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &efrm)
{
  const unsigned char *prvp = prv->GetReadPtr();
  const int prv_pitch = prv->GetPitch();
  const unsigned char *srcp = src->GetReadPtr();
  const int src_pitch = src->GetPitch();
  const int Width = src->GetRowSize();
  const int Height = src->GetHeight();
  const unsigned char *nxtp = nxt->GetReadPtr();
  const int nxt_pitch = nxt->GetPitch();
  unsigned char *dstp = dst->GetWritePtr();
  const int dst_pitch = dst->GetPitch();
  const unsigned char *maskp = mask->GetReadPtr();
  const int mask_pitch = mask->GetPitch();
  const unsigned char *efrmp = efrm->GetReadPtr();
  const int efrm_pitch = efrm->GetPitch();
  for (int y = 0; y < Height; ++y)
  {
    for (int x = 0; x < Width; ++x)
    {
      if (maskp[x] == 10) dstp[x] = srcp[x];
      else if (maskp[x] == 20) dstp[x] = prvp[x];
      else if (maskp[x] == 30) dstp[x] = nxtp[x];
      else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
      else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
      else if (maskp[x] == 60) dstp[x] = efrmp[x];
      else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
    }
    prvp += prv_pitch;
    srcp += src_pitch;
    nxtp += nxt_pitch;
    maskp += mask_pitch;
    efrmp += efrm_pitch;
    dstp += dst_pitch;
  }
}

void TDeinterlace::blendDeint(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
  IScriptEnvironment *env)
{
  PVideoFrame tf = env->NewVideoFrame(vi_saved);
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsPlanar() ? 3 : 1;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const unsigned char *prvp = prv->GetReadPtr(plane);
    const int prv_pitch = prv->GetPitch(plane);
    const unsigned char *srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int Width = src->GetRowSize(plane);
    const int Height = src->GetHeight(plane);
    const unsigned char *nxtp = nxt->GetReadPtr(plane);
    const int nxt_pitch = nxt->GetPitch(plane);
    unsigned char *dstp = tf->GetWritePtr(plane);
    const int dst_pitch = tf->GetPitch(plane);
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    for (int y = 0; y < Height; ++y)
    {
      for (int x = 0; x < Width; ++x)
      {
        if (maskp[x] == 10) dstp[x] = srcp[x];
        else if (maskp[x] == 20) dstp[x] = prvp[x];
        else if (maskp[x] == 30) dstp[x] = nxtp[x];
        else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
        else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
        else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
        else if (maskp[x] == 60) dstp[x] = srcp[x];
      }
      prvp += prv_pitch;
      srcp += src_pitch;
      nxtp += nxt_pitch;
      maskp += mask_pitch;
      dstp += dst_pitch;
    }
  }
  copyFrame(dst, tf, env);
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const unsigned char *srcp = tf->GetReadPtr(plane);
    const int src_pitch = tf->GetPitch(plane);
    const int Width = tf->GetRowSize(plane);
    const int Height = tf->GetHeight(plane);
    const unsigned char *srcpp = srcp - src_pitch;
    const unsigned char *srcpn = srcp + src_pitch;
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const unsigned char *maskpp = maskp - mask_pitch;
    const unsigned char *maskpn = maskp + mask_pitch;
    for (int y = 0; y < Height; ++y)
    {
      for (int x = 0; x < Width; ++x)
      {
        if (maskp[x] == 60 || (y != 0 && maskpp[x] == 60) ||
          (y != Height - 1 && maskpn[x] == 60))
        {
          if (y == 0) dstp[x] = (srcpn[x] + srcp[x] + 1) >> 1;
          else if (y == Height - 1) dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
          else dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
        }
      }
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      maskpp += mask_pitch;
      maskp += mask_pitch;
      maskpn += mask_pitch;
      dstp += dst_pitch;
    }
  }
}

// both YUY2 and planar
void TDeinterlace::blendDeint2(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
  IScriptEnvironment *env)
{
  PVideoFrame tf = env->NewVideoFrame(vi_saved);
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsYUY2() ? 1 : 3;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const unsigned char *prvp = prv->GetReadPtr(plane);
    const int prv_pitch = prv->GetPitch(plane);
    const unsigned char *srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int Width = src->GetRowSize(plane);
    const int Height = src->GetHeight(plane);
    const unsigned char *nxtp = nxt->GetReadPtr(plane);
    const int nxt_pitch = nxt->GetPitch(plane);
    unsigned char *dstp = tf->GetWritePtr(plane);
    const int dst_pitch = tf->GetPitch(plane);
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const unsigned char *kerp;
    int ker_pitch;
    if (field^order)
    {
      ker_pitch = nxt_pitch;
      kerp = nxtp;
    }
    else
    {
      ker_pitch = prv_pitch;
      kerp = prvp;
    }
    for (int y = 0; y < Height; ++y)
    {
      for (int x = 0; x < Width; ++x)
      {
        if (maskp[x] == 10) dstp[x] = srcp[x];
        else if (maskp[x] == 20) dstp[x] = prvp[x];
        else if (maskp[x] == 30) dstp[x] = nxtp[x];
        else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
        else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
        else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
        else if (maskp[x] == 60) dstp[x] = (srcp[x] + kerp[x] + 1) >> 1;
      }
      prvp += prv_pitch;
      srcp += src_pitch;
      kerp += ker_pitch;
      nxtp += nxt_pitch;
      maskp += mask_pitch;
      dstp += dst_pitch;
    }
  }
  copyFrame(dst, tf, env);
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const unsigned char *srcp = tf->GetReadPtr(plane);
    const int src_pitch = tf->GetPitch(plane);
    const int Width = tf->GetRowSize(plane);
    const int Height = tf->GetHeight(plane);
    const unsigned char *srcpp = srcp - src_pitch;
    const unsigned char *srcpn = srcp + src_pitch;
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const unsigned char *maskpp = maskp - mask_pitch;
    const unsigned char *maskpn = maskp + mask_pitch;
    for (int y = 0; y < Height; ++y)
    {
      for (int x = 0; x < Width; ++x)
      {
        if (maskp[x] == 60 || (y != 0 && maskpp[x] == 60) ||
          (y != Height - 1 && maskpn[x] == 60))
        {
          if (y == 0) dstp[x] = (srcpn[x] + srcp[x] + 1) >> 1;
          else if (y == Height - 1) dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
          else dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
        }
      }
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      maskpp += mask_pitch;
      maskp += mask_pitch;
      maskpn += mask_pitch;
      dstp += dst_pitch;
    }
  }
}

void TDeinterlace::cubicDeintYUY2(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const unsigned char *prvp = prv->GetReadPtr();
  const int prv_pitch = prv->GetPitch();
  const unsigned char *srcp = src->GetReadPtr();
  const int src_pitch = src->GetPitch();
  const int src_pitch2 = src_pitch << 1;
  const int Width = src->GetRowSize();
  const int Height = src->GetHeight();
  const unsigned char *nxtp = nxt->GetReadPtr();
  const int nxt_pitch = nxt->GetPitch();
  unsigned char *dstp = dst->GetWritePtr();
  const int dst_pitch = dst->GetPitch();
  const unsigned char *maskp = mask->GetReadPtr();
  const int mask_pitch = mask->GetPitch();
  const unsigned char *srcpp = srcp - src_pitch;
  const unsigned char *srcppp = srcpp - src_pitch2;
  const unsigned char *srcpn = srcp + src_pitch;
  const unsigned char *srcpnn = srcpn + src_pitch2;
  for (int y = 0; y < Height; ++y)
  {
    for (int x = 0; x < Width; ++x)
    {
      if (maskp[x] == 10) dstp[x] = srcp[x];
      else if (maskp[x] == 20) dstp[x] = prvp[x];
      else if (maskp[x] == 30) dstp[x] = nxtp[x];
      else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
      else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
      else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
      else if (maskp[x] == 60)
      {
        if (y == 0) dstp[x] = srcpn[x];
        else if (y == Height - 1) dstp[x] = srcpp[x];
        else if (y<3 || y>Height - 4) dstp[x] = (srcpn[x] + srcpp[x] + 1) >> 1;
        else dstp[x] = cubicInt(srcppp[x], srcpp[x], srcpn[x], srcpnn[x]);
      }
    }
    prvp += prv_pitch;
    srcppp += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    srcpnn += src_pitch;
    nxtp += nxt_pitch;
    maskp += mask_pitch;
    dstp += dst_pitch;
  }
}

void TDeinterlace::ELADeintYUY2(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const unsigned char *prvp = prv->GetReadPtr();
  const int prv_pitch = prv->GetPitch();
  const unsigned char *srcp = src->GetReadPtr();
  const int src_pitch = src->GetPitch();
  const int Width = src->GetRowSize();
  const int Height = src->GetHeight();
  const unsigned char *nxtp = nxt->GetReadPtr();
  const int nxt_pitch = nxt->GetPitch();
  unsigned char *dstp = dst->GetWritePtr();
  const int dst_pitch = dst->GetPitch();
  const unsigned char *maskp = mask->GetReadPtr();
  const int mask_pitch = mask->GetPitch();
  const unsigned char *srcpp = srcp - src_pitch;
  const unsigned char *srcpn = srcp + src_pitch;
  for (int y = 0; y < Height; ++y)
  {
    for (int x = 0; x < Width; ++x)
    {
      if (maskp[x] == 10) dstp[x] = srcp[x];
      else if (maskp[x] == 20) dstp[x] = prvp[x];
      else if (maskp[x] == 30) dstp[x] = nxtp[x];
      else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
      else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
      else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
      else if (maskp[x] == 60)
      {
        if (y == 0)
        {
          dstp[x] = srcpn[x];
          continue;
        }
        if (y == Height - 1)
        {
          dstp[x] = srcpp[x];
          continue;
        }
        int inc, stop, shft;
        if (x & 1)
        {
          if (x < 8 || x > Width - 9 || (abs(srcpp[x] - srcpn[x]) < 10 &&
            abs(srcpp[x - 8] - srcpp[x + 8]) < 10 && abs(srcpn[x - 8] - srcpn[x + 8]) < 10))
          {
            dstp[x] = (srcpp[x] + srcpn[x] + 1) >> 1;
            continue;
          }
          stop = min(x - 4, min(16, Width - 5 - x));
          inc = 4;
          shft = 3;
        }
        else
        {
          if (x < 4 || x > Width - 5 || (abs(srcpp[x] - srcpn[x]) < 10 &&
            abs(srcpp[x - 4] - srcpp[x + 4]) < 10 && abs(srcpn[x - 4] - srcpn[x + 4]) < 10))
          {
            dstp[x] = (srcpp[x] + srcpn[x] + 1) >> 1;
            continue;
          }
          stop = min(x - 2, min(16, Width - 3 - x));
          inc = shft = 2;
        }
        const int minf = min(srcpp[x], srcpn[x]) - 2;
        const int maxf = max(srcpp[x], srcpn[x]) + 2;
        int val = (srcpp[x] + srcpn[x] + 1) >> 1;
        int min = 450;
        for (int u = 0; u <= stop; u += inc)
        {
          {
            const int s1 = srcpp[x + (u >> shft)*inc] + srcpp[x + ((u + inc) >> shft)*inc];
            const int s2 = srcpn[x - (u >> shft)*inc] + srcpn[x - ((u + inc) >> shft)*inc];
            const int temp1 = abs(s1 - s2) + abs(srcpp[x - inc] - srcpn[x - inc - u]) +
              (abs(srcpp[x] - srcpn[x - u]) << 1) + abs(srcpp[x + inc] - srcpn[x + inc - u]) +
              abs(srcpn[x - inc] - srcpp[x - inc + u]) + (abs(srcpn[x] - srcpp[x + u]) << 1) +
              abs(srcpn[x + inc] - srcpp[x + inc + u]);
            const int temp2 = (s1 + s2 + 2) >> 2;
            if (temp1 < min && temp2 >= minf && temp2 <= maxf)
            {
              min = temp1;
              val = temp2;
            }
          }
          {
            const int s1 = srcpp[x - (u >> shft)*inc] + srcpp[x - ((u + inc) >> shft)*inc];
            const int s2 = srcpn[x + (u >> shft)*inc] + srcpn[x + ((u + inc) >> shft)*inc];
            const int temp1 = abs(s1 - s2) + abs(srcpp[x - inc] - srcpn[x - inc + u]) +
              (abs(srcpp[x] - srcpn[x + u]) << 1) + abs(srcpp[x + inc] - srcpn[x + inc + u]) +
              abs(srcpn[x - inc] - srcpp[x - inc - u]) + (abs(srcpn[x] - srcpp[x - u]) << 1) +
              abs(srcpn[x + inc] - srcpp[x + inc - u]);
            const int temp2 = (s1 + s2 + 2) >> 2;
            if (temp1 < min && temp2 >= minf && temp2 <= maxf)
            {
              min = temp1;
              val = temp2;
            }
          }
        }
        dstp[x] = val;
      }
    }
    prvp += prv_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    nxtp += nxt_pitch;
    maskp += mask_pitch;
    dstp += dst_pitch;
  }
}

void TDeinterlace::kernelDeintYUY2(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const unsigned char *prvp = prv->GetReadPtr();
  const int prv_pitch = prv->GetPitch();
  const int prv_pitch2 = prv_pitch << 1;
  const unsigned char *srcp = src->GetReadPtr();
  const int src_pitch = src->GetPitch();
  const int src_pitch2 = src_pitch << 1;
  const int Width = src->GetRowSize();
  const int Height = src->GetHeight();
  const unsigned char *nxtp = nxt->GetReadPtr();
  const int nxt_pitch = nxt->GetPitch();
  const int nxt_pitch2 = nxt_pitch << 1;
  unsigned char *dstp = dst->GetWritePtr();
  const int dst_pitch = dst->GetPitch();
  const unsigned char *maskp = mask->GetReadPtr();
  const int mask_pitch = mask->GetPitch();
  const unsigned char *srcpp = srcp - src_pitch;
  const unsigned char *srcppp = srcpp - src_pitch2;
  const unsigned char *srcpn = srcp + src_pitch;
  const unsigned char *srcpnn = srcpn + src_pitch2;
  const unsigned char *kerc, *kerp, *kerpp, *kern, *kernn;
  int ker_pitch;
  if (rmatch == 0)
  {
    if (field^order)
    {
      ker_pitch = src_pitch;
      kerpp = srcp - (src_pitch2 << 1);
      kerp = srcp - src_pitch2;
      kerc = srcp;
      kern = srcp + src_pitch2;
      kernn = srcp + (src_pitch2 << 1);
    }
    else
    {
      ker_pitch = prv_pitch;
      kerpp = prvp - (prv_pitch2 << 1);
      kerp = prvp - prv_pitch2;
      kerc = prvp;
      kern = prvp + prv_pitch2;
      kernn = prvp + (prv_pitch2 << 1);
    }
  }
  else
  {
    if (field^order)
    {
      ker_pitch = nxt_pitch;
      kerpp = nxtp - (nxt_pitch2 << 1);
      kerp = nxtp - nxt_pitch2;
      kerc = nxtp;
      kern = nxtp + nxt_pitch2;
      kernn = nxtp + (nxt_pitch2 << 1);
    }
    else
    {
      ker_pitch = src_pitch;
      kerpp = srcp - (src_pitch2 << 1);
      kerp = srcp - src_pitch2;
      kerc = srcp;
      kern = srcp + src_pitch2;
      kernn = srcp + (src_pitch2 << 1);
    }
  }
  for (int y = 0; y < Height; ++y)
  {
    for (int x = 0; x < Width; ++x)
    {
      if (maskp[x] == 10) dstp[x] = srcp[x];
      else if (maskp[x] == 20) dstp[x] = prvp[x];
      else if (maskp[x] == 30) dstp[x] = nxtp[x];
      else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
      else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
      else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
      else if (maskp[x] == 60)
      {
        if (sharp && y > 3 && y < Height - 4)
        {
          const int temp = (int)((0.526*(srcpp[x] + srcpn[x]) +
            0.170*(kerc[x]) -
            0.116*(kerp[x] + kern[x]) -
            0.026*(srcppp[x] + srcpnn[x]) +
            0.031*(kerpp[x] + kernn[x])) + 0.5);
          if (temp > 255) dstp[x] = 255;
          else if (temp < 0) dstp[x] = 0;
          else dstp[x] = temp;
        }
        else if (y > 1 && y < Height - 2)
        {
          const int temp = (((srcpp[x] + srcpn[x]) << 3) +
            (kerc[x] << 1) - (kerp[x] + kern[x]) + 8) >> 4;
          if (temp > 255) dstp[x] = 255;
          else if (temp < 0) dstp[x] = 0;
          else dstp[x] = temp;
        }
        else
        {
          if (y == 0) dstp[x] = srcpn[x];
          else if (y == Height - 1) dstp[x] = srcpp[x];
          else dstp[x] = (srcpn[x] + srcpp[x] + 1) >> 1;
        }
      }
    }
    prvp += prv_pitch;
    srcppp += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    srcpnn += src_pitch;
    kerpp += ker_pitch;
    kerp += ker_pitch;
    kerc += ker_pitch;
    kern += ker_pitch;
    kernn += ker_pitch;
    nxtp += nxt_pitch;
    maskp += mask_pitch;
    dstp += dst_pitch;
  }
}

void TDeinterlace::smartELADeintYUY2(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const unsigned char *prvp = prv->GetReadPtr();
  const int prv_pitch = prv->GetPitch();
  const unsigned char *srcp = src->GetReadPtr();
  const int src_pitch = src->GetPitch();
  const int src_pitch2 = src_pitch << 1;
  const int Width = src->GetRowSize();
  const int Height = src->GetHeight();
  const unsigned char *nxtp = nxt->GetReadPtr();
  const int nxt_pitch = nxt->GetPitch();
  unsigned char *dstp = dst->GetWritePtr();
  const int dst_pitch = dst->GetPitch();
  const unsigned char *maskp = mask->GetReadPtr();
  const int mask_pitch = mask->GetPitch();
  const unsigned char *srcpp = srcp - src_pitch;
  const unsigned char *srcppp = srcpp - src_pitch2;
  const unsigned char *srcpn = srcp + src_pitch;
  const unsigned char *srcpnn = srcpn + src_pitch2;
  for (int y = 0; y < Height; ++y)
  {
    for (int x = 0; x < Width; ++x)
    {
      if (maskp[x] == 10) dstp[x] = srcp[x];
      else if (maskp[x] == 20) dstp[x] = prvp[x];
      else if (maskp[x] == 30) dstp[x] = nxtp[x];
      else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
      else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
      else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
      else if (maskp[x] == 60)
      {
        if (y > 2 && y < Height - 3 && x>7 && x < Width - 8)
        {
          const int Iy1 = srcppp[x - 2] + srcppp[x] + srcppp[x] + srcppp[x + 2] - srcpn[x - 2] - srcpn[x] - srcpn[x] - srcpn[x + 2];
          const int Iy2 = srcpp[x - 2] + srcpp[x] + srcpp[x] + srcpp[x + 2] - srcpnn[x - 2] - srcpnn[x] - srcpnn[x] - srcpnn[x + 2];
          const int Ix1 = srcppp[x + 2] + srcpp[x + 2] + srcpp[x + 2] + srcpn[x + 2] - srcppp[x - 2] - srcpp[x - 2] - srcpp[x - 2] - srcpn[x - 2];
          const int Ix2 = srcpp[x + 2] + srcpn[x + 2] + srcpn[x + 2] + srcpnn[x + 2] - srcpp[x - 2] - srcpn[x - 2] - srcpn[x - 2] - srcpnn[x - 2];
          const int edgeS1 = Ix1*Ix1 + Iy1*Iy1;
          const int edgeS2 = Ix2*Ix2 + Iy2*Iy2;
          if (edgeS1 < 1600 && edgeS2 < 1600)
          {
            dstp[x] = (srcpp[x] + srcpn[x] + 1) >> 1;
            goto chromajump;
          }
          if (abs(srcpp[x] - srcpn[x]) < 10 && (edgeS1 < 1600 || edgeS2 < 1600))
          {
            dstp[x] = (srcpp[x] + srcpn[x] + 1) >> 1;
            goto chromajump;
          }
          const int sum = srcpp[x - 2] + srcpp[x] + srcpp[x + 2] + srcpn[x - 2] + srcpn[x] + srcpn[x + 2];
          const int sumsq = srcpp[x - 2] * srcpp[x - 2] + srcpp[x] * srcpp[x] + srcpp[x + 2] * srcpp[x + 2] +
            srcpn[x - 2] * srcpn[x - 2] + srcpn[x] * srcpn[x] + srcpn[x + 2] * srcpn[x + 2];
          if ((6 * sumsq - sum*sum) < 432)
          {
            dstp[x] = (srcpp[x] + srcpn[x] + 1) >> 1;
            goto chromajump;
          }
          double dir1;
          if (Ix1 == 0) dir1 = 3.1415926;
          else
          {
            dir1 = atan(Iy1 / (Ix1*2.0f)) + 1.5707963;
            if (Iy1 >= 0) { if (Ix1 < 0) dir1 += 3.1415927; }
            else { if (Ix1 >= 0) dir1 += 3.1415927; }
            if (dir1 >= 3.1415927) dir1 -= 3.1415927;
          }
          double dir2;
          if (Ix2 == 0) dir2 = 3.1415926;
          else
          {
            dir2 = atan(Iy2 / (Ix2*2.0f)) + 1.5707963;
            if (Iy2 >= 0) { if (Ix2 < 0) dir2 += 3.1415927; }
            else { if (Ix2 >= 0) dir2 += 3.1415927; }
            if (dir2 >= 3.1415927) dir2 -= 3.1415927;
          }
          double dir;
          if (fabs(dir1 - dir2) < 0.5)
          {
            if (edgeS1 >= 3600 && edgeS2 >= 3600) dir = (dir1 + dir2) * 0.5f;
            else dir = edgeS1 >= edgeS2 ? dir1 : dir2;
          }
          else
          {
            if (edgeS1 >= 5000 && edgeS2 >= 5000)
            {
              const int Iye = srcpp[x - 2] + srcpp[x] + srcpp[x] + srcpp[x + 2] - srcpn[x - 2] - srcpn[x] - srcpn[x] - srcpn[x + 2];
              if ((Iy1*Iye > 0) && (Iy2*Iye < 0)) dir = dir1;
              else if ((Iy1*Iye < 0) && (Iy2*Iye > 0)) dir = dir2;
              else
              {
                if (abs(Iye - Iy1) <= abs(Iye - Iy2)) dir = dir1;
                else dir = dir2;
              }
            }
            else dir = edgeS1 >= edgeS2 ? dir1 : dir2;
          }
          double dirF = 0.5f / tan(dir);
          int temp1, temp2, temp;
          if (dirF >= 0.0f)
          {
            if (dirF >= 0.5f)
            {
              if (dirF >= 1.0f)
              {
                if (dirF >= 1.5f)
                {
                  if (dirF >= 2.0f)
                  {
                    if (dirF <= 2.50f)
                    {
                      temp1 = srcpp[x + 8];
                      temp2 = srcpn[x - 8];
                      temp = (srcpp[x + 8] + srcpn[x - 8] + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcpn[x];
                      temp = cubicInt(srcppp[x], srcpp[x], srcpn[x], srcpnn[x]);
                    }
                  }
                  else
                  {
                    temp1 = (int)((dirF - 1.5f)*(srcpp[x + 8]) + (2.0f - dirF)*(srcpp[x + 6]) + 0.5f);
                    temp2 = (int)((dirF - 1.5f)*(srcpn[x - 8]) + (2.0f - dirF)*(srcpn[x - 6]) + 0.5f);
                    temp = (int)((dirF - 1.5f)*(srcpp[x + 8] + srcpn[x - 8]) + (2.0f - dirF)*(srcpp[x + 6] + srcpn[x - 6]) + 0.5f);
                  }
                }
                else
                {
                  temp1 = (int)((dirF - 1.0f)*(srcpp[x + 6]) + (1.5f - dirF)*(srcpp[x + 4]) + 0.5f);
                  temp2 = (int)((dirF - 1.0f)*(srcpn[x - 6]) + (1.5f - dirF)*(srcpn[x - 4]) + 0.5f);
                  temp = (int)((dirF - 1.0f)*(srcpp[x + 6] + srcpn[x - 6]) + (1.5f - dirF)*(srcpp[x + 4] + srcpn[x - 4]) + 0.5f);
                }
              }
              else
              {
                temp1 = (int)((dirF - 0.5f)*(srcpp[x + 4]) + (1.0f - dirF)*(srcpp[x + 2]) + 0.5f);
                temp2 = (int)((dirF - 0.5f)*(srcpn[x - 4]) + (1.0f - dirF)*(srcpn[x - 2]) + 0.5f);
                temp = (int)((dirF - 0.5f)*(srcpp[x + 4] + srcpn[x - 4]) + (1.0f - dirF)*(srcpp[x + 2] + srcpn[x - 2]) + 0.5f);
              }
            }
            else
            {
              temp1 = (int)(dirF*(srcpp[x + 2]) + (0.5f - dirF)*(srcpp[x]) + 0.5f);
              temp2 = (int)(dirF*(srcpn[x - 2]) + (0.5f - dirF)*(srcpn[x]) + 0.5f);
              temp = (int)(dirF*(srcpp[x + 2] + srcpn[x - 2]) + (0.5f - dirF)*(srcpp[x] + srcpn[x]) + 0.5f);
            }
          }
          else
          {
            if (dirF <= -0.5f)
            {
              if (dirF <= -1.0f)
              {
                if (dirF <= -1.5f)
                {
                  if (dirF <= -2.0f)
                  {
                    if (dirF >= -2.50f)
                    {
                      temp1 = srcpp[x - 8];
                      temp2 = srcpn[x + 8];
                      temp = (srcpp[x - 8] + srcpn[x + 8] + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcpn[x];
                      temp = cubicInt(srcppp[x], srcpp[x], srcpn[x], srcpnn[x]);
                    }
                  }
                  else
                  {
                    temp1 = (int)((-dirF - 1.5f)*(srcpp[x - 8]) + (2.0f + dirF)*(srcpp[x - 6]) + 0.5f);
                    temp2 = (int)((-dirF - 1.5f)*(srcpn[x + 8]) + (2.0f + dirF)*(srcpn[x + 6]) + 0.5f);
                    temp = (int)((-dirF - 1.5f)*(srcpp[x - 8] + srcpn[x + 8]) + (2.0f + dirF)*(srcpp[x - 6] + srcpn[x + 6]) + 0.5f);
                  }
                }
                else
                {
                  temp1 = (int)((-dirF - 1.0f)*(srcpp[x - 6]) + (1.5f + dirF)*(srcpp[x - 4]) + 0.5f);
                  temp2 = (int)((-dirF - 1.0f)*(srcpn[x + 6]) + (1.5f + dirF)*(srcpn[x + 4]) + 0.5f);
                  temp = (int)((-dirF - 1.0f)*(srcpp[x - 6] + srcpn[x + 6]) + (1.5f + dirF)*(srcpp[x - 4] + srcpn[x + 4]) + 0.5f);
                }
              }
              else
              {
                temp1 = (int)((-dirF - 0.5f)*(srcpp[x - 4]) + (1.0f + dirF)*(srcpp[x - 2]) + 0.5f);
                temp2 = (int)((-dirF - 0.5f)*(srcpn[x + 4]) + (1.0f + dirF)*(srcpn[x + 2]) + 0.5f);
                temp = (int)((-dirF - 0.5f)*(srcpp[x - 4] + srcpn[x + 4]) + (1.0f + dirF)*(srcpp[x - 2] + srcpn[x + 2]) + 0.5f);
              }
            }
            else
            {
              temp1 = (int)((-dirF)*(srcpp[x - 2]) + (0.5f + dirF)*(srcpp[x]) + 0.5f);
              temp2 = (int)((-dirF)*(srcpn[x + 2]) + (0.5f + dirF)*(srcpn[x]) + 0.5f);
              temp = (int)((-dirF)*(srcpp[x - 2] + srcpn[x + 2]) + (0.5f + dirF)*(srcpp[x] + srcpn[x]) + 0.5f);
            }
          }
          const int maxN = max(srcpp[x], srcpn[x]) + 25;
          const int minN = min(srcpp[x], srcpn[x]) - 25;
          if (abs(temp1 - temp2) > 20 || abs(srcpp[x] + srcpn[x] - temp - temp) > 60 || temp < minN || temp > maxN)
          {
            temp = cubicInt(srcppp[x], srcpp[x], srcpn[x], srcpnn[x]);
          }
          if (temp > 255) temp = 255;
          else if (temp < 0) temp = 0;
          dstp[x] = temp;
        }
        else
        {
          if (y == 0) dstp[x] = srcpn[x];
          else if (y == Height - 1) dstp[x] = srcpp[x];
          else if (y<3 || y>Height - 4) dstp[x] = (srcpn[x] + srcpp[x] + 1) >> 1;
          else dstp[x] = cubicInt(srcppp[x], srcpp[x], srcpn[x], srcpnn[x]);
        }
      }
    chromajump:
      ++x;
      if (maskp[x] == 10) dstp[x] = srcp[x];
      else if (maskp[x] == 20) dstp[x] = prvp[x];
      else if (maskp[x] == 30) dstp[x] = nxtp[x];
      else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
      else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
      else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
      else if (maskp[x] == 60)
      {
        if (y == 0) dstp[x] = srcpn[x];
        else if (y == Height - 1) dstp[x] = srcpp[x];
        else if (y<3 || y>Height - 4) dstp[x] = (srcpn[x] + srcpp[x] + 1) >> 1;
        else dstp[x] = cubicInt(srcppp[x], srcpp[x], srcpn[x], srcpnn[x]);
      }
    }
    prvp += prv_pitch;
    srcppp += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    srcpnn += src_pitch;
    nxtp += nxt_pitch;
    maskp += mask_pitch;
    dstp += dst_pitch;
  }
}

void TDeinterlace::apPostCheck(PVideoFrame &dst, PVideoFrame &mask, PVideoFrame &efrm,
  IScriptEnvironment *env)
{
  PVideoFrame maskt;
  if (APType > 0)
  {
    maskt = env->NewVideoFrame(vi_saved);
    copyFrame(maskt, mask, env);
  }
  int count = 0;
  const int stop = vi.IsPlanar() ? 3 : 1;
  const int AP6 = AP * 6;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < stop; ++b)
  {
    int plane = planes[b];
    const unsigned char *dstp = dst->GetReadPtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    const int dst_pitch2 = dst_pitch << 1;
    const int Width = dst->GetRowSize(plane);
    const int Height = dst->GetHeight(plane);
    dstp += (2 - field)*dst_pitch;
    const unsigned char *dstppp = dstp - dst_pitch2;
    const unsigned char *dstpp = dstp - dst_pitch;
    const unsigned char *dstpn = dstp + dst_pitch;
    const unsigned char *dstpnn = dstp + dst_pitch2;
    unsigned char *maskw = mask->GetWritePtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const int mask_pitch2 = mask_pitch << 1;
    const unsigned char *maskp = APType > 0 ? maskt->GetReadPtr(plane) : NULL;
    int maskp_pitch = APType > 0 ? maskt->GetPitch(plane) : 0;
    const int maskp_pitch2 = maskp_pitch << 1;
    maskw += (2 - field)*mask_pitch;
    int y = 2 - field;
    for (int x = 0; x < Width; ++x)
    {
      if (maskw[x] == 60) { maskw[x] = 10; continue; }
      maskw[x] = 10;
      const int sFirst = dstp[x] - dstpp[x];
      const int sSecond = dstp[x] - dstpn[x];
      if ((sFirst > AP && sSecond > AP) || (sFirst < -AP && sSecond < -AP))
      {
        if (abs(dstpnn[x] + (dstp[x] << 2) + dstpnn[x] - (3 * (dstpp[x] + dstpn[x]))) > AP6)
        {
          if (APType > 0)
          {
            const int inc = stop > 1 ? 1 : x & 1 ? 4 : 2;
            const int startx = x - (inc << 1) < 0 ? x - inc < 0 ? x : x - inc : x - (inc << 1);
            const int stopx = x + (inc << 1) > Width - 1 ? x + inc > Width - 1 ? x : x + inc : x + (inc << 1);
            const int starty = y - 4 < 0 ? y - 2 < 0 ? y : y - 2 : y - 4;
            const int stopy = y + 4 > Height - 1 ? y + 2 > Height - 1 ? y : y + 2 : y + 4;
            int neighbors = 0, moving = 0;
            const unsigned char *maskpT = maskp + starty*maskp_pitch;
            for (int u = starty; u <= stopy; u += 2)
            {
              for (int v = startx; v <= stopx; v += inc)
              {
                if (maskpT[v] == 60) ++moving;
                ++neighbors;
              }
              maskpT += maskp_pitch2;
            }
            if ((APType == 1 && (moving << 1) >= neighbors) ||
              (APType == 2 && (moving * 3) >= neighbors))
            {
              maskw[x] = 60;
              ++count;
            }
          }
          else
          {
            maskw[x] = 60;
            ++count;
          }
        }
      }
    }
    dstppp += dst_pitch2;
    dstpp += dst_pitch2;
    dstp += dst_pitch2;
    dstpn += dst_pitch2;
    dstpnn += dst_pitch2;
    maskw += mask_pitch2;
    y += 2;
    for (; y < Height - 3; y += 2)
    {
      const int starty = y - 4 < 0 ? y - 2 < 0 ? y : y - 2 : y - 4;
      const int stopy = y + 4 > Height - 1 ? y + 2 > Height - 1 ? y : y + 2 : y + 4;
      for (int x = 0; x < Width; ++x)
      {
        if (maskw[x] == 60) { maskw[x] = 10; continue; }
        maskw[x] = 10;
        const int sFirst = dstp[x] - dstpp[x];
        const int sSecond = dstp[x] - dstpn[x];
        if ((sFirst > AP && sSecond > AP) || (sFirst < -AP && sSecond < -AP))
        {
          if (abs(dstppp[x] + (dstp[x] << 2) + dstpnn[x] - (3 * (dstpp[x] + dstpn[x]))) > AP6)
          {
            if (APType > 0)
            {
              const int inc = stop > 1 ? 1 : x & 1 ? 4 : 2;
              const int startx = x - (inc << 1) < 0 ? x - inc < 0 ? x : x - inc : x - (inc << 1);
              const int stopx = x + (inc << 1) > Width - 1 ? x + inc > Width - 1 ? x : x + inc : x + (inc << 1);
              int neighbors = 0, moving = 0;
              const unsigned char *maskpT = maskp + starty*maskp_pitch;
              for (int u = starty; u <= stopy; u += 2)
              {
                for (int v = startx; v <= stopx; v += inc)
                {
                  if (maskpT[v] == 60) ++moving;
                  ++neighbors;
                }
                maskpT += maskp_pitch2;
              }
              if ((APType == 1 && (moving << 1) >= neighbors) ||
                (APType == 2 && (moving * 3) >= neighbors))
              {
                maskw[x] = 60;
                ++count;
              }
            }
            else
            {
              maskw[x] = 60;
              ++count;
            }
          }
        }
      }
      dstppp += dst_pitch2;
      dstpp += dst_pitch2;
      dstp += dst_pitch2;
      dstpn += dst_pitch2;
      dstpnn += dst_pitch2;
      maskw += mask_pitch2;
    }
    for (int x = 0; x < Width; ++x)
    {
      if (maskw[x] == 60) { maskw[x] = 10; continue; }
      maskw[x] = 10;
      const int sFirst = dstp[x] - dstpp[x];
      const int sSecond = dstp[x] - dstpn[x];
      if ((sFirst > AP && sSecond > AP) || (sFirst < -AP && sSecond < -AP))
      {
        if (abs(dstppp[x] + (dstp[x] << 2) + dstppp[x] - (3 * (dstpp[x] + dstpn[x]))) > AP6)
        {
          if (APType > 0)
          {
            const int inc = stop > 1 ? 1 : x & 1 ? 4 : 2;
            const int startx = x - (inc << 1) < 0 ? x - inc < 0 ? x : x - inc : x - (inc << 1);
            const int stopx = x + (inc << 1) > Width - 1 ? x + inc > Width - 1 ? x : x + inc : x + (inc << 1);
            const int starty = y - 4 < 0 ? y - 2 < 0 ? y : y - 2 : y - 4;
            const int stopy = y + 4 > Height - 1 ? y + 2 > Height - 1 ? y : y + 2 : y + 4;
            int neighbors = 0, moving = 0;
            const unsigned char *maskpT = maskp + starty*maskp_pitch;
            for (int u = starty; u <= stopy; u += 2)
            {
              for (int v = startx; v <= stopx; v += inc)
              {
                if (maskpT[v] == 60) ++moving;
                ++neighbors;
              }
              maskpT += maskp_pitch2;
            }
            if ((APType == 1 && (moving << 1) >= neighbors) ||
              (APType == 2 && (moving * 3) >= neighbors))
            {
              maskw[x] = 60;
              ++count;
            }
          }
          else
          {
            maskw[x] = 60;
            ++count;
          }
        }
      }
    }
  }
  if (count > 0)
  {
    if (vi.IsPlanar())
    {
      if (edeint) eDeintPlanar(dst, mask, dst, dst, dst, efrm);
      else if (type == 0) cubicDeintPlanar(dst, mask, dst, dst, dst);
      else if (type == 1) smartELADeintPlanar(dst, mask, dst, dst, dst);
      else if (type == 2) kernelDeintPlanar(dst, mask, dst, dst, dst);
      else if (type == 3) ELADeintPlanar(dst, mask, dst, dst, dst);
      else if (type == 4) blendDeint(dst, mask, dst, dst, dst, env);
      else if (type == 5) blendDeint2(dst, mask, dst, dst, dst, env);
    }
    else
    { // YUY2
      if (edeint) eDeintYUY2(dst, mask, dst, dst, dst, efrm);
      else if (type == 0) cubicDeintYUY2(dst, mask, dst, dst, dst);
      else if (type == 1) smartELADeintYUY2(dst, mask, dst, dst, dst);
      else if (type == 2) kernelDeintYUY2(dst, mask, dst, dst, dst);
      else if (type == 3) ELADeintYUY2(dst, mask, dst, dst, dst);
      else if (type == 4) blendDeint(dst, mask, dst, dst, dst, env);
      else if (type == 5) blendDeint2(dst, mask, dst, dst, dst, env);
    }
  }
}

void TDeinterlace::copyForUpsize(PVideoFrame &dst, PVideoFrame &src, int np, IScriptEnvironment *env)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    env->BitBlt(dst->GetWritePtr(plane), dst->GetPitch(plane) * 2, src->GetReadPtr(plane),
      src->GetPitch(plane), src->GetRowSize(plane), src->GetHeight(plane));
    env->BitBlt(dst->GetWritePtr(plane) + (dst->GetPitch(plane)*(dst->GetHeight(plane) - 1)),
      dst->GetPitch(plane), src->GetReadPtr(plane) + (src->GetPitch(plane)*(src->GetHeight(plane) - 1)),
      src->GetPitch(plane), src->GetRowSize(plane), 1);
  }
}

void TDeinterlace::copyFrame(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env)
{
  const int stop = vi.IsPlanar() ? 3 : 1;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    env->BitBlt(dst->GetWritePtr(plane), dst->GetPitch(plane), src->GetReadPtr(plane),
      src->GetPitch(plane), src->GetRowSize(plane + 8 /*fixme: aligned why?*/), src->GetHeight(plane));
  }
}

// for Planar (np=3) and YUY2 (np=1)
void TDeinterlace::setMaskForUpsize(PVideoFrame &msk, int np)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    unsigned char *maskwc = msk->GetWritePtr(plane);
    const int msk_pitch = msk->GetPitch(plane) << 1;
    const int height = msk->GetHeight(plane) >> 1;
    const int width = msk->GetRowSize(plane);
    unsigned char *maskwn = maskwc + (msk_pitch >> 1);
    if (field == 1)
    {
      for (int y = 0; y < height - 1; ++y)
      {
        memset(maskwc, 10, width);
        memset(maskwn, 60, width);
        maskwc += msk_pitch;
        maskwn += msk_pitch;
      }
      memset(maskwc, 10, width);
      memset(maskwn, 10, width);
    }
    else
    {
      memset(maskwc, 10, width);
      memset(maskwn, 10, width);
      for (int y = 0; y < height - 1; ++y)
      {
        maskwc += msk_pitch;
        maskwn += msk_pitch;
        memset(maskwc, 60, width);
        memset(maskwn, 10, width);
      }
    }
  }
}

void TDeinterlace::createWeaveFrameYUY2(PVideoFrame &dst, PVideoFrame &prv,
  PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env)
{
  if (field^order)
  {
    if (rmatch == 0)
    {
      env->BitBlt(dst->GetWritePtr(), dst->GetPitch(), src->GetReadPtr(),
        src->GetPitch(), src->GetRowSize(), src->GetHeight());
    }
    else
    {
      env->BitBlt(dst->GetWritePtr() + (1 - field)*dst->GetPitch(), dst->GetPitch() << 1,
        src->GetReadPtr() + (1 - field)*src->GetPitch(), src->GetPitch() << 1,
        src->GetRowSize(), src->GetHeight() >> 1);
      env->BitBlt(dst->GetWritePtr() + field*dst->GetPitch(), dst->GetPitch() << 1,
        nxt->GetReadPtr() + field*nxt->GetPitch(), nxt->GetPitch() << 1, nxt->GetRowSize(),
        nxt->GetHeight() >> 1);
    }
  }
  else
  {
    if (rmatch == 1)
    {
      env->BitBlt(dst->GetWritePtr(), dst->GetPitch(), src->GetReadPtr(),
        src->GetPitch(), src->GetRowSize(), src->GetHeight());
    }
    else
    {
      env->BitBlt(dst->GetWritePtr() + (1 - field)*dst->GetPitch(), dst->GetPitch() << 1,
        src->GetReadPtr() + (1 - field)*src->GetPitch(), src->GetPitch() << 1,
        src->GetRowSize(), src->GetHeight() >> 1);
      env->BitBlt(dst->GetWritePtr() + field*dst->GetPitch(), dst->GetPitch() << 1,
        prv->GetReadPtr() + field*prv->GetPitch(), prv->GetPitch() << 1, prv->GetRowSize(),
        prv->GetHeight() >> 1);
    }
  }
}

PVideoFrame TDeinterlace::createMap(PVideoFrame &src, int c, IScriptEnvironment *env, int tf)
{
  if (map == 2)
    return src;
  PVideoFrame dst = env->NewVideoFrame(vi);
  const int stop = vi.IsPlanar() ? 3 : 1;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  if (c == 0)
  {
    for (int b = 0; b < stop; ++b)
    {
      const int plane = planes[b];
      const int height = src->GetHeight(plane);
      const int dst_pitch = dst->GetPitch(plane);
      if (map > 2)
      {
        env->BitBlt(dst->GetWritePtr(plane), dst_pitch,
          src->GetReadPtr(plane), src->GetPitch(plane),
          src->GetRowSize(plane), height);
      }
      unsigned char *dstp = dst->GetWritePtr(plane);
      if (map > 2) dstp += dst_pitch*height;
      if (map & 1)
        memset(dstp, 0, dst_pitch*height);
      else
        env->BitBlt(dstp, dst_pitch,
          src->GetReadPtr(plane), src->GetPitch(plane),
          src->GetRowSize(plane), height);
    }
  }
  else
  {
    const int rmlut[3] = { 51, 0, 102 };
    int mt = (tf^order) ? (rmatch == 0 ? 1 : 2) : (rmatch == 1 ? 1 : 0);
    const int val1 = tf == 0 ? rmlut[mt] : 0;
    const int val2 = tf == 0 ? 0 : rmlut[mt];
    for (int b = 0; b < stop; ++b)
    {
      const int plane = planes[b];
      const int height = src->GetHeight(plane);
      const int dst_pitch = dst->GetPitch(plane);
      const int width = src->GetRowSize(plane);
      if (map > 2)
      {
        env->BitBlt(dst->GetWritePtr(plane), dst_pitch,
          src->GetReadPtr(plane), src->GetPitch(plane),
          width, height);
      }
      unsigned char *dstp = dst->GetWritePtr(plane);
      if (map > 2) dstp += dst_pitch*height;
      if (map & 1)
      {
        for (int i = 0; i < height; i += 2)
        {
          memset(dstp, val1, width);
          dstp += dst_pitch;
          memset(dstp, val2, width);
          dstp += dst_pitch;
        }
      }
      else
        env->BitBlt(dstp, dst_pitch,
          src->GetReadPtr(plane), src->GetPitch(plane),
          width, height); // no hbd, mask is 8 bits
    }
  }
  return dst;
}