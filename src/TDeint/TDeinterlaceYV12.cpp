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

#include "TDeinterlace.h"
#include "TCommonASM.h"
#include <cassert>

PVideoFrame TDeinterlace::GetFramePlanar(int n, IScriptEnvironment* env, bool &wdtd)
{
  int n_saved = n;
  if (mode < 0)
  {
    PVideoFrame src2up = child->GetFrame(n, env);
    PVideoFrame dst2up = // frame property support
      has_at_least_v8 ? env->NewVideoFrameP(vi_saved, &src2up) : env->NewVideoFrame(vi_saved);
    PVideoFrame msk2up = env->NewVideoFrame(vi_saved);
    copyForUpsize(dst2up, src2up, 3, env);
    setMaskForUpsize(msk2up, 3);
    if (mode == -2) smartELADeintPlanar(dst2up, msk2up, dst2up, dst2up, dst2up);
    else if (mode == -1) ELADeintPlanar(dst2up, msk2up, dst2up, dst2up, dst2up);
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
            sprintf(buf, "TDeint:  frame %d:  not deinterlacing\n", n);
            OutputDebugString(buf);
          }
          if (map > 0)
            return createMap(src, 0, env, 0);
          return src;
        }
        else if (input[x] == 43 && mode != 1) found = true;  // +
        else if (input[x] == 102 && mode != 1) { field = input[x + 3]; fieldOVR = true; } // f
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
        sprintf(buf, "TDeint2:  frame %d:  not deinterlacing\n", n);
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
      sprintf(buf, "TDeint2:  frame %d:  not deinterlacing (HINTS)\n", n);
      OutputDebugString(buf);
    }
    if (map > 0)
      return createMap(src, 0, env, 0);
    return src;
  }
  if (mode == 0 && !full && !found)
  {
    int MIC;
    if (!checkCombedPlanar(src, MIC, env))
    {
      if (debug)
      {
        sprintf(buf, "TDeint:  frame %d:  not deinterlacing (full = false, MIC = %d)\n", n, MIC);
        OutputDebugString(buf);
      }
      if (map > 0)
        return createMap(src, 0, env, 0);
      return src;
    }
    else if (debug)
    {
      sprintf(buf, "TDeint2:  frame %d:  deinterlacing (full = false, MIC = %d)\n", n, MIC);
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

  // property support
  if(has_at_least_v8)
    dst = env->NewVideoFrameP(vi_saved, &src);
  else
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
      sprintf(buf, "TDeint2:  frame %d:  accumPn = %u  accumNn = %u\n", n, accumPn, accumNn);
      OutputDebugString(buf);
      sprintf(buf, "TDeint2:  frame %d:  accumPm = %u  accumNm = %u\n", n, accumPm, accumNm);
      OutputDebugString(buf);
    }
  }
  if (tryWeave && (mode != 0 || full || found || (field^order && rmatch == 1) ||
    (!(field^order) && rmatch == 0)))
  {
    createWeaveFramePlanar(dst, prv, src, nxt, env);
    int MIC;
    if (!checkCombedPlanar(dst, MIC, env))
    {
      if (debug)
      {
        sprintf(buf, "TDeint2:  frame %d:  weaved with %s (tryWeave, MIC = %d)\n", n,
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
      sprintf(buf, "TDeint2:  frame %d:  not weaving (tryWeave, MIC = %d)\n", n, MIC);
      OutputDebugString(buf);
    }
  }
  wdtd = true;
  mask = env->NewVideoFrame(vi_saved);
  if (emask) mask = emask->GetFrame(n_saved, env);
  else
  {
    if (mthreshL <= 0 && mthreshC <= 0) setMaskForUpsize(mask, 3);
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
          createMotionMap4_PlanarOrYUY2(prv2e, prve, srce, nxte, nxt2e, mask, n, false/*planar*/, env);
        else
          createMotionMap5_PlanarOrYUY2(prv2e, prve, srce, nxte, nxt2e, mask, n, false/*planar*/, env);
      }
      else
      {
        if (mtnmode == 0 || mtnmode == 2)
          createMotionMap4_PlanarOrYUY2(prv2, prv, src, nxt, nxt2, mask, n, false/*planar*/, env);
        else
          createMotionMap5_PlanarOrYUY2(prv2, prv, src, nxt, nxt2, mask, n, false/*planar*/, env);
      }
    }
    else env->ThrowError("TDeint:  an unknown error occured!");
    if (denoise) denoisePlanar(mask);
    if (expand > 0) {
      if (vi_saved.Is420())
        expandMap_Planar<420>(mask);
      else if (vi_saved.Is422())
        expandMap_Planar<422>(mask);
      else if (vi_saved.Is444())
        expandMap_Planar<444>(mask);
      else
        env->ThrowError("TDeint: unsupported video format");
    }
    if (link == 1) {
      if (vi_saved.Is420())
        linkFULL_Planar<420>(mask);
      else if (vi_saved.Is422())
        linkFULL_Planar<422>(mask);
      else if (vi_saved.Is444())
        linkFULL_Planar<444>(mask);
      else
        env->ThrowError("TDeint: unsupported video format");
    }
    else if (link == 2) {
      if (vi_saved.Is420())
        linkYtoUV_Planar<420>(mask);
      else if (vi_saved.Is422())
        linkYtoUV_Planar<422>(mask);
      else if (vi_saved.Is444())
        linkYtoUV_Planar<444>(mask);
      else {
        env->ThrowError("TDeint: unsupported video format");
      }

    }
    else if (link == 3) {
      if(vi_saved.Is420())
        linkUVtoY_Planar<420>(mask);
      else if(vi_saved.Is422())
        linkUVtoY_Planar<422>(mask);
      else if (vi_saved.Is444())
        linkUVtoY_Planar<444>(mask);
      else {
        env->ThrowError("TDeint: unsupported video format");
      }
    }
    else if (link != 0) env->ThrowError("TDeint:  an unknown error occured (link)!");
  }
  PVideoFrame efrm = NULL;
  if (edeint) efrm = edeint->GetFrame(n_saved, env);
  PVideoFrame dmap = map ? env->NewVideoFrame(vi_saved) : NULL;
  if (map == 1 || map == 3) mapColorsPlanar(dmap, mask);
  else if (map == 2 || map == 4) mapMergePlanar(dmap, mask, prv, src, nxt);
  const bool uap = (AP >= 0 && AP < 255) ? true : false;
  if (map == 0 || uap || map > 2)
  {

    if (edeint) eDeintPlanar(dst, mask, prv, src, nxt, efrm);
    else if (type == 0) cubicDeintPlanar(dst, mask, prv, src, nxt);
    else if (type == 1) smartELADeintPlanar(dst, mask, prv, src, nxt);
    else if (type == 2) kernelDeintPlanar(dst, mask, prv, src, nxt);
    else if (type == 3) ELADeintPlanar(dst, mask, prv, src, nxt);
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
    sprintf(buf, "TDeint2:  frame %d:  field = %s (%d)  order = %s (%d)\n", n,
      field == 1 ? "interp bottom" : "interp top", field, order == 1 ? "tff" : "bff", order);
    OutputDebugString(buf);
    sprintf(buf, "TDeint2:  frame %d:  mthreshL = %d  mthreshC = %d  type = %d\n", n,
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

void TDeinterlace::createMotionMap4_PlanarOrYUY2(PVideoFrame &prv2, PVideoFrame &prv,
  PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask,
  int n, bool isYUY2, IScriptEnvironment *env)
{
  db->resetCacheStart(n);
  InsertDiff(prv, src, n, db->GetPos(0), env);
  InsertDiff(src, nxt, n + 1, db->GetPos(1), env);
  if (mode == 0)
  {
    if (field^order) InsertDiff(nxt, nxt2, n + 2, db->GetPos(2), env);
    else InsertDiff(prv2, prv, n - 1, db->GetPos(2), env);
  }
  else
  {
    InsertDiff(nxt, nxt2, n + 2, db->GetPos(2), env);
    InsertDiff(prv2, prv, n - 1, db->GetPos(3), env);
  }
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < (isYUY2 ? 1 : 3); ++b)
  {
    const int plane = planes[b];
    const int dpitch = db->GetPitch(b) << 1;
    const int dpitchl = db->GetPitch(b);
    const int Height = db->GetHeight(b);
    const int Width = db->GetWidth(b);
    const unsigned char *d1p = db->GetReadPtr(db->GetPos(0), b) + dpitchl*field;
    const unsigned char *d2p = db->GetReadPtr(db->GetPos(1), b) + dpitchl*field;
    const unsigned char *d3p;
    if (mode == 0) d3p = db->GetReadPtr(db->GetPos(2), b) + dpitchl*field;
    else d3p = db->GetReadPtr(db->GetPos(field^order ? 2 : 3), b) + dpitchl*field;
    unsigned char *maskw = mask->GetWritePtr(plane);
    const int mask_pitch = mask->GetPitch(plane) << 1;
    memset(maskw, 10, (mask_pitch >> 1)*Height);
    maskw += (mask_pitch >> 1)*field;
    const unsigned char *d1pn = d1p + dpitchl;
    const unsigned char *d2pn = d2p + dpitchl;
    const unsigned char *d1pp = field ? d1p - dpitchl : d1pn;
    const unsigned char *d2pp = field ? d2p - dpitchl : d2pn;
    if (field^order)
    {
      const int val1 = mtnmode > 2 ? (rmatch == 0 ? 10 : 30) : 40;
      if (n <= 1 || n >= nfrms - 1)
      {
        for (int y = field; y < Height; y += 2)
        {
          for (int x = 0; x < Width; ++x)
          {
            const int t1 = n == 0 ? 0 : d1pp[x];
            const int t5 = n == 0 ? 0 : d1p[x];
            const int t2 = n == 0 ? 0 : d1pn[x];
            const int t3 = n == nfrms ? 0 : d2pp[x];
            const int t6 = n == nfrms ? 0 : d2p[x];
            const int t4 = n == nfrms ? 0 : d2pn[x];
            const int t7 = n >= nfrms - 1 ? 0 : d3p[x];
            if (t6 && ((t1 && t2) || (t3 && t4) || (((t2 && t4) || (t1 && t3)) && (t5 || t7))))
              maskw[x] = val1;
            else if (t1 && t5 && t2) maskw[x] = 10;
            else if (t3 && t7 && t4) maskw[x] = 30;
            else maskw[x] = 60;
          }
          if (y != 0)
          {
            d1pp += dpitch;
            d2pp += dpitch;
          }
          if (y != Height - 3)
          {
            d1pn += dpitch;
            d2pn += dpitch;
          }
          d1p += dpitch;
          d2p += dpitch;
          d3p += dpitch;
          maskw += mask_pitch;
        }
      }
      else
      {
        for (int y = field; y < Height; y += 2)
        {
          for (int x = 0; x < Width; ++x)
          {
            const int t1 = d1pp[x];
            const int t5 = d1p[x];
            const int t2 = d1pn[x];
            const int t3 = d2pp[x];
            const int t6 = d2p[x];
            const int t4 = d2pn[x];
            const int t7 = d3p[x];
            if (t6 && ((t1 && t2) || (t3 && t4) || (((t2 && t4) || (t1 && t3)) && (t5 || t7))))
              maskw[x] = val1;
            else if (t1 && t5 && t2) maskw[x] = 10;
            else if (t3 && t7 && t4) maskw[x] = 30;
            else maskw[x] = 60;
          }
          if (y != 0)
          {
            d1pp += dpitch;
            d2pp += dpitch;
          }
          if (y != Height - 3)
          {
            d1pn += dpitch;
            d2pn += dpitch;
          }
          d1p += dpitch;
          d2p += dpitch;
          d3p += dpitch;
          maskw += mask_pitch;
        }
      }
    }
    else
    {
      const int val1 = mtnmode > 2 ? (rmatch == 0 ? 20 : 10) : 50;
      if (n <= 1 || n >= nfrms - 1)
      {
        for (int y = field; y < Height; y += 2)
        {
          for (int x = 0; x < Width; ++x)
          {
            const int t1 = n == 0 ? 0 : d1pp[x];
            const int t6 = n == 0 ? 0 : d1p[x];
            const int t2 = n == 0 ? 0 : d1pn[x];
            const int t3 = n == nfrms ? 0 : d2pp[x];
            const int t7 = n == nfrms ? 0 : d2p[x];
            const int t4 = n == nfrms ? 0 : d2pn[x];
            const int t5 = n <= 1 ? 0 : d3p[x];
            if (t6 && ((t1 && t2) || (t3 && t4) || (((t2 && t4) || (t1 && t3)) && (t5 || t7))))
              maskw[x] = val1;
            else if (t1 && t5 && t2) maskw[x] = 20;
            else if (t3 && t7 && t4) maskw[x] = 10;
            else maskw[x] = 60;
          }
          if (y != 0)
          {
            d1pp += dpitch;
            d2pp += dpitch;
          }
          if (y != Height - 3)
          {
            d1pn += dpitch;
            d2pn += dpitch;
          }
          d1p += dpitch;
          d2p += dpitch;
          d3p += dpitch;
          maskw += mask_pitch;
        }
      }
      else
      {
        for (int y = field; y < Height; y += 2)
        {
          for (int x = 0; x < Width; ++x)
          {
            const int t1 = d1pp[x];
            const int t6 = d1p[x];
            const int t2 = d1pn[x];
            const int t3 = d2pp[x];
            const int t7 = d2p[x];
            const int t4 = d2pn[x];
            const int t5 = d3p[x];
            if (t6 && ((t1 && t2) || (t3 && t4) || (((t2 && t4) || (t1 && t3)) && (t5 || t7))))
              maskw[x] = val1;
            else if (t1 && t5 && t2) maskw[x] = 20;
            else if (t3 && t7 && t4) maskw[x] = 10;
            else maskw[x] = 60;
          }
          if (y != 0)
          {
            d1pp += dpitch;
            d2pp += dpitch;
          }
          if (y != Height - 3)
          {
            d1pn += dpitch;
            d2pn += dpitch;
          }
          d1p += dpitch;
          d2p += dpitch;
          d3p += dpitch;
          maskw += mask_pitch;
        }
      }
    }
  }
}

void TDeinterlace::createMotionMap5_PlanarOrYUY2(PVideoFrame &prv2, PVideoFrame &prv,
  PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask,
  int n, bool isYUY2, IScriptEnvironment *env)
{
  db->resetCacheStart(n - 1);
  InsertDiff(prv2, prv, n - 1, db->GetPos(0), env);
  InsertDiff(prv, src, n, db->GetPos(1), env);
  InsertDiff(src, nxt, n + 1, db->GetPos(2), env);
  InsertDiff(nxt, nxt2, n + 2, db->GetPos(3), env);
  InsertDiff(prv2, src, -n - 2, db->GetPos(4), env);
  InsertDiff(prv, nxt, -n - 3, db->GetPos(5), env);
  InsertDiff(src, nxt2, -n - 4, db->GetPos(6), env);
  int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const unsigned char *dpp[7], *dp[7], *dpn[7];
  for (int b = 0; b < (isYUY2 ? 1 : 3); ++b)
  {
    const int dpitch = db->GetPitch(b) << 1;
    const int dpitchl = db->GetPitch(b);
    const int Height = db->GetHeight(b);
    const int Width = db->GetWidth(b);
    for (int i = 0; i < 7; ++i)
    {
      dp[i] = db->GetReadPtr(db->GetPos(i), b) + dpitchl*field;
      dpn[i] = dp[i] + dpitchl;
      dpp[i] = field ? dp[i] - dpitchl : dpn[i];
    }
    unsigned char *maskw = mask->GetWritePtr(plane[b]);
    const int mask_pitch = mask->GetPitch(plane[b]) << 1;
    memset(maskw, 10, (mask_pitch >> 1)*Height);
    maskw += (mask_pitch >> 1)*field;
// not used    const int mthresh = b == 0 ? mthreshL : mthreshC;
    if (field^order)
    {
      const int val1 = mtnmode > 2 ? (rmatch == 0 ? 10 : 30) : 40;
      if (n <= 1 || n >= nfrms - 1)
      {
        for (int y = field; y < Height; y += 2)
        {
          for (int x = 0; x < Width; ++x)
          {
            const int t8 = n <= 1 ? 0 : dpp[0][x];
            const int t9 = n <= 1 ? 0 : dpn[0][x];
            const int t1 = n == 0 ? 0 : dpp[1][x];
            const int t5 = n == 0 ? 0 : dp[1][x];
            const int t2 = n == 0 ? 0 : dpn[1][x];
            const int t3 = n == nfrms ? 0 : dpp[2][x];
            const int t6 = n == nfrms ? 0 : dp[2][x];
            const int t4 = n == nfrms ? 0 : dpn[2][x];
            const int t10 = n >= nfrms - 1 ? 0 : dpp[3][x];
            const int t7 = n >= nfrms - 1 ? 0 : dp[3][x];
            const int t11 = n >= nfrms - 1 ? 0 : dpn[3][x];
            const int t12 = dpp[4][x];
            const int t13 = dpn[4][x];
            const int t14 = dpp[5][x];
            const int t18 = dp[5][x];
            const int t15 = dpn[5][x];
            const int t16 = dpp[6][x];
            const int t19 = dp[6][x];
            const int t17 = dpn[6][x];
            if (t6 && ((t1 && t2 && ((t3 && t4 && t14 && t15) || (t5 && t18))) ||
              (t3 && t4 && t7 && t19) ||
              (t5 && t18 && ((t1 && t3 && t14) || (t2 && t4 && t15) || (t1 && t8 && t12) || (t2 && t9 && t13))) ||
              (t7 && t19 && ((t1 && t3 && t14) || (t2 && t4 && t15) || (t3 && t10 && t16) || (t4 && t11 && t17)))))
              maskw[x] = val1;
            else if (t1 && t5 && t2 && t8 && t9 && t12 && t13) maskw[x] = 10;
            else if (t3 && t7 && t4 && t10 && t11 && t16 && t17) maskw[x] = 30;
            else maskw[x] = 60;
          }
          if (y != 0)
          {
            for (int i = 0; i < 7; ++i)
              dpp[i] += dpitch;
          }
          if (y != Height - 3)
          {
            for (int i = 0; i < 7; ++i)
              dpn[i] += dpitch;
          }
          for (int i = 0; i < 7; ++i)
            dp[i] += dpitch;
          maskw += mask_pitch;
        }
      }
      else
      {
        for (int y = field; y < Height; y += 2)
        {
          for (int x = 0; x < Width; ++x)
          {
            const int t8 = dpp[0][x];
            const int t9 = dpn[0][x];
            const int t1 = dpp[1][x];
            const int t5 = dp[1][x];
            const int t2 = dpn[1][x];
            const int t3 = dpp[2][x];
            const int t6 = dp[2][x];
            const int t4 = dpn[2][x];
            const int t10 = dpp[3][x];
            const int t7 = dp[3][x];
            const int t11 = dpn[3][x];
            const int t12 = dpp[4][x];
            const int t13 = dpn[4][x];
            const int t14 = dpp[5][x];
            const int t18 = dp[5][x];
            const int t15 = dpn[5][x];
            const int t16 = dpp[6][x];
            const int t19 = dp[6][x];
            const int t17 = dpn[6][x];
            if (t6 && ((t1 && t2 && ((t3 && t4 && t14 && t15) || (t5 && t18))) ||
              (t3 && t4 && t7 && t19) ||
              (t5 && t18 && ((t1 && t3 && t14) || (t2 && t4 && t15) || (t1 && t8 && t12) || (t2 && t9 && t13))) ||
              (t7 && t19 && ((t1 && t3 && t14) || (t2 && t4 && t15) || (t3 && t10 && t16) || (t4 && t11 && t17)))))
              maskw[x] = val1;
            else if (t1 && t5 && t2 && t8 && t9 && t12 && t13) maskw[x] = 10;
            else if (t3 && t7 && t4 && t10 && t11 && t16 && t17) maskw[x] = 30;
            else maskw[x] = 60;
          }
          if (y != 0)
          {
            for (int i = 0; i < 7; ++i)
              dpp[i] += dpitch;
          }
          if (y != Height - 3)
          {
            for (int i = 0; i < 7; ++i)
              dpn[i] += dpitch;
          }
          for (int i = 0; i < 7; ++i)
            dp[i] += dpitch;
          maskw += mask_pitch;
        }
      }
    }
    else
    {
      const int val1 = mtnmode > 2 ? (rmatch == 0 ? 20 : 10) : 50;
      if (n <= 1 || n >= nfrms - 1)
      {
        for (int y = field; y < Height; y += 2)
        {
          for (int x = 0; x < Width; ++x)
          {
            const int t8 = n <= 1 ? 0 : dpp[0][x];
            const int t5 = n <= 1 ? 0 : dp[0][x];
            const int t9 = n <= 1 ? 0 : dpn[0][x];
            const int t1 = n == 0 ? 0 : dpp[1][x];
            const int t6 = n == 0 ? 0 : dp[1][x];
            const int t2 = n == 0 ? 0 : dpn[1][x];
            const int t3 = n == nfrms ? 0 : dpp[2][x];
            const int t7 = n == nfrms ? 0 : dp[2][x];
            const int t4 = n == nfrms ? 0 : dpn[2][x];
            const int t10 = n >= nfrms - 1 ? 0 : dpp[3][x];
            const int t11 = n >= nfrms - 1 ? 0 : dpn[3][x];
            const int t12 = dpp[4][x];
            const int t18 = dp[4][x];
            const int t13 = dpn[4][x];
            const int t14 = dpp[5][x];
            const int t19 = dp[5][x];
            const int t15 = dpn[5][x];
            const int t16 = dpp[6][x];
            const int t17 = dpn[6][x];
            if (t6 && ((t1 && t2 && ((t3 && t4 && t14 && t15) || (t5 && t18))) ||
              (t3 && t4 && t7 && t19) ||
              (t5 && t18 && ((t1 && t3 && t14) || (t2 && t4 && t15) || (t1 && t8 && t12) || (t2 && t9 && t13))) ||
              (t7 && t19 && ((t1 && t3 && t14) || (t2 && t4 && t15) || (t3 && t10 && t16) || (t4 && t11 && t17)))))
              maskw[x] = val1;
            else if (t1 && t5 && t2 && t8 && t9 && t12 && t13) maskw[x] = 20;
            else if (t3 && t7 && t4 && t10 && t11 && t16 && t17) maskw[x] = 10;
            else maskw[x] = 60;
          }
          if (y != 0)
          {
            for (int i = 0; i < 7; ++i)
              dpp[i] += dpitch;
          }
          if (y != Height - 3)
          {
            for (int i = 0; i < 7; ++i)
              dpn[i] += dpitch;
          }
          for (int i = 0; i < 7; ++i)
            dp[i] += dpitch;
          maskw += mask_pitch;
        }
      }
      else
      {
        for (int y = field; y < Height; y += 2)
        {
          for (int x = 0; x < Width; ++x)
          {
            const int t8 = dpp[0][x];
            const int t5 = dp[0][x];
            const int t9 = dpn[0][x];
            const int t1 = dpp[1][x];
            const int t6 = dp[1][x];
            const int t2 = dpn[1][x];
            const int t3 = dpp[2][x];
            const int t7 = dp[2][x];
            const int t4 = dpn[2][x];
            const int t10 = dpp[3][x];
            const int t11 = dpn[3][x];
            const int t12 = dpp[4][x];
            const int t18 = dp[4][x];
            const int t13 = dpn[4][x];
            const int t14 = dpp[5][x];
            const int t19 = dp[5][x];
            const int t15 = dpn[5][x];
            const int t16 = dpp[6][x];
            const int t17 = dpn[6][x];
            if (t6 && ((t1 && t2 && ((t3 && t4 && t14 && t15) || (t5 && t18))) ||
              (t3 && t4 && t7 && t19) ||
              (t5 && t18 && ((t1 && t3 && t14) || (t2 && t4 && t15) || (t1 && t8 && t12) || (t2 && t9 && t13))) ||
              (t7 && t19 && ((t1 && t3 && t14) || (t2 && t4 && t15) || (t3 && t10 && t16) || (t4 && t11 && t17)))))
              maskw[x] = val1;
            else if (t1 && t5 && t2 && t8 && t9 && t12 && t13) maskw[x] = 20;
            else if (t3 && t7 && t4 && t10 && t11 && t16 && t17) maskw[x] = 10;
            else maskw[x] = 60;
          }
          if (y != 0)
          {
            for (int i = 0; i < 7; ++i)
              dpp[i] += dpitch;
          }
          if (y != Height - 3)
          {
            for (int i = 0; i < 7; ++i)
              dpn[i] += dpitch;
          }
          for (int i = 0; i < 7; ++i)
            dp[i] += dpitch;
          maskw += mask_pitch;
        }
      }
    }
  }
}

template<int planarType>
void TDeinterlace::expandMap_Planar(PVideoFrame &mask)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < 3; ++b)
  {
    const int plane = planes[b];
    unsigned char *maskp = mask->GetWritePtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const int mask_pitch2 = mask_pitch << 1;
    const int Height = mask->GetHeight(plane);
    const int Width = mask->GetRowSize(plane);
    const int dis = 
      b == 0 ? 
      expand : // luma
      (expand >> (planarType == 444 ? 0 : 1)); // chroma
    maskp += mask_pitch*field;
    for (int y = field; y < Height; y += 2)
    {
      for (int x = 0; x < Width; ++x)
      {
        if (maskp[x] == 0x3C)
        {
          int xt = x - 1;
          while (xt >= 0 && xt >= x - dis)
          {
            maskp[xt] = 0x3C;
            --xt;
          }
          xt = x + 1;
          int nc = x + dis + 1;
          while (xt < Width && xt <= x + dis)
          {
            if (maskp[xt] == 0x3C)
            {
              nc = xt;
              break;
            }
            else maskp[xt] = 0x3C;
            ++xt;
          }
          x = nc - 1;
        }
      }
      maskp += mask_pitch2;
    }
  }
}

template<int planarType>
void TDeinterlace::linkFULL_Planar(PVideoFrame &mask)
{
  unsigned char *maskpY = mask->GetWritePtr(PLANAR_Y);
  unsigned char *maskpV = mask->GetWritePtr(PLANAR_V);
  unsigned char *maskpU = mask->GetWritePtr(PLANAR_U);
  const int mask_pitchY = mask->GetPitch(PLANAR_Y);
  const int mask_pitchY2 = mask_pitchY << 1;
  const int mask_pitchY4 = mask_pitchY << 2;
  const int mask_pitchUV = mask->GetPitch(PLANAR_V);
  const int mask_pitchUV2 = mask_pitchUV << 1;
  const int HeightUV = mask->GetHeight(PLANAR_V);
  const int WidthUV = mask->GetRowSize(PLANAR_V);
  maskpY += mask_pitchY*field;
  maskpV += mask_pitchUV*field;
  maskpU += mask_pitchUV*field;
  if constexpr (planarType == 420) {
    unsigned char* maskpnY = maskpY + mask_pitchY2;
    for (int y = field; y < HeightUV; y += 2)
    {
      for (int x = 0; x < WidthUV; ++x)
      {
        if (((((unsigned short*)maskpY)[x] == (unsigned short)0x3C3C) &&
          (((unsigned short*)maskpnY)[x] == (unsigned short)0x3C3C)) ||
          maskpV[x] == 0x3C || maskpU[x] == 0x3C)
        {
          ((unsigned short*)maskpY)[x] = (unsigned short)0x3C3C;
          ((unsigned short*)maskpnY)[x] = (unsigned short)0x3C3C;
          maskpV[x] = maskpU[x] = 0x3C;
        }
      }
      maskpY += mask_pitchY4; // subsampling *2
      maskpnY += mask_pitchY4;
      maskpV += mask_pitchUV2;
      maskpU += mask_pitchUV2;
    }
  }
  else {
    // 422, 444
    for (int y = field; y < HeightUV; y+=2) // always by 2
    {
      for (int x = 0; x < WidthUV; ++x)
      {
        if constexpr (planarType == 422) {
          if (
            (maskpY[2 * x + 0] == 0x3C && maskpY[2 * x + 1] == 0x3C)
            || maskpV[x] == 0x3C || maskpU[x] == 0x3C)
          {
            ((unsigned short*)maskpY)[x] = (unsigned short)0x3C3C;
            maskpV[x] = maskpU[x] = 0x3C;
          }
        }
        else {
          // 444
          if (
            (maskpY[x] == 0x3C) ||
            maskpV[x] == 0x3C || maskpU[x] == 0x3C)
          {
            maskpY[x] = 0x3C;
            maskpV[x] = maskpU[x] = 0x3C;
          }
        }
      }
      maskpY += mask_pitchY2;
      maskpV += mask_pitchUV2;
      maskpU += mask_pitchUV2;
    }
  }
}

template<int planarType>
void TDeinterlace::linkYtoUV_Planar(PVideoFrame &mask)
{
  unsigned char *maskpY = mask->GetWritePtr(PLANAR_Y);
  unsigned char *maskpV = mask->GetWritePtr(PLANAR_V);
  unsigned char *maskpU = mask->GetWritePtr(PLANAR_U);
  const int mask_pitchY = mask->GetPitch(PLANAR_Y);
  const int mask_pitchY2 = mask_pitchY << 1;
  const int mask_pitchY4 = mask_pitchY << 2;
  const int mask_pitchUV = mask->GetPitch(PLANAR_V);
  const int mask_pitchUV2 = mask_pitchUV << 1;
  const int HeightUV = mask->GetHeight(PLANAR_V);
  const int WidthUV = mask->GetRowSize(PLANAR_V);
  maskpY += mask_pitchY*field;
  maskpV += mask_pitchUV*field;
  maskpU += mask_pitchUV*field;
  if (planarType == 420) {
    unsigned char* maskpnY = maskpY + mask_pitchY2;
    for (int y = field; y < HeightUV; y += 2)
    {
      for (int x = 0; x < WidthUV; ++x)
      {
        if (((unsigned short*)maskpY)[x] == (unsigned short)0x3C3C &&
          ((unsigned short*)maskpnY)[x] == (unsigned short)0x3C3C)
        {
          maskpV[x] = maskpU[x] = 0x3C;
        }
      }
      maskpY += mask_pitchY4; // subsampling additional * 2
      maskpnY += mask_pitchY4;
      maskpV += mask_pitchUV2;
      maskpU += mask_pitchUV2;
    }
  }
  else {
    // 422, 444
    for (int y = field; y < HeightUV; y+=2) // always by 2
    {
      for (int x = 0; x < WidthUV; ++x)
      {
        if constexpr (planarType == 422) {
          if (((unsigned short*)maskpY)[x] == (unsigned short)0x3C3C)
          {
            maskpV[x] = maskpU[x] = 0x3C;
          }
        }
        else { // 444
          if (maskpY[x] == 0x3C)
          {
            maskpV[x] = maskpU[x] = 0x3C;
          }
        }
      }
      maskpY += mask_pitchY2;
      // maskpnY += mask_pitchY4;
      maskpV += mask_pitchUV2;
      maskpU += mask_pitchUV2;
    }
  }
}

template<int planarType>
void TDeinterlace::linkUVtoY_Planar(PVideoFrame &mask)
{
  unsigned char *maskpY = mask->GetWritePtr(PLANAR_Y);
  unsigned char *maskpV = mask->GetWritePtr(PLANAR_V);
  unsigned char *maskpU = mask->GetWritePtr(PLANAR_U);
  const int mask_pitchY = mask->GetPitch(PLANAR_Y);
  const int mask_pitchY2 = mask_pitchY << 1;
  const int mask_pitchY4 = mask_pitchY << 2;
  const int mask_pitchUV = mask->GetPitch(PLANAR_V);
  const int mask_pitchUV2 = mask_pitchUV << 1;
  const int HeightUV = mask->GetHeight(PLANAR_V);
  const int WidthUV = mask->GetRowSize(PLANAR_V);
  maskpY += mask_pitchY*field;
  maskpV += mask_pitchUV*field;
  maskpU += mask_pitchUV*field;
  
  // only used at 420:
  unsigned char *maskpnY = maskpY + mask_pitchY2;
  for (int y = field; y < HeightUV; y += 2)
  {
    for (int x = 0; x < WidthUV; ++x)
    {
      if (maskpV[x] == 0x3C || maskpU[x] == 0x3C)
      {
        if constexpr (planarType == 420) {
          // fill Y: 2x2 
          ((unsigned short*)maskpY)[x] = (unsigned short)0x3C3C;
          ((unsigned short*)maskpnY)[x] = (unsigned short)0x3C3C;
        }
        else if constexpr (planarType == 422) {
          maskpY[x * 2 + 0] = 0x3C;
          maskpY[x * 2 + 1] = 0x3C;
        }
        else {
          maskpY[x] = 0x3C;
        }
      }
    }
    maskpY += mask_pitchY4;
    if constexpr (planarType == 420)
      maskpnY += mask_pitchY4;
    maskpV += mask_pitchUV2;
    maskpU += mask_pitchUV2;
  }
}

void TDeinterlace::denoisePlanar(PVideoFrame &mask)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < 3; ++b)
  {
    const int plane = planes[b];
    unsigned char *maskp = mask->GetWritePtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const int mask_pitch2 = mask_pitch << 1;
    const int Height = mask->GetHeight(plane);
    const int Width = mask->GetRowSize(plane);
    maskp += mask_pitch*(2 + field);
    unsigned char *maskpp = maskp - mask_pitch2;
    unsigned char *maskpn = maskp + mask_pitch2;
    for (int y = 2; y < Height - 2; y += 2)
    {
      for (int x = 1; x < Width - 1; ++x)
      {
        if (maskp[x] == 0x3C)
        {
          if (maskpp[x - 1] == 0x3C) continue;
          if (maskpp[x] == 0x3C) continue;
          if (maskpp[x + 1] == 0x3C) continue;
          if (maskp[x - 1] == 0x3C) continue;
          if (maskp[x + 1] == 0x3C) continue;
          if (maskpn[x - 1] == 0x3C) continue;
          if (maskpn[x] == 0x3C) continue;
          if (maskpn[x + 1] == 0x3C) continue;
          maskp[x] = (maskp[x - 1] == maskp[x + 1]) ? maskp[x - 1] :
            (maskpp[x] == maskpn[x]) ? maskpp[x] : maskp[x - 1];
        }
      }
      maskpp += mask_pitch2;
      maskp += mask_pitch2;
      maskpn += mask_pitch2;
    }
  }
}

bool TDeinterlace::checkCombedPlanar(PVideoFrame &src, int &MIC, IScriptEnvironment *env)
{
  PVideoFrame cmask = env->NewVideoFrame(vi_saved);
  
  long cpu = env->GetCPUFlags();
  if (opt == 0) cpu = 0;
  bool use_sse2 = (cpu & CPUF_SSE2) ? true : false;

  // cthresh: Area combing threshold used for combed frame detection.
  // This essentially controls how "strong" or "visible" combing must be to be detected.
  // Good values are from 6 to 12. If you know your source has a lot of combed frames set 
  // this towards the low end(6 - 7). If you know your source has very few combed frames set 
  // this higher(10 - 12). Going much lower than 5 to 6 or much higher than 12 is not recommended.
  const int cthresh6 = cthresh * 6;
  __m128i cthreshb_m128i;
  __m128i cthresh6w_m128i;

  if (metric == 0 && use_sse2)
  {
    unsigned int cthresht = min(max(255 - cthresh - 1, 0), 255);
    cthreshb_m128i = _mm_set1_epi8(cthresht);
    unsigned int cthresh6t = min(max(65535 - cthresh * 6 - 1, 0), 65535);
    cthresh6w_m128i = _mm_set1_epi16(cthresh6t);
  }
  else if (metric == 1 && use_sse2)
  {
    cthreshb_m128i = _mm_set1_epi32(cthresh * cthresh);
  }

  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = chroma ? 3 : 1;
  for (int b = 0; b < stop; ++b) // fix 2020413: mode==0 checkCombedPlanar failed (never processed luma)
  {
    const int plane = planes[b];
    const unsigned char *srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int Width = src->GetRowSize(plane);
    const int Height = src->GetHeight(plane);
    const unsigned char *srcpp = srcp - src_pitch;
    const unsigned char *srcppp = srcpp - src_pitch;
    const unsigned char *srcpn = srcp + src_pitch;
    const unsigned char *srcpnn = srcpn + src_pitch;
    unsigned char *cmkp = cmask->GetWritePtr(plane);
    const int cmk_pitch = cmask->GetPitch(plane);
    
    if (cthresh < 0) { 
      memset(cmkp, 255, Height*cmk_pitch); 
      continue;
    }
    memset(cmkp, 0, Height*cmk_pitch);

    if (metric == 0)
    {
      // top 1 
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
      // top #2
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
      // middle Height - 4
      if (use_sse2)
      {
        check_combing_SSE2(srcp, cmkp, Width, Height - 4, src_pitch, src_pitch * 2, cmk_pitch, cthreshb_m128i, cthresh6w_m128i);
        srcppp += src_pitch * (Height - 4);
        srcpp += src_pitch * (Height - 4);
        srcp += src_pitch * (Height - 4);
        srcpn += src_pitch * (Height - 4);
        srcpnn += src_pitch * (Height - 4);
        cmkp += cmk_pitch * (Height - 4);
      }
      else
      {
        for (int y = 2; y < Height - 2; ++y)
        {
          for (int x = 0; x < Width; ++x)
          {
            const int sFirst = srcp[x] - srcpp[x];
            const int sSecond = srcp[x] - srcpn[x];
            if ((sFirst > cthresh && sSecond > cthresh) || (sFirst < -cthresh && sSecond < -cthresh))
            {
              if (abs(srcppp[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpp[x] + srcpn[x]))) > cthresh6)
                cmkp[x] = 0xFF;
            }
          }
          srcppp += src_pitch;
          srcpp += src_pitch;
          srcp += src_pitch;
          srcpn += src_pitch;
          srcpnn += src_pitch;
          cmkp += cmk_pitch;
        }
      }
      // bottom #-2
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
      // bottom #-1
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
      // metric == 1: squared
      const int cthreshsq = cthresh*cthresh;
      // top #1
      for (int x = 0; x < Width; ++x)
      {
        if ((srcp[x] - srcpn[x])*(srcp[x] - srcpn[x]) > cthreshsq)
          cmkp[x] = 0xFF;
      }
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      cmkp += cmk_pitch;
      // middle Height - 2
      if (use_sse2)
      {
        check_combing_SSE2_M1(srcp, cmkp, Width, Height - 2, src_pitch, cmk_pitch, cthreshb_m128i);
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
      // Bottom
      for (int x = 0; x < Width; ++x)
      {
        if ((srcp[x] - srcpp[x])*(srcp[x] - srcpp[x]) > cthreshsq)
          cmkp[x] = 0xFF;
      }
    }
  }

  // 'chroma' parameter default false.
  // Includes chroma combing in the decision about whether a frame is combed.
  if (chroma)
  {
    unsigned char *cmkp = cmask->GetWritePtr(PLANAR_Y);
    unsigned char *cmkpU = cmask->GetWritePtr(PLANAR_U);
    unsigned char *cmkpV = cmask->GetWritePtr(PLANAR_V);
    const int Width = cmask->GetRowSize(PLANAR_V);
    const int Height = cmask->GetHeight(PLANAR_V);
    const int cmk_pitch = cmask->GetPitch(PLANAR_Y) << 1;
    const int cmk_pitchUV = cmask->GetPitch(PLANAR_V);
    unsigned char *cmkpp = cmkp - (cmk_pitch >> 1);
    unsigned char *cmkpn = cmkp + (cmk_pitch >> 1);
    unsigned char *cmkpnn = cmkpn + (cmk_pitch >> 1);
    unsigned char *cmkppU = cmkpU - cmk_pitchUV;
    unsigned char *cmkpnU = cmkpU + cmk_pitchUV;
    unsigned char *cmkppV = cmkpV - cmk_pitchUV;
    unsigned char *cmkpnV = cmkpV + cmk_pitchUV;

    int planarType;
    if (vi_saved.Is420()) planarType = 420;
    else if (vi_saved.Is422()) planarType = 422;
    else if (vi_saved.Is444()) planarType = 444;

    for (int y = 1; y < Height - 1; ++y)
    {
      cmkpp += cmk_pitch;
      cmkp += cmk_pitch;
      cmkpn += cmk_pitch;
      cmkpnn += cmk_pitch;
      cmkppV += cmk_pitchUV;
      cmkpV += cmk_pitchUV;
      cmkpnV += cmk_pitchUV;
      cmkppU += cmk_pitchUV;
      cmkpU += cmk_pitchUV;
      cmkpnU += cmk_pitchUV;
      for (int x = 1; x < Width - 1; ++x)
      {
        if ((cmkpV[x] == 0xFF && (cmkpV[x - 1] == 0xFF || cmkpV[x + 1] == 0xFF ||
          cmkppV[x - 1] == 0xFF || cmkppV[x] == 0xFF || cmkppV[x + 1] == 0xFF ||
          cmkpnV[x - 1] == 0xFF || cmkpnV[x] == 0xFF || cmkpnV[x + 1] == 0xFF)) ||
          (cmkpU[x] == 0xFF && (cmkpU[x - 1] == 0xFF || cmkpU[x + 1] == 0xFF ||
            cmkppU[x - 1] == 0xFF || cmkppU[x] == 0xFF || cmkppU[x + 1] == 0xFF ||
            cmkpnU[x - 1] == 0xFF || cmkpnU[x] == 0xFF || cmkpnU[x + 1] == 0xFF)))
        {
          // fixme: template?
          // like in Link chroma
          if (planarType == 420) {
            ((unsigned short*)cmkp)[x] = (unsigned short)0xFFFF;
            ((unsigned short*)cmkpn)[x] = (unsigned short)0xFFFF;
            if (y & 1) ((unsigned short*)cmkpp)[x] = (unsigned short)0xFFFF;
            else ((unsigned short*)cmkpnn)[x] = (unsigned short)0xFFFF;
          }
          else if (planarType == 422) {
            ((unsigned short*)cmkp)[x] = (unsigned short)0xFFFF;
            ((unsigned short*)cmkpn)[x] = (unsigned short)0xFFFF;
            ((unsigned short*)cmkpp)[x] = (unsigned short)0xFFFF;
          }
          else if (planarType == 444) {
            cmkp[x] = 0xFF;
            cmkpn[x] = 0xFF;
            cmkpp[x] = 0xFF;
          }
        }
      }
    }
  }
  // end of chroma handling

  const int cmk_pitch = cmask->GetPitch(PLANAR_Y);
  const unsigned char *cmkp = cmask->GetReadPtr(PLANAR_Y) + cmk_pitch;
  const unsigned char *cmkpp = cmkp - cmk_pitch;
  const unsigned char *cmkpn = cmkp + cmk_pitch;
  const int Width = cmask->GetRowSize(PLANAR_Y);
  const int Height = cmask->GetHeight(PLANAR_Y);
  const int xblocks = ((Width + blockx_half) >> blockx_shift) + 1;
  const int xblocks4 = xblocks << 2;
  const int yblocks = ((Height + blocky_half) >> blocky_shift) + 1;
  const int arraysize = (xblocks*yblocks) << 2;
  memset(cArray, 0, arraysize * sizeof(int));
  int Heighta = (Height >> (blocky_shift - 1)) << (blocky_shift - 1);
  if (Heighta == Height) Heighta = Height - blocky_half;
  const int Widtha = (Width >> (blockx_shift - 1)) << (blockx_shift - 1);
  // quick case for 16x16 base
  const bool use_sse2_8x8_sum = (use_sse2 && blockx_half == 8 && blocky_half == 8) ? true : false;
  
  // top y block
  for (int y = 1; y < blocky_half; ++y)
  {
    const int temp1 = (y >> blocky_shift)*xblocks4;
    const int temp2 = ((y + blocky_half) >> blocky_shift)*xblocks4;
    for (int x = 0; x < Width; ++x)
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
    if (use_sse2_8x8_sum)
    {
      for (int x = 0; x < Widtha; x += blockx_half)
      {
        // only if blockx_half and blocky_half is 8x8!!! checked above
        int sum = 0;
        compute_sum_8xN_sse2<8>(cmkpp + x, cmk_pitch, sum);
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
          for (int v = 0; v < blockx_half; ++v)
          {
            if (cmkppT[x + v] == 0xFF && cmkpT[x + v] == 0xFF && cmkpnT[x + v] == 0xFF)
              ++sum;
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
    // rest on the right
    for (int x = Widtha; x < Width; ++x)
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
  // rest non-whole blocky at the bottom
  for (int y = Heighta; y < Height - 1; ++y)
  {
    const int temp1 = (y >> blocky_shift)*xblocks4;
    const int temp2 = ((y + blocky_half) >> blocky_shift)*xblocks4;
    for (int x = 0; x < Width; ++x)
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

  // 'MI': The number of required combed pixels inside any of the blockx by blocky sized blocks 
  // on the frame for the frame to be considered combed. While cthresh controls how "visible" or
  // "strong" the combing must be, this setting controls how much combing there must be in any 
  // localized area(a blockx by blocky sized window) on the frame.
  // Min setting = 0, max setting = blockx x blocky (at which point no frames will ever be detected as combed).
  // default - 64 (int)

  if (MIC > MI) return true;
  return false;
}

// common planar YUY2
void TDeinterlace::subtractFields(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
  VideoInfo &vit, int &aPn, int &aNn, int &aPm, int &aNm, int fieldt, int ordert,
  int optt, bool d2, int _slow, IScriptEnvironment *env)
{
  if (_slow == 1)
    return subtractFields1(prv, src, nxt, vit, aPn, aNn, aPm, aNm, fieldt, ordert, optt, d2, env);
  else if (_slow == 2)
    return subtractFields2(prv, src, nxt, vit, aPn, aNn, aPm, aNm, fieldt, ordert, optt, d2, env);

  PVideoFrame map = env->NewVideoFrame(vit);
  const int stop = vit.IsYUY2() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  unsigned long accumPns = 0, accumNns = 0;
  unsigned long accumPms = 0, accumNms = 0;
  aPn = aNn = 0;
  aPm = aNm = 0;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    unsigned char *mapp = map->GetWritePtr(plane);
    const int map_pitch = map->GetPitch(plane) << 1;
    const unsigned char *prvp = prv->GetReadPtr(plane);
    const int prv_pitch = prv->GetPitch(plane);
    const unsigned char *srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int Width = src->GetRowSize(plane); // fixme: at HBD /2
    const int Height = src->GetHeight(plane);
    const unsigned char *nxtp = nxt->GetReadPtr(plane);
    int nxt_pitch = nxt->GetPitch(plane);
    const int startx = vit.IsYUY2() ? 16 : 8 >> vit.GetPlaneWidthSubsampling(plane); // stop > 1 ? (b == 0 ? 8 : 4) : 16; // fixed: YV12/YUY2 specific
    const int stopx = Width - startx;
    const unsigned char *prvpf, *curf, *nxtpf;
    int prvf_pitch, curf_pitch, nxtf_pitch;
    if (d2)
    {
      prvf_pitch = prv_pitch << 1;
      curf_pitch = src_pitch << 1;
      nxtf_pitch = nxt_pitch << 1;
      prvpf = prvp + ((fieldt == 1 ? 1 : 2)*prv_pitch);
      curf = srcp + ((3 - fieldt)*src_pitch);
      nxtpf = nxtp + ((fieldt == 1 ? 1 : 2)*nxt_pitch);
    }
    else if (fieldt^ordert)
    {
      prvf_pitch = src_pitch << 1;
      curf_pitch = src_pitch << 1;
      nxtf_pitch = nxt_pitch << 1;
      prvpf = srcp + ((fieldt == 1 ? 1 : 2)*src_pitch);
      curf = srcp + ((3 - fieldt)*src_pitch);
      nxtpf = nxtp + ((fieldt == 1 ? 1 : 2)*nxt_pitch);
    }
    else
    {
      prvf_pitch = prv_pitch << 1;
      curf_pitch = src_pitch << 1;
      nxtf_pitch = src_pitch << 1;
      prvpf = prvp + ((fieldt == 1 ? 1 : 2)*prv_pitch);
      curf = srcp + ((3 - fieldt)*src_pitch);
      nxtpf = srcp + ((fieldt == 1 ? 1 : 2)*src_pitch);
    }
    mapp = mapp + ((fieldt == 1 ? 1 : 2)*(map_pitch >> 1));
    const unsigned char *prvnf = prvpf + prvf_pitch;
    const unsigned char *curpf = curf - curf_pitch;
    const unsigned char *curnf = curf + curf_pitch;
    const unsigned char *nxtnf = nxtpf + nxtf_pitch;
    unsigned char *mapn = mapp + map_pitch;
    if (fieldt != 1)
      buildDiffMapPlane2(prvpf - prvf_pitch, nxtpf - nxtf_pitch, mapp - map_pitch, prvf_pitch,
        nxtf_pitch, map_pitch, Height >> 1, Width, optt, env);
    else
      buildDiffMapPlane2(prvnf - prvf_pitch, nxtnf - nxtf_pitch, mapn - map_pitch, prvf_pitch,
        nxtf_pitch, map_pitch, Height >> 1, Width, optt, env);
    for (int y = 2; y < Height - 2; y += 2)
    {
      for (int x = startx; x < stopx; x++)
      {
        unsigned char eax = (mapp[x] << 2) + mapn[x];
        if (eax == 0) {
          if (x+1 < stopx) continue;
          break;
        }
        int ecx = (curf[x] << 2) + curpf[x] + curnf[x];

        int edi = prvpf[x] + prvnf[x];
        int edx = abs(edi * 3 - ecx);
        if (edx > 23) {
          accumPns += edx;
          if (edx > 42)
          {
            if ((eax & 10) != 0)
              accumPms += edx;
          }
        }

        edi = nxtpf[x] + nxtnf[x];
        edx = abs(edi * 3 - ecx);
        if (edx > 23) {
          accumNns += edx;
          if (edx > 42) {
            if ((eax & 10) != 0)
              accumNms += edx;
          }
        }
      }
      mapp += map_pitch;
      prvpf += prvf_pitch;
      curpf += curf_pitch;
      prvnf += prvf_pitch;
      curf += curf_pitch;
      nxtpf += nxtf_pitch;
      curnf += curf_pitch;
      nxtnf += nxtf_pitch;
      mapn += map_pitch;
    } // huhh
  }
  aPn = int(accumPns / 6.0 + 0.5);
  aNn = int(accumNns / 6.0 + 0.5);
  aPm = int(accumPms / 6.0 + 0.5);
  aNm = int(accumNms / 6.0 + 0.5);
}

// common planar / YUY2
void TDeinterlace::subtractFields1(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
  VideoInfo &vit, int &aPn, int &aNn, int &aPm, int &aNm, int fieldt, int ordert,
  int optt, bool d2, IScriptEnvironment *env)
{
  PVideoFrame map = env->NewVideoFrame(vit);
  const int stop = vit.IsYUY2() ? 1 : 3;
  int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  unsigned long accumPns = 0, accumNns = 0, accumNmls = 0;
  unsigned long accumPms = 0, accumNms = 0, accumPmls = 0;
  aPn = aNn = 0;
  aPm = aNm = 0;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    unsigned char* mapp = map->GetWritePtr(plane);
    const int map_pitch = map->GetPitch(plane) << 1;
    const unsigned char* prvp = prv->GetReadPtr(plane);
    const int prv_pitch = prv->GetPitch(plane);
    const unsigned char* srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int Width = src->GetRowSize(plane);
    const int Height = src->GetHeight(plane);
    const unsigned char* nxtp = nxt->GetReadPtr(plane);
    const int nxt_pitch = nxt->GetPitch(plane);
    const int startx = vit.IsYUY2() ? 16 : 8 >> vit.GetPlaneWidthSubsampling(plane); // stop > 1 ? (b == 0 ? 8 : 4) : 16; // fixed: YV12/YUY2 specific
    const int stopx = Width - startx;
    memset(mapp, 0, Height * (map_pitch >> 1));
    const unsigned char* prvpf, * curf, * nxtpf;
    int prvf_pitch, curf_pitch, nxtf_pitch, tp;
    if (d2)
    {
      prvf_pitch = prv_pitch << 1;
      curf_pitch = src_pitch << 1;
      nxtf_pitch = nxt_pitch << 1;
      prvpf = prvp + ((fieldt == 1 ? 1 : 2) * prv_pitch);
      curf = srcp + ((3 - fieldt) * src_pitch);
      nxtpf = nxtp + ((fieldt == 1 ? 1 : 2) * nxt_pitch);
    }
    else if (fieldt ^ ordert)
    {
      prvf_pitch = src_pitch << 1;
      curf_pitch = src_pitch << 1;
      nxtf_pitch = nxt_pitch << 1;
      prvpf = srcp + ((fieldt == 1 ? 1 : 2) * src_pitch);
      curf = srcp + ((3 - fieldt) * src_pitch);
      nxtpf = nxtp + ((fieldt == 1 ? 1 : 2) * nxt_pitch);
    }
    else
    {
      prvf_pitch = prv_pitch << 1;
      curf_pitch = src_pitch << 1;
      nxtf_pitch = src_pitch << 1;
      prvpf = prvp + ((fieldt == 1 ? 1 : 2) * prv_pitch);
      curf = srcp + ((3 - fieldt) * src_pitch);
      nxtpf = srcp + ((fieldt == 1 ? 1 : 2) * src_pitch);
    }
    mapp = mapp + ((fieldt == 1 ? 1 : 2) * (map_pitch >> 1));
    const unsigned char* prvnf = prvpf + prvf_pitch;
    const unsigned char* curpf = curf - curf_pitch;
    const unsigned char* curnf = curf + curf_pitch;
    const unsigned char* nxtnf = nxtpf + nxtf_pitch;
    unsigned char* mapn = mapp + map_pitch;
    if (b == 0) tp = tpitchy;
    else tp = tpitchuv;
    
    if (vit.IsPlanar()) // plane count 3: planar YV12
    {
      if (fieldt != 1)
        buildDiffMapPlane_Planar(prvpf, nxtpf, mapp, prvf_pitch, nxtf_pitch, map_pitch, Height, Width, tp, env);
      else
        buildDiffMapPlane_Planar(prvnf, nxtnf, mapn, prvf_pitch, nxtf_pitch, map_pitch, Height, Width, tp, env);
    }
    else
    {
      if (fieldt != 1)
        buildDiffMapPlaneYUY2(prvpf, nxtpf, mapp, prvf_pitch, nxtf_pitch, map_pitch, Height, Width, tp, env);
      else
        buildDiffMapPlaneYUY2(prvnf, nxtnf, mapn, prvf_pitch, nxtf_pitch, map_pitch, Height, Width, tp, env);
    }

    for (int y = 2; y < Height - 2; y += 2)
    {
      for (int x = startx; x < stopx; x++) {
        int eax = (mapp[x] << 3) + mapn[x];
        if (eax == 0)
            continue;

        int ecx = (curf[x] << 2) + curpf[x] + curnf[x];

        int edx = prvpf[x] + prvnf[x];
        edx = abs(edx * 3 - ecx);
        if (edx > 23) {
          if ((eax & 9) != 0)
            accumPns += edx;
          if (edx > 42) {
            if ((eax & 18) != 0)
              accumPms += edx;
            if ((eax & 36) != 0)
              accumPmls += edx;
          }
        }

        edx = nxtpf[x] + nxtnf[x];
        edx = abs(edx * 3 - ecx);
        if (edx > 23) {
          if ((eax & 9) != 0)
            accumNns += edx;
          if (edx > 42) {
            if ((eax & 18) != 0)
              accumNms += edx;
            if ((eax & 36) != 0)
              accumNmls += edx;
          }
        }

      } // x
      mapp += map_pitch;
      prvpf += prvf_pitch;
      curpf += curf_pitch;
      prvnf += prvf_pitch;
      curf += curf_pitch;
      nxtpf += nxtf_pitch;
      curnf += curf_pitch;
      nxtnf += nxtf_pitch;
      mapn += map_pitch;
    }// for y
  } //for b 

  if (accumPms < 500 && accumNms < 500 && (accumPmls >= 500 || accumNmls >= 500) &&
    max(accumPmls, accumNmls) > 3 * min(accumPmls, accumNmls))
  {
    accumPms = accumPmls;
    accumNms = accumNmls;
  }
  aPn = int(accumPns / 6.0 + 0.5);
  aNn = int(accumNns / 6.0 + 0.5);
  aPm = int(accumPms / 6.0 + 0.5);
  aNm = int(accumNms / 6.0 + 0.5);
}

// common planar YUY2
void TDeinterlace::subtractFields2(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt,
  VideoInfo& vit, int& aPn, int& aNn, int& aPm, int& aNm, int fieldt, int ordert,
  int optt, bool d2, IScriptEnvironment* env)
{
  PVideoFrame map = env->NewVideoFrame(vit);
  const int stop = vit.IsYUY2() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  unsigned long accumPns = 0, accumNns = 0, accumNmls = 0;
  unsigned long accumPms = 0, accumNms = 0, accumPmls = 0;
  aPn = aNn = 0;
  aPm = aNm = 0;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    unsigned char* mapp = map->GetWritePtr(plane);
    const int map_pitch = map->GetPitch(plane) << 1;
    const unsigned char* prvp = prv->GetReadPtr(plane);
    const int prv_pitch = prv->GetPitch(plane);
    const unsigned char* srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int Width = src->GetRowSize(plane);
    const int Height = src->GetHeight(plane);
    const unsigned char* nxtp = nxt->GetReadPtr(plane);
    const int nxt_pitch = nxt->GetPitch(plane);
    const int startx = vit.IsYUY2() ? 16 : (8 >> vit.GetPlaneWidthSubsampling(plane));
    // stop > 1 ? (b == 0 ? 8 : 4) : 16; // fixed: YV12/YUY2 specific
    const int stopx = Width - startx;
    memset(mapp, 0, Height * (map_pitch >> 1));
    const unsigned char* prvpf, * curf, * nxtpf;
    int prvf_pitch, curf_pitch, nxtf_pitch, tp;
    if (d2)
    {
      prvf_pitch = prv_pitch << 1;
      curf_pitch = src_pitch << 1;
      nxtf_pitch = nxt_pitch << 1;
      prvpf = prvp + ((fieldt == 1 ? 1 : 2) * prv_pitch);
      curf = srcp + ((3 - fieldt) * src_pitch);
      nxtpf = nxtp + ((fieldt == 1 ? 1 : 2) * nxt_pitch);
    }
    else if (fieldt ^ ordert)
    {
      prvf_pitch = src_pitch << 1;
      curf_pitch = src_pitch << 1;
      nxtf_pitch = nxt_pitch << 1;
      prvpf = srcp + ((fieldt == 1 ? 1 : 2) * src_pitch);
      curf = srcp + ((3 - fieldt) * src_pitch);
      nxtpf = nxtp + ((fieldt == 1 ? 1 : 2) * nxt_pitch);
    }
    else
    {
      prvf_pitch = prv_pitch << 1;
      curf_pitch = src_pitch << 1;
      nxtf_pitch = src_pitch << 1;
      prvpf = prvp + ((fieldt == 1 ? 1 : 2) * prv_pitch);
      curf = srcp + ((3 - fieldt) * src_pitch);
      nxtpf = srcp + ((fieldt == 1 ? 1 : 2) * src_pitch);
    }
    mapp = mapp + ((fieldt == 1 ? 1 : 2) * (map_pitch >> 1));
    const unsigned char* prvppf = prvpf - prvf_pitch;
    const unsigned char* prvnf = prvpf + prvf_pitch;
    const unsigned char* prvnnf = prvnf + prvf_pitch;
    const unsigned char* curpf = curf - curf_pitch;
    const unsigned char* curnf = curf + curf_pitch;
    const unsigned char* nxtppf = nxtpf - nxtf_pitch;
    const unsigned char* nxtnf = nxtpf + nxtf_pitch;
    const unsigned char* nxtnnf = nxtnf + nxtf_pitch;
    unsigned char* mapn = mapp + map_pitch;
    
    if (b == 0) 
      tp = tpitchy;
    else 
      tp = tpitchuv;

    if (vit.IsPlanar())
    {
      // planar YUV
      if (fieldt != 1)
        buildDiffMapPlane_Planar(prvpf, nxtpf, mapp, prvf_pitch, nxtf_pitch, map_pitch, Height, Width, tp, env);
      else
        buildDiffMapPlane_Planar(prvnf, nxtnf, mapn, prvf_pitch, nxtf_pitch, map_pitch, Height, Width, tp, env);
    }
    else
    {
      if (fieldt != 1)
        buildDiffMapPlaneYUY2(prvpf, nxtpf, mapp, prvf_pitch, nxtf_pitch, map_pitch, Height, Width, tp, env);
      else
        buildDiffMapPlaneYUY2(prvnf, nxtnf, mapn, prvf_pitch, nxtf_pitch, map_pitch, Height, Width, tp, env);
    }

    if (fieldt == 0)
    {
      for (int y = 2; y < Height - 2; y += 2)
      {
        for (int x = startx; x < stopx; x++) {
          int eax = (mapp[x] << 3) + mapn[x];
          if (eax == 0)
              continue;

          int ecx = (curf[x] << 2) + curpf[x] + curnf[x];

          int edx = prvpf[x] + prvnf[x];
          edx = abs(edx * 3 - ecx);
          if (edx > 23) {
            if ((eax & 9) != 0)
              accumPns += edx;
            if (edx > 42) {
              if ((eax & 18) != 0)
                accumPms += edx;
              if ((eax & 36) != 0)
                accumPmls += edx;
            }
          }

          edx = nxtpf[x] + nxtnf[x];
          edx = abs(edx * 3 - ecx);
          if (edx > 23) {
            if ((eax & 9) != 0)
              accumNns += edx;
            if (edx > 42) {
              if ((eax & 18) != 0)
                accumNms += edx;
              if ((eax & 36) != 0)
                accumNmls += edx;
            }
          }
          // this part is plus to the version'1'
          if ((eax & 56) != 0) {

            int edx = curpf[x] + curf[x];
            int edx_mul3 = edx * 3;

            int ecx = (prvpf[x] << 2) + prvppf[x] + prvnf[x];

            edx = abs(edx_mul3 - ecx);

            if (edx > 23) {
              if ((eax & 8) != 0)
                accumPns += edx;
              if (edx > 42) {
                if ((eax & 16) != 0)
                  accumPms += edx;
                if ((eax & 32) != 0)
                  accumPmls += edx;
              }
            }

            ecx = (nxtpf[x] << 2) + nxtppf[x] + nxtnf[x];

            edx = abs(edx_mul3 - ecx);

            if (edx > 23) {
              if ((eax & 8) != 0)
                accumNns += edx;
              if (edx > 42) {
                if ((eax & 16) != 0)
                  accumNms += edx;
                if ((eax & 32) != 0)
                  accumNmls += edx;
              }
            }

          }
        } // x
        mapp += map_pitch;
        prvpf += prvf_pitch;
        curpf += curf_pitch;
        prvnf += prvf_pitch;
        curf += curf_pitch;
        nxtpf += nxtf_pitch;
        prvppf += nxtf_pitch;
        curnf += curf_pitch;
        nxtnf += nxtf_pitch;
        mapn += map_pitch;
        nxtppf += nxtf_pitch;
      }// for y
    }
    else
    { // fieldt == 0 else
      for (int y = 2; y < Height - 2; y += 2)
      {
        for (int x = startx; x < stopx; x++) {
          int eax = (mapp[x] << 3) + mapn[x];
          if (eax == 0)
            continue;

          int ecx = (curf[x] << 2) + curpf[x] + curnf[x];

          int edx = prvpf[x] + prvnf[x];
          edx = abs(edx * 3 - ecx);
          if (edx > 23) {
            if ((eax & 9) != 0)
              accumPns += edx;
            if (edx > 42) {
              if ((eax & 18) != 0)
                accumPms += edx;
              if ((eax & 36) != 0)
                accumPmls += edx;
            }
          }

          edx = nxtpf[x] + nxtnf[x];
          edx = abs(edx * 3 - ecx);
          if (edx > 23) {
            if ((eax & 9) != 0)
              accumNns += edx;
            if (edx > 42) {
              if ((eax & 18) != 0)
                accumNms += edx;
              if ((eax & 36) != 0)
                accumNmls += edx;
            }
          }

          // p61: this part is plus to the version'1'
          if ((eax & 7) != 0) {

            int edx = curf[x] + curnf[x];
            int edx_mul3 = edx * 3;

            int ecx = (prvnf[x] << 2) + prvpf[x] + prvnnf[x];

            edx = abs(edx_mul3 - ecx);

            if (edx > 23) {
              if ((eax & 1) != 0)
                accumPns += edx;
              if (edx > 42) {
                if ((eax & 2) != 0)
                  accumPms += edx;
                if ((eax & 4) != 0)
                  accumPmls += edx;
              }
            }
            // p91
            ecx = (nxtnf[x] << 2) + nxtpf[x] + nxtnnf[x];

            edx = abs(edx_mul3 - ecx);

            if (edx > 23) {
              if ((eax & 1) != 0)
                accumNns += edx;
              if (edx > 42) {
                if ((eax & 2) != 0)
                  accumNms += edx;
                if ((eax & 4) != 0)
                  accumNmls += edx;
              }
            }

          }

        } // x
        mapp += map_pitch;
        prvpf += prvf_pitch;
        curpf += curf_pitch;
        prvnf += prvf_pitch;
        curf += curf_pitch;
        prvnnf += prvf_pitch; // instead of prvppf
        nxtpf += nxtf_pitch;
        curnf += curf_pitch;
        nxtnf += nxtf_pitch;
        mapn += map_pitch;
        nxtnnf += nxtf_pitch; // instead of nxtppf
      }// for y
    } // if
  } // for b

  if (accumPms < 500 && accumNms < 500 && (accumPmls >= 500 || accumNmls >= 500) &&
    max(accumPmls, accumNmls) > 3 * min(accumPmls, accumNmls))
  {
    accumPms = accumPmls;
    accumNms = accumNmls;
  }
  aPn = int(accumPns / 6.0 + 0.5);
  aNn = int(accumNns / 6.0 + 0.5);
  aPm = int(accumPms / 6.0 + 0.5);
  aNm = int(accumNms / 6.0 + 0.5);
}

void TDeinterlace::mapColorsPlanar(PVideoFrame &dst, PVideoFrame &mask)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < 3; ++b)
  {
    const int plane = planes[b];
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const int Height = mask->GetHeight(plane);
    const int Width = mask->GetRowSize(plane);
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
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
}

void TDeinterlace::mapMergePlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < 3; ++b)
  {
    const int plane = planes[b];
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const int Height = mask->GetHeight(plane);
    const int Width = mask->GetRowSize(plane);
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    const unsigned char *prvp = prv->GetReadPtr(plane);
    const int prv_pitch = prv->GetPitch(plane);
    const unsigned char *srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const unsigned char *nxtp = nxt->GetReadPtr(plane);
    const int nxt_pitch = nxt->GetPitch(plane);
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
        else if (maskp[x] == 60) dstp[x] = 255;
      }
      prvp += prv_pitch;
      srcp += src_pitch;
      nxtp += nxt_pitch;
      maskp += mask_pitch;
      dstp += dst_pitch;
    }
  }
}

void TDeinterlace::eDeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &efrm)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < 3; ++b)
  {
    const int plane = planes[b];
    const unsigned char *prvp = prv->GetReadPtr(plane);
    const int prv_pitch = prv->GetPitch(plane);
    const unsigned char *srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int width = src->GetRowSize(plane);
    const int height = src->GetHeight(plane);
    const unsigned char *nxtp = nxt->GetReadPtr(plane);
    const int nxt_pitch = nxt->GetPitch(plane);
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const unsigned char *efrmp = efrm->GetReadPtr(plane);
    const int efrm_pitch = efrm->GetPitch(plane);
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        if (maskp[x] == 10) dstp[x] = srcp[x];
        else if (maskp[x] == 20) dstp[x] = prvp[x];
        else if (maskp[x] == 30) dstp[x] = nxtp[x];
        else if (maskp[x] == 40) dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
        else if (maskp[x] == 50) dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
        else if (maskp[x] == 70) dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
        else if (maskp[x] == 60) dstp[x] = efrmp[x];
      }
      prvp += prv_pitch;
      srcp += src_pitch;
      nxtp += nxt_pitch;
      maskp += mask_pitch;
      efrmp += efrm_pitch;
      dstp += dst_pitch;
    }
  }
}

void TDeinterlace::cubicDeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < 3; ++b)
  {
    const int plane = planes[b];
    const unsigned char *prvp = prv->GetReadPtr(plane);
    const int prv_pitch = prv->GetPitch(plane);
    const unsigned char *srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int src_pitch2 = src_pitch << 1;
    const int Width = src->GetRowSize(plane);
    const int Height = src->GetHeight(plane);
    const unsigned char *nxtp = nxt->GetReadPtr(plane);
    const int nxt_pitch = nxt->GetPitch(plane);
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
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
}

void TDeinterlace::ELADeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < 3; ++b)
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
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const unsigned char *srcpp = srcp - src_pitch;
    const unsigned char *srcpn = srcp + src_pitch;
    const int ustop = 8 >> vi.GetPlaneWidthSubsampling(plane); // YV12 specific fixed. b == 0 ? 8 : 4;
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
          else if (x < 2 || x > Width - 3 || (abs(srcpp[x] - srcpn[x]) < 10 &&
            abs(srcpp[x - 2] - srcpp[x + 2]) < 10 && abs(srcpn[x - 2] - srcpn[x + 2]) < 10))
          {
            dstp[x] = (srcpp[x] + srcpn[x] + 1) >> 1;
          }
          else
          {
            const int stop = min(x - 1, min(ustop, Width - 2 - x));
            const int minf = min(srcpp[x], srcpn[x]) - 2;
            const int maxf = max(srcpp[x], srcpn[x]) + 2;
            int val = (srcpp[x] + srcpn[x] + 1) >> 1;
            int min = 450;
            for (int u = 0; u <= stop; ++u)
            {
              {
                const int s1 = srcpp[x + (u >> 1)] + srcpp[x + ((u + 1) >> 1)];
                const int s2 = srcpn[x - (u >> 1)] + srcpn[x - ((u + 1) >> 1)];
                const int temp1 = abs(s1 - s2) + abs(srcpp[x - 1] - srcpn[x - 1 - u]) +
                  (abs(srcpp[x] - srcpn[x - u]) << 1) + abs(srcpp[x + 1] - srcpn[x + 1 - u]) +
                  abs(srcpn[x - 1] - srcpp[x - 1 + u]) + (abs(srcpn[x] - srcpp[x + u]) << 1) +
                  abs(srcpn[x + 1] - srcpp[x + 1 + u]);
                const int temp2 = (s1 + s2 + 2) >> 2;
                if (temp1 < min && temp2 >= minf && temp2 <= maxf)
                {
                  min = temp1;
                  val = temp2;
                }
              }
              {
                const int s1 = srcpp[x - (u >> 1)] + srcpp[x - ((u + 1) >> 1)];
                const int s2 = srcpn[x + (u >> 1)] + srcpn[x + ((u + 1) >> 1)];
                const int temp1 = abs(s1 - s2) + abs(srcpp[x - 1] - srcpn[x - 1 + u]) +
                  (abs(srcpp[x] - srcpn[x + u]) << 1) + abs(srcpp[x + 1] - srcpn[x + 1 + u]) +
                  abs(srcpn[x - 1] - srcpp[x - 1 - u]) + (abs(srcpn[x] - srcpp[x - u]) << 1) +
                  abs(srcpn[x + 1] - srcpp[x + 1 - u]);
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
}

void TDeinterlace::kernelDeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < 3; ++b)
  {
    const int plane = planes[b];
    const unsigned char *prvp = prv->GetReadPtr(plane);
    const int prv_pitch = prv->GetPitch(plane);
    const int prv_pitch2 = prv_pitch << 1;
    const unsigned char *srcp = src->GetReadPtr(plane);
    const int src_pitch = src->GetPitch(plane);
    const int src_pitch2 = src_pitch << 1;
    const int Width = src->GetRowSize(plane);
    const int Height = src->GetHeight(plane);
    const unsigned char *nxtp = nxt->GetReadPtr(plane);
    const int nxt_pitch = nxt->GetPitch(plane);
    const int nxt_pitch2 = nxt_pitch << 1;
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    const unsigned char *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
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
              0.031*(kerpp[x] + kernn[x])) + 0.5f);
            if (temp > 255) dstp[x] = 255; // fixme: 8 bit only
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
}

void TDeinterlace::smartELADeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const unsigned char *prvpY = prv->GetReadPtr(PLANAR_Y);
  const unsigned char *prvpV = prv->GetReadPtr(PLANAR_V);
  const unsigned char *prvpU = prv->GetReadPtr(PLANAR_U);
  const int prv_pitchY = prv->GetPitch(PLANAR_Y);
  const int prv_pitchUV = prv->GetPitch(PLANAR_V);
  const unsigned char *srcpY = src->GetReadPtr(PLANAR_Y);
  const unsigned char *srcpV = src->GetReadPtr(PLANAR_V);
  const unsigned char *srcpU = src->GetReadPtr(PLANAR_U);
  const int src_pitchY = src->GetPitch(PLANAR_Y);
  const int src_pitchY2 = src_pitchY << 1; // help for old compilers
  const int src_pitchUV = src->GetPitch(PLANAR_V);
  const int src_pitchUV2 = src_pitchUV << 1;
  const int WidthY = src->GetRowSize(PLANAR_Y);
  const int WidthUV = src->GetRowSize(PLANAR_V);
  const int HeightY = src->GetHeight(PLANAR_Y);
  const int HeightUV = src->GetHeight(PLANAR_V);
  const unsigned char *nxtpY = nxt->GetReadPtr(PLANAR_Y);
  const unsigned char *nxtpV = nxt->GetReadPtr(PLANAR_V);
  const unsigned char *nxtpU = nxt->GetReadPtr(PLANAR_U);
  const int nxt_pitchY = nxt->GetPitch(PLANAR_Y);
  const int nxt_pitchUV = nxt->GetPitch(PLANAR_V);
  unsigned char *dstpY = dst->GetWritePtr(PLANAR_Y);
  unsigned char *dstpV = dst->GetWritePtr(PLANAR_V);
  unsigned char *dstpU = dst->GetWritePtr(PLANAR_U);
  const int dst_pitchY = dst->GetPitch(PLANAR_Y);
  const int dst_pitchUV = dst->GetPitch(PLANAR_V);
  const unsigned char *maskpY = mask->GetReadPtr(PLANAR_Y);
  const unsigned char *maskpV = mask->GetReadPtr(PLANAR_V);
  const unsigned char *maskpU = mask->GetReadPtr(PLANAR_U);
  const int mask_pitchY = mask->GetPitch(PLANAR_Y);
  const int mask_pitchUV = mask->GetPitch(PLANAR_V);
  const unsigned char *srcppY = srcpY - src_pitchY;
  const unsigned char *srcpppY = srcppY - src_pitchY2;
  const unsigned char *srcpnY = srcpY + src_pitchY;
  const unsigned char *srcpnnY = srcpnY + src_pitchY2;
  const unsigned char *srcppV = srcpV - src_pitchUV;
  const unsigned char *srcpppV = srcppV - src_pitchUV2;
  const unsigned char *srcpnV = srcpV + src_pitchUV;
  const unsigned char *srcpnnV = srcpnV + src_pitchUV2;
  const unsigned char *srcppU = srcpU - src_pitchUV;
  const unsigned char *srcpppU = srcppU - src_pitchUV2;
  const unsigned char *srcpnU = srcpU + src_pitchUV;
  const unsigned char *srcpnnU = srcpnU + src_pitchUV2;
  for (int y = 0; y < HeightY; ++y)
  {
    for (int x = 0; x < WidthY; ++x)
    {
      if (maskpY[x] == 10) dstpY[x] = srcpY[x];
      else if (maskpY[x] == 20) dstpY[x] = prvpY[x];
      else if (maskpY[x] == 30) dstpY[x] = nxtpY[x];
      else if (maskpY[x] == 40) dstpY[x] = (srcpY[x] + nxtpY[x] + 1) >> 1;
      else if (maskpY[x] == 50) dstpY[x] = (srcpY[x] + prvpY[x] + 1) >> 1;
      else if (maskpY[x] == 70) dstpY[x] = (prvpY[x] + (srcpY[x] << 1) + nxtpY[x] + 2) >> 2;
      else if (maskpY[x] == 60)
      {
        if (y > 2 && y < HeightY - 3 && x>3 && x < WidthY - 4)
        {
          const int Iy1 = srcpppY[x - 1] + srcpppY[x] + srcpppY[x] + srcpppY[x + 1] - srcpnY[x - 1] - srcpnY[x] - srcpnY[x] - srcpnY[x + 1];
          const int Iy2 = srcppY[x - 1] + srcppY[x] + srcppY[x] + srcppY[x + 1] - srcpnnY[x - 1] - srcpnnY[x] - srcpnnY[x] - srcpnnY[x + 1];
          const int Ix1 = srcpppY[x + 1] + srcppY[x + 1] + srcppY[x + 1] + srcpnY[x + 1] - srcpppY[x - 1] - srcppY[x - 1] - srcppY[x - 1] - srcpnY[x - 1];
          const int Ix2 = srcppY[x + 1] + srcpnY[x + 1] + srcpnY[x + 1] + srcpnnY[x + 1] - srcppY[x - 1] - srcpnY[x - 1] - srcpnY[x - 1] - srcpnnY[x - 1];
          const int edgeS1 = Ix1*Ix1 + Iy1*Iy1;
          const int edgeS2 = Ix2*Ix2 + Iy2*Iy2;
          if (edgeS1 < 1600 && edgeS2 < 1600)
          {
            dstpY[x] = (srcppY[x] + srcpnY[x] + 1) >> 1;
            continue;
          }
          if (abs(srcppY[x] - srcpnY[x]) < 10 && (edgeS1 < 1600 || edgeS2 < 1600))
          {
            dstpY[x] = (srcppY[x] + srcpnY[x] + 1) >> 1;
            continue;
          }
          const int sum = srcppY[x - 1] + srcppY[x] + srcppY[x + 1] + srcpnY[x - 1] + srcpnY[x] + srcpnY[x + 1];
          const int sumsq = srcppY[x - 1] * srcppY[x - 1] + srcppY[x] * srcppY[x] + srcppY[x + 1] * srcppY[x + 1] +
            srcpnY[x - 1] * srcpnY[x - 1] + srcpnY[x] * srcpnY[x] + srcpnY[x + 1] * srcpnY[x + 1];
          if ((6 * sumsq - sum*sum) < 432)
          {
            dstpY[x] = (srcppY[x] + srcpnY[x] + 1) >> 1;
            continue;
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
              const int Iye = srcppY[x - 1] + srcppY[x] + srcppY[x] + srcppY[x + 1] - srcpnY[x - 1] - srcpnY[x] - srcpnY[x] - srcpnY[x + 1];
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
          double dirF = 0.5f / tan(dir); // fixme: float-double
          int temp, temp1, temp2;
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
                      temp1 = srcppY[x + 4]; // fixme: reuse temp1 and temp2
                      temp2 = srcpnY[x - 4];
                      temp = (srcppY[x + 4] + srcpnY[x - 4] + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcpnY[x];
                      temp = cubicInt(srcpppY[x], srcppY[x], srcpnY[x], srcpnnY[x]);
                    }
                  }
                  else
                  {
                    temp1 = (int)((dirF - 1.5f)*(srcppY[x + 4]) + (2.0f - dirF)*(srcppY[x + 3]) + 0.5f);
                    temp2 = (int)((dirF - 1.5f)*(srcpnY[x - 4]) + (2.0f - dirF)*(srcpnY[x - 3]) + 0.5f);
                    temp = (int)((dirF - 1.5f)*(srcppY[x + 4] + srcpnY[x - 4]) + (2.0f - dirF)*(srcppY[x + 3] + srcpnY[x - 3]) + 0.5f);
                  }
                }
                else
                {
                  temp1 = (int)((dirF - 1.0f)*(srcppY[x + 3]) + (1.5f - dirF)*(srcppY[x + 2]) + 0.5f);
                  temp2 = (int)((dirF - 1.0f)*(srcpnY[x - 3]) + (1.5f - dirF)*(srcpnY[x - 2]) + 0.5f);
                  temp = (int)((dirF - 1.0f)*(srcppY[x + 3] + srcpnY[x - 3]) + (1.5f - dirF)*(srcppY[x + 2] + srcpnY[x - 2]) + 0.5f);
                }
              }
              else
              {
                temp1 = (int)((dirF - 0.5f)*(srcppY[x + 2]) + (1.0f - dirF)*(srcppY[x + 1]) + 0.5f);
                temp2 = (int)((dirF - 0.5f)*(srcpnY[x - 2]) + (1.0f - dirF)*(srcpnY[x - 1]) + 0.5f);
                temp = (int)((dirF - 0.5f)*(srcppY[x + 2] + srcpnY[x - 2]) + (1.0f - dirF)*(srcppY[x + 1] + srcpnY[x - 1]) + 0.5f);
              }
            }
            else
            {
              temp1 = (int)(dirF*(srcppY[x + 1]) + (0.5f - dirF)*(srcppY[x]) + 0.5f);
              temp2 = (int)(dirF*(srcpnY[x - 1]) + (0.5f - dirF)*(srcpnY[x]) + 0.5f);
              temp = (int)(dirF*(srcppY[x + 1] + srcpnY[x - 1]) + (0.5f - dirF)*(srcppY[x] + srcpnY[x]) + 0.5f);
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
                      temp1 = srcppY[x - 4];
                      temp2 = srcpnY[x + 4];
                      temp = (srcppY[x - 4] + srcpnY[x + 4] + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcpnY[x];
                      temp = cubicInt(srcpppY[x], srcppY[x], srcpnY[x], srcpnnY[x]);
                    }
                  }
                  else
                  {
                    temp1 = (int)((-dirF - 1.5f)*(srcppY[x - 4]) + (2.0f + dirF)*(srcppY[x - 3]) + 0.5f);
                    temp2 = (int)((-dirF - 1.5f)*(srcpnY[x + 4]) + (2.0f + dirF)*(srcpnY[x + 3]) + 0.5f);
                    temp = (int)((-dirF - 1.5f)*(srcppY[x - 4] + srcpnY[x + 4]) + (2.0f + dirF)*(srcppY[x - 3] + srcpnY[x + 3]) + 0.5f);
                  }
                }
                else
                {
                  temp1 = (int)((-dirF - 1.0f)*(srcppY[x - 3]) + (1.5f + dirF)*(srcppY[x - 2]) + 0.5f);
                  temp2 = (int)((-dirF - 1.0f)*(srcpnY[x + 3]) + (1.5f + dirF)*(srcpnY[x + 2]) + 0.5f);
                  temp = (int)((-dirF - 1.0f)*(srcppY[x - 3] + srcpnY[x + 3]) + (1.5f + dirF)*(srcppY[x - 2] + srcpnY[x + 2]) + 0.5f);
                }
              }
              else
              {
                temp1 = (int)((-dirF - 0.5f)*(srcppY[x - 2]) + (1.0f + dirF)*(srcppY[x - 1]) + 0.5f);
                temp2 = (int)((-dirF - 0.5f)*(srcpnY[x + 2]) + (1.0f + dirF)*(srcpnY[x + 1]) + 0.5f);
                temp = (int)((-dirF - 0.5f)*(srcppY[x - 2] + srcpnY[x + 2]) + (1.0f + dirF)*(srcppY[x - 1] + srcpnY[x + 1]) + 0.5f);
              }
            }
            else
            {
              temp1 = (int)((-dirF)*(srcppY[x - 1]) + (0.5f + dirF)*(srcppY[x]) + 0.5f);
              temp2 = (int)((-dirF)*(srcpnY[x + 1]) + (0.5f + dirF)*(srcpnY[x]) + 0.5f);
              temp = (int)((-dirF)*(srcppY[x - 1] + srcpnY[x + 1]) + (0.5f + dirF)*(srcppY[x] + srcpnY[x]) + 0.5f);
            }
          }
          const int maxN = max(srcppY[x], srcpnY[x]) + 25;
          const int minN = min(srcppY[x], srcpnY[x]) - 25;
          if (abs(temp1 - temp2) > 20 || abs(srcppY[x] + srcpnY[x] - temp - temp) > 60 || temp < minN || temp > maxN)
          {
            temp = cubicInt(srcpppY[x], srcppY[x], srcpnY[x], srcpnnY[x]);
          }
          if (temp > 255) temp = 255; // FIXME: only 8 bits
          else if (temp < 0) temp = 0;
          dstpY[x] = temp;
        }
        else
        {
          if (y == 0) dstpY[x] = srcpnY[x];
          else if (y == HeightY - 1) dstpY[x] = srcppY[x];
          else if (y<3 || y>HeightY - 4) dstpY[x] = (srcpnY[x] + srcppY[x] + 1) >> 1;
          else dstpY[x] = cubicInt(srcpppY[x], srcppY[x], srcpnY[x], srcpnnY[x]);
        }
      }
    }
    prvpY += prv_pitchY;
    srcpppY += src_pitchY;
    srcppY += src_pitchY;
    srcpY += src_pitchY;
    srcpnY += src_pitchY;
    srcpnnY += src_pitchY;
    nxtpY += nxt_pitchY;
    maskpY += mask_pitchY;
    dstpY += dst_pitchY;
  }

  // chroma
  for (int y = 0; y < HeightUV; ++y)
  {
    for (int x = 0; x < WidthUV; ++x)
    {
      if (maskpV[x] == 10) dstpV[x] = srcpV[x];
      else if (maskpV[x] == 20) dstpV[x] = prvpV[x];
      else if (maskpV[x] == 30) dstpV[x] = nxtpV[x];
      else if (maskpV[x] == 40) dstpV[x] = (srcpV[x] + nxtpV[x] + 1) >> 1;
      else if (maskpV[x] == 50) dstpV[x] = (srcpV[x] + prvpV[x] + 1) >> 1;
      else if (maskpV[x] == 70) dstpV[x] = (prvpV[x] + (srcpV[x] << 1) + nxtpV[x] + 2) >> 2;
      else if (maskpV[x] == 60)
      {
        if (y == 0) dstpV[x] = srcpnV[x];
        else if (y == HeightUV - 1) dstpV[x] = srcppV[x];
        else if (y<3 || y>HeightUV - 4) dstpV[x] = (srcpnV[x] + srcppV[x] + 1) >> 1;
        else dstpV[x] = cubicInt(srcpppV[x], srcppV[x], srcpnV[x], srcpnnV[x]);
      }
      if (maskpU[x] == 10) dstpU[x] = srcpU[x];
      else if (maskpU[x] == 20) dstpU[x] = prvpU[x];
      else if (maskpU[x] == 30) dstpU[x] = nxtpU[x];
      else if (maskpU[x] == 40) dstpU[x] = (srcpU[x] + nxtpU[x] + 1) >> 1;
      else if (maskpU[x] == 50) dstpU[x] = (srcpU[x] + prvpU[x] + 1) >> 1;
      else if (maskpU[x] == 70) dstpU[x] = (prvpU[x] + (srcpU[x] << 1) + nxtpU[x] + 2) >> 2;
      else if (maskpU[x] == 60)
      {
        if (y == 0) dstpU[x] = srcpnU[x];
        else if (y == HeightUV - 1) dstpU[x] = srcppU[x];
        else if (y<3 || y>HeightUV - 4) dstpU[x] = (srcpnU[x] + srcppU[x] + 1) >> 1;
        else dstpU[x] = cubicInt(srcpppU[x], srcppU[x], srcpnU[x], srcpnnU[x]);
      }
    }
    prvpV += prv_pitchUV;
    prvpU += prv_pitchUV;
    srcpppV += src_pitchUV;
    srcppV += src_pitchUV;
    srcpV += src_pitchUV;
    srcpnV += src_pitchUV;
    srcpnnV += src_pitchUV;
    srcpppU += src_pitchUV;
    srcppU += src_pitchUV;
    srcpU += src_pitchUV;
    srcpnU += src_pitchUV;
    srcpnnU += src_pitchUV;
    nxtpV += nxt_pitchUV;
    nxtpU += nxt_pitchUV;
    maskpV += mask_pitchUV;
    maskpU += mask_pitchUV;
    dstpV += dst_pitchUV;
    dstpU += dst_pitchUV;
  }
}

void TDeinterlace::createWeaveFramePlanar(PVideoFrame &dst, PVideoFrame &prv,
  PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env)
{
  // planar and hbd compatible fixme: getting planes in a usual manner
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < 3; ++b)
  {
    const int plane = planes[b];;
    if (field^order)
    {
      if (rmatch == 0)
      {
        env->BitBlt(dst->GetWritePtr(plane), dst->GetPitch(plane), src->GetReadPtr(plane),
          src->GetPitch(plane), src->GetRowSize(plane), src->GetHeight(plane));
      }
      else
      {
        env->BitBlt(dst->GetWritePtr(plane) + (1 - field)*dst->GetPitch(plane), dst->GetPitch(plane) << 1,
          src->GetReadPtr(plane) + (1 - field)*src->GetPitch(plane), src->GetPitch(plane) << 1,
          src->GetRowSize(plane), src->GetHeight(plane) >> 1);
        env->BitBlt(dst->GetWritePtr(plane) + field*dst->GetPitch(plane), dst->GetPitch(plane) << 1,
          nxt->GetReadPtr(plane) + field*nxt->GetPitch(plane), nxt->GetPitch(plane) << 1, nxt->GetRowSize(plane),
          nxt->GetHeight(plane) >> 1);
      }
    }
    else
    {
      if (rmatch == 1)
      {
        env->BitBlt(dst->GetWritePtr(plane), dst->GetPitch(plane), src->GetReadPtr(plane),
          src->GetPitch(plane), src->GetRowSize(plane), src->GetHeight(plane));
      }
      else
      {
        env->BitBlt(dst->GetWritePtr(plane) + (1 - field)*dst->GetPitch(plane), dst->GetPitch(plane) << 1,
          src->GetReadPtr(plane) + (1 - field)*src->GetPitch(plane), src->GetPitch(plane) << 1,
          src->GetRowSize(plane), src->GetHeight(plane) >> 1);
        env->BitBlt(dst->GetWritePtr(plane) + field*dst->GetPitch(plane), dst->GetPitch(plane) << 1,
          prv->GetReadPtr(plane) + field*prv->GetPitch(plane), prv->GetPitch(plane) << 1, prv->GetRowSize(plane),
          prv->GetHeight(plane) >> 1);
      }
    }
  }
}