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
#include <cassert>

PVideoFrame TDeinterlace::GetFramePlanar(int n, IScriptEnvironment* env, bool &wdtd)
{
  const int bits_per_pixel = vi.BitsPerComponent();

  int n_saved = n;
  if (mode < 0)
  {
    PVideoFrame src2up = child->GetFrame(n, env);
    PVideoFrame dst2up = // frame property support
      has_at_least_v8 ? env->NewVideoFrameP(vi_saved, &src2up) : env->NewVideoFrame(vi_saved);

    PVideoFrame msk2up = env->NewVideoFrame(vi_mask);
    copyForUpsize(dst2up, src2up, vi_saved, env);
    setMaskForUpsize(msk2up, vi_mask);
    if (mode == -2) 
      dispatch_smartELADeintPlanar(dst2up, msk2up, dst2up, dst2up, dst2up, vi_saved);
    else if (mode == -1) 
      dispatch_ELADeintPlanar(dst2up, msk2up, dst2up, dst2up, dst2up, vi_saved);
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
    if (field_origSaved == -1) field = order;
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
      if (field_origSaved != -1) field = field_origSaved;
      if (!autoFO) order = order_origSaved;
    }
    mthreshL = mthreshL_origSaved;
    mthreshC = mthreshC_origSaved;
    type = type_origSaved;
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
  if (mode == 0 && hints && TDeinterlace::getHint(vi, src, passHint, hintField) == 0 && !found)
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
    if (!dispatch_checkCombedPlanar(src, MIC, vi_saved, chroma, cthresh, env))
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
    const int bits_per_pixel = vi.BitsPerComponent();
    if(bits_per_pixel == 8)
      subtractFields<uint8_t>(prv, src, nxt, vi_mask, accumPn, accumNn, accumPm, accumNm,
        field, order, false, slow, env);
    else if(bits_per_pixel <= 16)
      subtractFields<uint16_t>(prv, src, nxt, vi_mask, accumPn, accumNn, accumPm, accumNm,
        field, order, false, slow, env);
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
    createWeaveFrame(dst, prv, src, nxt, env); // no spc hbd
    int MIC;
    if (!dispatch_checkCombedPlanar(dst, MIC, vi_saved, chroma, cthresh, env))
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
  mask = env->NewVideoFrame(vi_mask);
  if (emask) mask = emask->GetFrame(n_saved, env);
  else
  {
    if (mthreshL <= 0 && mthreshC <= 0) setMaskForUpsize(mask, vi_mask);
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
    if (denoise)
      denoisePlanar(mask);
    if (expand > 0) {
      if (vi_saved.Is420())
        expandMap_Planar<420>(mask);
      else if (vi_saved.Is422())
        expandMap_Planar<422>(mask);
      else if (vi_saved.Is444() || vi_saved.IsY())
        expandMap_Planar<444>(mask);
      else if (vi_saved.IsYV411())
        expandMap_Planar<411>(mask);
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
      else if (vi_saved.IsYV411())
        linkFULL_Planar<411>(mask);
      else if (vi_saved.IsY())
        ; // do nothing
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
      else if (vi_saved.IsYV411())
        linkYtoUV_Planar<411>(mask);
      else if (vi_saved.IsY())
        ; // do nothing
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
      else if (vi_saved.IsYV411())
        linkUVtoY_Planar<411>(mask);
      else if (vi_saved.IsY())
        ; // do nothing
      else {
        env->ThrowError("TDeint: unsupported video format");
      }
    }
    else if (link != 0) env->ThrowError("TDeint:  an unknown error occured (link)!");
  }
  PVideoFrame efrm = NULL;
  if (edeint) 
    efrm = edeint->GetFrame(n_saved, env);
  // dmap is of original bit depth
  PVideoFrame dmap = map ? env->NewVideoFrame(vi_saved) : NULL;
  if (map == 1 || map == 3) 
    dispatch_mapColorsPlanar(dmap, mask, vi);
  else if (map == 2 || map == 4) 
    dispatch_mapMergePlanar(dmap, mask, prv, src, nxt, vi);
  
  const bool uap = (AP >= 0 && AP < 255) ? true : false;
  if (map == 0 || uap || map > 2)
  {

    if (edeint) dispatch_eDeintPlanar(dst, mask, prv, src, nxt, efrm, vi);
    else if (type == 0) dispatch_cubicDeintPlanar(dst, mask, prv, src, nxt, vi);
    else if (type == 1) dispatch_smartELADeintPlanar(dst, mask, prv, src, nxt, vi);
    else if (type == 2) dispatch_kernelDeintPlanar(dst, mask, prv, src, nxt, vi);
    else if (type == 3) dispatch_ELADeintPlanar(dst, mask, prv, src, nxt, vi);
    else if (type == 4) dispatch_blendDeint(dst, mask, prv, src, nxt, vi, env);
    else if (type == 5) dispatch_blendDeint2(dst, mask, prv, src, nxt, vi, env);
    else env->ThrowError("TDeint:  an unknown error occured!");
  }
  else
  {
    if (hintField >= 0 && !fieldOVR) field = hintField;
    return dmap;
  }
  if (uap)
  {
    if(bits_per_pixel == 8)
      apPostCheck<uint8_t>(dst, mask, efrm, env);
    else
      apPostCheck<uint16_t>(dst, mask, efrm, env);

    if (map) {
      if (bits_per_pixel == 8)
        updateMapAP<uint8_t>(dmap, mask, env);
      else
        updateMapAP<uint16_t>(dmap, mask, env);
    }
    if (map > 0 && map < 3)
    {
      if (hintField >= 0 && !fieldOVR) field = hintField;
      return dmap;
    }
  }
  if (map != 1 && map != 2)
    TDeinterlace::putHint(vi, dst, passHint, field);
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
    PVideoFrame dst2 = has_at_least_v8 ? env->NewVideoFrameP(vi, &dst) : env->NewVideoFrame(vi);
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
  // these are hbd ready
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
  // from now on only mask is handled, no hbd stuff here
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const int dpitch = db->GetPitch(b) << 1;
    const int dpitchl = db->GetPitch(b);
    const int Height = db->GetHeight(b);
    const int Width = db->GetWidth(b);
    const uint8_t *d1p = db->GetReadPtr(db->GetPos(0), b) + dpitchl*field;
    const uint8_t *d2p = db->GetReadPtr(db->GetPos(1), b) + dpitchl*field;
    const uint8_t *d3p;
    if (mode == 0) d3p = db->GetReadPtr(db->GetPos(2), b) + dpitchl*field;
    else d3p = db->GetReadPtr(db->GetPos(field^order ? 2 : 3), b) + dpitchl*field;
    uint8_t *maskw = mask->GetWritePtr(plane);
    const int mask_pitch = mask->GetPitch(plane) << 1;
    memset(maskw, 10, (mask_pitch >> 1)*Height);
    maskw += (mask_pitch >> 1)*field;
    const uint8_t *d1pn = d1p + dpitchl;
    const uint8_t *d2pn = d2p + dpitchl;
    const uint8_t *d1pp = field ? d1p - dpitchl : d1pn;
    const uint8_t *d2pp = field ? d2p - dpitchl : d2pn;
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

// HBD ready
void TDeinterlace::createMotionMap5_PlanarOrYUY2(PVideoFrame &prv2, PVideoFrame &prv,
  PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &nxt2, PVideoFrame &mask,
  int n, bool isYUY2, IScriptEnvironment *env)
{
  db->resetCacheStart(n - 1);
  // insertDiff is HBD ready. DB is 8 bits
  InsertDiff(prv2, prv, n - 1, db->GetPos(0), env);
  InsertDiff(prv, src, n, db->GetPos(1), env);
  InsertDiff(src, nxt, n + 1, db->GetPos(2), env);
  InsertDiff(nxt, nxt2, n + 2, db->GetPos(3), env);
  InsertDiff(prv2, src, -n - 2, db->GetPos(4), env);
  InsertDiff(prv, nxt, -n - 3, db->GetPos(5), env);
  InsertDiff(src, nxt2, -n - 4, db->GetPos(6), env);
  int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const uint8_t *dpp[7], *dp[7], *dpn[7];
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  for (int b = 0; b < np; ++b)
  {
    //db is 8 bit format
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
    uint8_t *maskw = mask->GetWritePtr(plane[b]);
    const int mask_pitch = mask->GetPitch(plane[b]) << 1;
    memset(maskw, 10, (mask_pitch >> 1)*Height); // initialize masks to 10
    maskw += (mask_pitch >> 1)*field;
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

// mask-only no need HBD here
template<int planarType>
void TDeinterlace::expandMap_Planar(PVideoFrame &mask)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    uint8_t *maskp = mask->GetWritePtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const int mask_pitch2 = mask_pitch << 1;
    const int Height = mask->GetHeight(plane);
    const int Width = mask->GetRowSize(plane);
    const int dis = 
      b == 0 ? 
      expand : // luma
      (expand >> (planarType == 444 ? 0 : planarType == 411 ? 2 : 1 /* 422, 420 */)); // chroma

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

// mask-only no need HBD here
template<int planarType>
void TDeinterlace::linkFULL_Planar(PVideoFrame &mask)
{
  uint8_t *maskpY = mask->GetWritePtr(PLANAR_Y);
  uint8_t *maskpV = mask->GetWritePtr(PLANAR_V);
  uint8_t *maskpU = mask->GetWritePtr(PLANAR_U);
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
    uint8_t* maskpnY = maskpY + mask_pitchY2;
    for (int y = field; y < HeightUV; y += 2)
    {
      for (int x = 0; x < WidthUV; ++x)
      {
        if (((((uint16_t*)maskpY)[x] == (uint16_t)0x3C3C) &&
          (((uint16_t*)maskpnY)[x] == (uint16_t)0x3C3C)) ||
          maskpV[x] == 0x3C || maskpU[x] == 0x3C)
        {
          ((uint16_t*)maskpY)[x] = (uint16_t)0x3C3C;
          ((uint16_t*)maskpnY)[x] = (uint16_t)0x3C3C;
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
    // 411, 422, 444
    for (int y = field; y < HeightUV; y+=2) // always by 2
    {
      for (int x = 0; x < WidthUV; ++x)
      {
        if constexpr (planarType == 422) {
          if (
            ((uint16_t*)(maskpY))[x] == (uint16_t)0x3C3C
            || maskpV[x] == 0x3C || maskpU[x] == 0x3C)
          {
            ((uint16_t*)maskpY)[x] = (uint16_t)0x3C3C;
            maskpV[x] = maskpU[x] = 0x3C;
          }
        }
        else if constexpr (planarType == 444) {
          if (
            (maskpY[x] == 0x3C) ||
            maskpV[x] == 0x3C || maskpU[x] == 0x3C)
          {
            maskpY[x] = 0x3C;
            maskpV[x] = maskpU[x] = 0x3C;
          }
        }
        else if constexpr (planarType == 411) {
          if (
            ((uint32_t *)(maskpY))[x] == (uint32_t)0x3C3C3C3C
            || maskpV[x] == 0x3C || maskpU[x] == 0x3C)
          {
            ((uint32_t*)maskpY)[x] = (uint32_t)0x3C3C3C3C; // was: 0x3C3C fixed after 1.3
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

// mask-only no need HBD here
template<int planarType>
void TDeinterlace::linkYtoUV_Planar(PVideoFrame &mask)
{
  uint8_t *maskpY = mask->GetWritePtr(PLANAR_Y);
  uint8_t *maskpV = mask->GetWritePtr(PLANAR_V);
  uint8_t *maskpU = mask->GetWritePtr(PLANAR_U);
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
    uint8_t* maskpnY = maskpY + mask_pitchY2;
    for (int y = field; y < HeightUV; y += 2)
    {
      for (int x = 0; x < WidthUV; ++x)
      {
        if (((uint16_t*)maskpY)[x] == (uint16_t)0x3C3C &&
          ((uint16_t*)maskpnY)[x] == (uint16_t)0x3C3C)
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
    // 422, 444, 411
    for (int y = field; y < HeightUV; y+=2) // always by 2
    {
      for (int x = 0; x < WidthUV; ++x)
      {
        if constexpr (planarType == 422) {
          if (((uint16_t*)maskpY)[x] == (uint16_t)0x3C3C)
          {
            maskpV[x] = maskpU[x] = 0x3C;
          }
        }
        else if constexpr (planarType == 444) {
          if (maskpY[x] == 0x3C)
          {
            maskpV[x] = maskpU[x] = 0x3C;
          }
        }
        else if constexpr (planarType == 411) {
          if (((uint32_t*)maskpY)[x] == (uint32_t)0x3C3C3C3C)
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

// mask-only no need HBD here
template<int planarType>
void TDeinterlace::linkUVtoY_Planar(PVideoFrame& mask)
{
  uint8_t* maskpY = mask->GetWritePtr(PLANAR_Y);
  uint8_t* maskpV = mask->GetWritePtr(PLANAR_V);
  uint8_t* maskpU = mask->GetWritePtr(PLANAR_U);
  const int mask_pitchY = mask->GetPitch(PLANAR_Y);
  const int mask_pitchY2 = mask_pitchY << 1;
  const int mask_pitchY4 = mask_pitchY << 2;
  const int mask_pitchUV = mask->GetPitch(PLANAR_V);
  const int mask_pitchUV2 = mask_pitchUV << 1;
  const int HeightUV = mask->GetHeight(PLANAR_V);
  const int WidthUV = mask->GetRowSize(PLANAR_V);
  maskpY += mask_pitchY * field;
  maskpV += mask_pitchUV * field;
  maskpU += mask_pitchUV * field;

  // only used at 420:
  uint8_t* maskpnY = maskpY + mask_pitchY2;
  for (int y = field; y < HeightUV; y += 2)
  {
    for (int x = 0; x < WidthUV; ++x)
    {
      if (maskpV[x] == 0x3C || maskpU[x] == 0x3C)
      {
        if constexpr (planarType == 420) {
          // fill Y: 2x2 
          ((uint16_t*)maskpY)[x] = (uint16_t)0x3C3C;
          ((uint16_t*)maskpnY)[x] = (uint16_t)0x3C3C;
        }
        else if constexpr (planarType == 422) {
          ((uint16_t*)maskpY)[x] = (uint16_t)0x3C3C;
        }
        else if constexpr (planarType == 411) { // was missing, fixed after 1.3
          ((uint32_t*)maskpY)[x * 4] = (uint32_t)0x3C3C3C3C;
        }
        else if constexpr (planarType == 444) {
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

// mask-only no need HBD here
// Differences
// TFMPP::denoisePlanar: PlanarFrame, 0xFF, TDeinterlace:PVideoFrame 0x3C
void TDeinterlace::denoisePlanar(PVideoFrame &mask)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    uint8_t *maskp = mask->GetWritePtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const int mask_pitch2 = mask_pitch << 1;
    const int Height = mask->GetHeight(plane);
    const int Width = mask->GetRowSize(plane);
    maskp += mask_pitch*(2 + field);
    uint8_t *maskpp = maskp - mask_pitch2;
    uint8_t *maskpn = maskp + mask_pitch2;
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

// similar in tfm but with PlanarFrame
template<int planarType>
void FillCombedPlanarUpdateCmaskByUV(PVideoFrame& cmask)
{
  uint8_t* cmkp = cmask->GetWritePtr(PLANAR_Y);
  uint8_t* cmkpU = cmask->GetWritePtr(PLANAR_U);
  uint8_t* cmkpV = cmask->GetWritePtr(PLANAR_V);
  const int Width = cmask->GetRowSize(PLANAR_V); // chroma!
  const int Height = cmask->GetHeight(PLANAR_V);
  const ptrdiff_t cmk_pitch = cmask->GetPitch(PLANAR_Y);
  const ptrdiff_t cmk_pitchUV = cmask->GetPitch(PLANAR_V);
  do_FillCombedPlanarUpdateCmaskByUV<planarType>(cmkp, cmkpU, cmkpV, Width, Height, cmk_pitch, cmk_pitchUV);
}

bool TDeinterlace::dispatch_checkCombedPlanar(PVideoFrame& src, int& MIC, const VideoInfo &vi, bool chroma, int cthresh, IScriptEnvironment* env)
{
  const int bits_per_pixel = vi.BitsPerComponent();
    switch (bits_per_pixel) {
    case 8: return checkCombedPlanar<uint8_t>(src, MIC, bits_per_pixel, chroma, cthresh, env); break;
    case 10:
    case 12:
    case 14:
    case 16: return checkCombedPlanar<uint16_t>(src, MIC, bits_per_pixel, chroma, cthresh, env); break;
    default: return false; // n/a
    }
}

template<typename pixel_t>
static void do_checkCombedPlanar(PVideoFrame &src, int &MIC, int bits_per_pixel, bool chroma, int cthresh, 
  PVideoFrame &cmask,
  int cpuFlags, const VideoInfo &vi_saved, int metric, IScriptEnvironment *env)
{
  // vi_saved: original vi, not a possible stacked one (some modes change height for debug)
  const bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;
  const bool use_sse4 = (cpuFlags & CPUF_SSE4_1) ? true : false;
  // cthresh: Area combing threshold used for combed frame detection.
  // This essentially controls how "strong" or "visible" combing must be to be detected.
  // Good values are from 6 to 12. If you know your source has a lot of combed frames set 
  // this towards the low end(6 - 7). If you know your source has very few combed frames set 
  // this higher(10 - 12). Going much lower than 5 to 6 or much higher than 12 is not recommended.

  const int scaled_cthresh = cthresh << (bits_per_pixel - 8);

  const int cthresh6 = scaled_cthresh * 6;

  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int np = vi_saved.IsYUY2() || vi_saved.IsY() ? 1 : 3;
  const int stop = chroma ? np : 1;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];

    const pixel_t *srcp = reinterpret_cast<const pixel_t *>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);

    const int Width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = src->GetHeight(plane);

    const pixel_t* srcpp = srcp - src_pitch;
    const pixel_t* srcppp = srcpp - src_pitch;
    const pixel_t* srcpn = srcp + src_pitch;
    const pixel_t* srcpnn = srcpn + src_pitch;

    uint8_t *cmkp = cmask->GetWritePtr(plane);
    const int cmk_pitch = cmask->GetPitch(plane);
    
    if (scaled_cthresh < 0) {
      memset(cmkp, 255, Height*cmk_pitch); // mask. Always 8 bits 
      continue;
    }
    memset(cmkp, 0, Height*cmk_pitch);

    if (metric == 0)
    {
      // top 1 
      for (int x = 0; x < Width; ++x)
      {
        const int sFirst = srcp[x] - srcpn[x];
        if (sFirst > scaled_cthresh || sFirst < -scaled_cthresh)
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
        if ((sFirst > scaled_cthresh && sSecond > scaled_cthresh) || (sFirst < -scaled_cthresh && sSecond < -scaled_cthresh))
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
      const int lines_to_process = Height - 4;
      if (use_sse2 && sizeof(pixel_t) == 1)
        check_combing_SSE2((const uint8_t*)srcp, cmkp, Width, lines_to_process, src_pitch, cmk_pitch, scaled_cthresh);
      else if (use_sse4 && sizeof(pixel_t) == 2)
        check_combing_uint16_SSE4((const uint16_t *)srcp, cmkp, Width, lines_to_process, src_pitch, cmk_pitch, scaled_cthresh);
      else
        check_combing_c<pixel_t, false>(srcp, cmkp, Width, lines_to_process, src_pitch, cmk_pitch, scaled_cthresh);
      srcppp += src_pitch * lines_to_process;
      srcpp += src_pitch * lines_to_process;
      srcp += src_pitch * lines_to_process;
      srcpn += src_pitch * lines_to_process;
      srcpnn += src_pitch * lines_to_process;
      cmkp += cmk_pitch * lines_to_process;
      // bottom #-2
      for (int x = 0; x < Width; ++x)
      {
        const int sFirst = srcp[x] - srcpp[x];
        const int sSecond = srcp[x] - srcpn[x];
        if ((sFirst > scaled_cthresh && sSecond > scaled_cthresh) || (sFirst < -scaled_cthresh && sSecond < -scaled_cthresh))
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
        if (sFirst > scaled_cthresh || sFirst < -scaled_cthresh)
        {
          if (abs(srcppp[x] + (srcp[x] << 2) + srcppp[x] - (3 * (srcpp[x] + srcpp[x]))) > cthresh6)
            cmkp[x] = 0xFF;
        }
      }
    }
    else
    {
      // metric == 1: squared
      typedef typename std::conditional<sizeof(pixel_t) == 1, int, int64_t> ::type safeint_t;

      const safeint_t cthreshsq = (safeint_t)scaled_cthresh * scaled_cthresh;
      // top #1
      for (int x = 0; x < Width; ++x)
      {
        if ((safeint_t)(srcp[x] - srcpn[x])*(srcp[x] - srcpn[x]) > cthreshsq)
          cmkp[x] = 0xFF;
      }
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      cmkp += cmk_pitch;
      // middle Height - 2
      const int lines_to_process = Height - 2;
      if (use_sse2)
      {
        if constexpr(sizeof(pixel_t) == 1)
          check_combing_SSE2_Metric1(srcp, cmkp, Width, lines_to_process, src_pitch, cmk_pitch, cthreshsq);
        else
          check_combing_c_Metric1<pixel_t, false, safeint_t>(srcp, cmkp, Width, lines_to_process, src_pitch, cmk_pitch, cthreshsq);
        // fixme: hbd SIMD? int64 inside.
        // check_combing_uint16_SSE2_Metric1(srcp, cmkp, Width, lines_to_process, src_pitch, cmk_pitch, cthreshsq);
      }
      else
      {
        check_combing_c_Metric1<pixel_t, false, safeint_t>(srcp, cmkp, Width, lines_to_process, src_pitch, cmk_pitch, cthreshsq);
      }
      srcpp += src_pitch * lines_to_process;
      srcp += src_pitch * lines_to_process;
      srcpn += src_pitch * lines_to_process;
      cmkp += cmk_pitch * lines_to_process;
      // Bottom
      for (int x = 0; x < Width; ++x)
      {
        if ((safeint_t)(srcp[x] - srcpp[x])*(srcp[x] - srcpp[x]) > cthreshsq)
          cmkp[x] = 0xFF;
      }
    }
  }

  // Includes chroma combing in the decision about whether a frame is combed.
  if (chroma)
  {
    if (vi_saved.Is420()) FillCombedPlanarUpdateCmaskByUV<420>(cmask);
    else if (vi_saved.Is422()) FillCombedPlanarUpdateCmaskByUV<422>(cmask);
    else if (vi_saved.Is444()) FillCombedPlanarUpdateCmaskByUV<444>(cmask);
    else if (vi_saved.IsYV411()) FillCombedPlanarUpdateCmaskByUV<411>(cmask);
  }
  // end of chroma handling

}

template<typename pixel_t>
bool TDeinterlace::checkCombedPlanar(PVideoFrame& src, int& MIC, int bits_per_pixel, bool chroma, int cthresh, IScriptEnvironment* env)
{
  PVideoFrame cmask = env->NewVideoFrame(vi_mask);

  do_checkCombedPlanar<pixel_t>(src, MIC, bits_per_pixel, chroma, cthresh, cmask, cpuFlags, vi_saved, metric, env);

  // from now on, only mask is used, no hbd stuff here

  const int cmk_pitch = cmask->GetPitch(PLANAR_Y);
  const uint8_t* cmkp = cmask->GetReadPtr(PLANAR_Y) + cmk_pitch;
  const int Width = cmask->GetRowSize(PLANAR_Y);
  const int Height = cmask->GetHeight(PLANAR_Y);

  const uint8_t* cmkpp = cmkp - cmk_pitch;
  const uint8_t* cmkpn = cmkp + cmk_pitch;

  const int xblocks = ((Width + blockx_half) >> blockx_shift) + 1;
  const int xblocks4 = xblocks << 2;
  const int yblocks = ((Height + blocky_half) >> blocky_shift) + 1;
  const int arraysize = (xblocks * yblocks) << 2;
  memset(cArray, 0, arraysize * sizeof(int));
  int Heighta = (Height >> (blocky_shift - 1)) << (blocky_shift - 1);
  if (Heighta == Height) Heighta = Height - blocky_half;
  const int Widtha = (Width >> (blockx_shift - 1)) << (blockx_shift - 1);

  const bool use_sse2 = cpuFlags & CPUF_SSE2;
  // quick case for 8x8 base
  const bool use_sse2_8x8_sum = (use_sse2 && blockx_half == 8 && blocky_half == 8) ? true : false;

  // top y block
  for (int y = 1; y < blocky_half; ++y)
  {
    const int temp1 = (y >> blocky_shift) * xblocks4;
    const int temp2 = ((y + blocky_half) >> blocky_shift) * xblocks4;
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
    const int temp1 = (y >> blocky_shift) * xblocks4;
    const int temp2 = ((y + blocky_half) >> blocky_shift) * xblocks4;
    // fixme: do it probably for other block sizes than 8x8 and dispatch earlier
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
        int sum = 0;
        // fixme: put into compute_sum_8xN_C
        const uint8_t* cmkppT = cmkpp;
        const uint8_t* cmkpT = cmkp;
        const uint8_t* cmkpnT = cmkpn;
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
      const uint8_t* cmkppT = cmkpp;
      const uint8_t* cmkpT = cmkp;
      const uint8_t* cmkpnT = cmkpn;
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
    cmkpp += cmk_pitch * blocky_half;
    cmkp += cmk_pitch * blocky_half;
    cmkpn += cmk_pitch * blocky_half;
  }
  // rest non-whole blocky at the bottom
  for (int y = Heighta; y < Height - 1; ++y)
  {
    const int temp1 = (y >> blocky_shift) * xblocks4;
    const int temp2 = ((y + blocky_half) >> blocky_shift) * xblocks4;
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
  // min setting = 0, max setting = blockx x blocky (at which point no frames will ever be detected as combed).
  // default - 64 (int)

  if (MIC > MI) return true;
  return false;

}


template<typename pixel_t>
void TDeinterlace::buildDiffMapPlane2(const uint8_t* prvp, const uint8_t* nxtp,
  uint8_t* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int bits_per_pixel, IScriptEnvironment* env)
{
  const bool YUY2_LumaOnly = false; // no mChroma like in TFM
  do_buildABSDiffMask2<pixel_t>(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch, Width, Height, YUY2_LumaOnly, cpuFlags, bits_per_pixel);
}

// instantiate
template void TDeinterlace::buildDiffMapPlane2<uint8_t>(const uint8_t* prvp, const uint8_t* nxtp,
  uint8_t* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int bits_per_pixel, IScriptEnvironment* env);
template void TDeinterlace::buildDiffMapPlane2<uint16_t>(const uint8_t* prvp, const uint8_t* nxtp,
  uint8_t* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height,
  int Width, int bits_per_pixel, IScriptEnvironment* env);

// common planar YUY2
template<typename pixel_t>
void TDeinterlace::subtractFields(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
  VideoInfo &vi_map, int &aPn, int &aNn, int &aPm, int &aNm, int fieldt, int ordert,
  bool d2, int _slow, IScriptEnvironment *env)
{
  const int bits_per_pixel = vi.BitsPerComponent(); // not of the mask, mask is 8 bits always

  if (_slow == 1)
    return subtractFields1<pixel_t>(prv, src, nxt, vi_map,
      aPn, aNn, aPm, aNm,
      fieldt, ordert, d2, bits_per_pixel, env);
  else if (_slow == 2)
    return subtractFields2<pixel_t>(prv, src, nxt, vi_map,
      aPn, aNn, aPm, aNm,
      fieldt, ordert, d2, bits_per_pixel, env);

  PVideoFrame map = env->NewVideoFrame(vi_map);
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };

  uint64_t accumPns = 0, accumNns = 0;
  uint64_t accumPms = 0, accumNms = 0;
  
  aPn = aNn = 0;
  aPm = aNm = 0;
  
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    uint8_t *mapp = map->GetWritePtr(plane);
    const int map_pitch = map->GetPitch(plane) << 1;

    const pixel_t *prvp = reinterpret_cast<const pixel_t *>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);

    const pixel_t *srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    
    const int Width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = src->GetHeight(plane);

    const pixel_t *nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);

    const int startx = vi_map.IsYUY2() ? 16 : 8 >> vi_map.GetPlaneWidthSubsampling(plane);
    const int stopx = Width - startx;

    const pixel_t *prvpf, *curf, *nxtpf;
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

    const pixel_t* prvnf = prvpf + prvf_pitch;
    const pixel_t* curpf = curf - curf_pitch;
    const pixel_t* curnf = curf + curf_pitch;
    const pixel_t* nxtnf = nxtpf + nxtf_pitch;
    uint8_t *mapn = mapp + map_pitch;
    
    // back to byte pointers
    if (fieldt != 1)
      buildDiffMapPlane2<pixel_t>(
        reinterpret_cast<const uint8_t *>(prvpf - prvf_pitch), 
        reinterpret_cast<const uint8_t *>(nxtpf - nxtf_pitch),
        mapp - map_pitch,
        prvf_pitch * sizeof(pixel_t),
        nxtf_pitch * sizeof(pixel_t),
        map_pitch, Height >> 1, Width, bits_per_pixel, env);
    else
      buildDiffMapPlane2<pixel_t>(
        reinterpret_cast<const uint8_t*>(prvnf - prvf_pitch),
        reinterpret_cast<const uint8_t*>(nxtnf - nxtf_pitch),
        mapn - map_pitch, 
        prvf_pitch * sizeof(pixel_t),
        nxtf_pitch * sizeof(pixel_t),
        map_pitch, Height >> 1, Width, bits_per_pixel, env);

    const int Const23 = 23 << (bits_per_pixel - 8);
    const int Const42 = 42 << (bits_per_pixel - 8);

    for (int y = 2; y < Height - 2; y += 2)
    {
      for (int x = startx; x < stopx; x++)
      {
        int map_flag = (mapp[x] << 2) + mapn[x];
        if (map_flag == 0)
          continue;

        int cur_sum = curpf[x] + (curf[x] << 2) + curnf[x];

        int diff_p = abs((prvpf[x] + prvnf[x]) * 3 - cur_sum);
        if (diff_p > Const23) {
          accumPns += diff_p;
          if (diff_p > Const42)
          {
            if ((map_flag & 10) != 0)
              accumPms += diff_p;
          }
        }

        int diff_n = abs((nxtpf[x] + nxtnf[x]) * 3 - cur_sum);
        if (diff_n > Const23) {
          accumNns += diff_n;
          if (diff_n > Const42) {
            if ((map_flag & 10) != 0)
              accumNms += diff_n;
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
    }
  }
  // High bit depth: I chose to scale back to 8 bit range.
  // Or else we should threat them as int64 and act upon them outside
  const double factor = 1.0 / (1 << (bits_per_pixel - 8));
  aPn = int(accumPns / 6.0 * factor + 0.5);
  aNn = int(accumNns / 6.0 * factor + 0.5);
  aPm = int(accumPms / 6.0 * factor + 0.5);
  aNm = int(accumNms / 6.0 * factor + 0.5);
}

// common planar / YUY2. slow==1
template<typename pixel_t>
void TDeinterlace::subtractFields1(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
  VideoInfo &vi_map, 
  int &aPn, int &aNn, int &aPm, int &aNm, 
  int fieldt, int ordert,
  bool d2, int bits_per_pixel, IScriptEnvironment *env)
{
  PVideoFrame map = env->NewVideoFrame(vi_map);
  const int np = vi_map.IsYUY2() || vi_map.IsY() ? 1 : 3;
  int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  
  uint64_t accumPns = 0, accumNns = 0, accumNmls = 0;
  uint64_t accumPms = 0, accumNms = 0, accumPmls = 0;

  aPn = aNn = 0;
  aPm = aNm = 0;
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    uint8_t* mapp = map->GetWritePtr(plane);
    const int map_pitch = map->GetPitch(plane) << 1;
    
    const pixel_t* prvp = reinterpret_cast<const pixel_t *>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);

    const pixel_t* srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    const int Width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = src->GetHeight(plane);
    
    const pixel_t* nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);
    
    const int startx = vi_map.IsYUY2() ? 16 : 8 >> vi_map.GetPlaneWidthSubsampling(plane);
    const int stopx = Width - startx;
    
    memset(mapp, 0, Height * (map_pitch >> 1));
    const pixel_t* prvpf, * curf, * nxtpf;
    int prvf_pitch, curf_pitch, nxtf_pitch, actual_tbuffer_pitch;
    
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
    const pixel_t* prvnf = prvpf + prvf_pitch;
    const pixel_t* curpf = curf - curf_pitch;
    const pixel_t* curnf = curf + curf_pitch;
    const pixel_t* nxtnf = nxtpf + nxtf_pitch;
    uint8_t* mapn = mapp + map_pitch;
    
    if (b == 0) 
      actual_tbuffer_pitch = tpitchy;
    else
      actual_tbuffer_pitch = tpitchuv;
    
    if (vi_map.IsPlanar())
    {
      if (fieldt != 1)
        buildDiffMapPlane_Planar<pixel_t>(
          reinterpret_cast<const uint8_t*>(prvpf), 
          reinterpret_cast<const uint8_t*>(nxtpf),
          mapp, 
          prvf_pitch * sizeof(pixel_t), 
          nxtf_pitch * sizeof(pixel_t), 
          map_pitch, 
          Height, Width,
          actual_tbuffer_pitch,
          bits_per_pixel,
          env);
      else
        buildDiffMapPlane_Planar<pixel_t>(
          reinterpret_cast<const uint8_t*>(prvnf),
          reinterpret_cast<const uint8_t*>(nxtnf),
          mapn, 
          prvf_pitch * sizeof(pixel_t),
          nxtf_pitch * sizeof(pixel_t),
          map_pitch, 
          Height, Width,
          actual_tbuffer_pitch,
          bits_per_pixel,
          env);
    }
    else
    {
      if (fieldt != 1)
        buildDiffMapPlaneYUY2(
          reinterpret_cast<const uint8_t*>(prvpf),
          reinterpret_cast<const uint8_t*>(nxtpf), 
          mapp, 
          prvf_pitch, nxtf_pitch, map_pitch, Height, Width,
          actual_tbuffer_pitch,
          env);
      else
        buildDiffMapPlaneYUY2(
          reinterpret_cast<const uint8_t*>(prvnf),
          reinterpret_cast<const uint8_t*>(nxtnf),
          mapn, 
          prvf_pitch, nxtf_pitch, map_pitch, Height, Width, 
          actual_tbuffer_pitch, 
          env);
    }

    const int Const23 = 23 << (bits_per_pixel - 8);
    const int Const42 = 42 << (bits_per_pixel - 8);

    for (int y = 2; y < Height - 2; y += 2)
    {
      for (int x = startx; x < stopx; x++) {
        int map_flag = (mapp[x] << 3) + mapn[x];
        if (map_flag == 0)
            continue;

        int cur_sum = curpf[x] + (curf[x] << 2) + curnf[x];

        int diff_p = abs((prvpf[x] + prvnf[x]) * 3 - cur_sum);
        if (diff_p > Const23) {
          if ((map_flag & 9) != 0)
            accumPns += diff_p;
          if (diff_p > Const42) {
            if ((map_flag & 18) != 0)
              accumPms += diff_p;
            if ((map_flag & 36) != 0)
              accumPmls += diff_p;
          }
        }

        int diff_n = abs((nxtpf[x] + nxtnf[x]) * 3 - cur_sum);
        if (diff_n > Const23) {
          if ((map_flag & 9) != 0)
            accumNns += diff_n;
          if (diff_n > Const42) {
            if ((map_flag & 18) != 0)
              accumNms += diff_n;
            if ((map_flag & 36) != 0)
              accumNmls += diff_n;
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

  const int Const500 = 500 << (bits_per_pixel - 8);
  if (accumPms < Const500 && 
    accumNms < Const500 && 
    (accumPmls >= Const500 || accumNmls >= Const500) &&
    std::max(accumPmls, accumNmls) > 3 * std::min(accumPmls, accumNmls))
  {
    accumPms = accumPmls;
    accumNms = accumNmls;
  }
  // back to 8 bits range
  const double factor = 1.0 / (1 << (bits_per_pixel - 8));
  aPn = int(accumPns * factor / 6.0 + 0.5);
  aNn = int(accumNns * factor / 6.0 + 0.5);
  aPm = int(accumPms * factor / 6.0 + 0.5);
  aNm = int(accumNms * factor / 6.0 + 0.5);
}

// common planar / YUY2. slow==2
template<typename pixel_t>
void TDeinterlace::subtractFields2(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt,
  VideoInfo& vi_map, 
  int& aPn, int& aNn, int& aPm, int& aNm, 
  int fieldt, int ordert,
  bool d2, int bits_per_pixel, IScriptEnvironment* env)
{
  PVideoFrame map = env->NewVideoFrame(vi_map);

  const int stop = vi_map.IsYUY2() || vi_map.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  
  uint64_t accumPns = 0, accumNns = 0, accumNmls = 0;
  uint64_t accumPms = 0, accumNms = 0, accumPmls = 0;
  
  aPn = aNn = 0;
  aPm = aNm = 0;

  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];

    // always 8 bit
    uint8_t* mapp = map->GetWritePtr(plane);
    const int map_pitch = map->GetPitch(plane) << 1;

    const pixel_t* prvp = reinterpret_cast<const pixel_t *>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);

    const pixel_t* srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    const int Width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = src->GetHeight(plane);
    
    const pixel_t* nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);
    
    const int startx = vi_map.IsYUY2() ? 16 : (8 >> vi_map.GetPlaneWidthSubsampling(plane));
    const int stopx = Width - startx;
    
    memset(mapp, 0, Height * (map_pitch >> 1));

    const pixel_t* prvpf, * curf, * nxtpf;
    int prvf_pitch, curf_pitch, nxtf_pitch, actual_tbuffer_pitch;
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
    const pixel_t* prvppf = prvpf - prvf_pitch;
    const pixel_t* prvnf = prvpf + prvf_pitch;
    const pixel_t* prvnnf = prvnf + prvf_pitch;
    const pixel_t* curpf = curf - curf_pitch;
    const pixel_t* curnf = curf + curf_pitch;
    const pixel_t* nxtppf = nxtpf - nxtf_pitch;
    const pixel_t* nxtnf = nxtpf + nxtf_pitch;
    const pixel_t* nxtnnf = nxtnf + nxtf_pitch;
    uint8_t* mapn = mapp + map_pitch;
    
    if (b == 0)
      actual_tbuffer_pitch = tpitchy;
    else 
      actual_tbuffer_pitch = tpitchuv;

    if (vi_map.IsPlanar())
    {
      if (fieldt != 1)
        buildDiffMapPlane_Planar<pixel_t>(
          reinterpret_cast<const uint8_t *>(prvpf), 
          reinterpret_cast<const uint8_t *>(nxtpf),
          mapp, 
          prvf_pitch * sizeof(pixel_t),
          nxtf_pitch * sizeof(pixel_t),
          map_pitch, 
          Height, Width, 
          actual_tbuffer_pitch, bits_per_pixel, env);
      else
        buildDiffMapPlane_Planar<pixel_t>(
          reinterpret_cast<const uint8_t*>(prvnf),
          reinterpret_cast<const uint8_t*>(nxtnf),
          mapn, 
          prvf_pitch * sizeof(pixel_t),
          nxtf_pitch * sizeof(pixel_t),
          map_pitch, 
          Height, Width, 
          actual_tbuffer_pitch, bits_per_pixel, env);
    }
    else
    {
      if (fieldt != 1)
        buildDiffMapPlaneYUY2(
          reinterpret_cast<const uint8_t*>(prvpf),
          reinterpret_cast<const uint8_t*>(nxtpf),
          mapp, 
          prvf_pitch * sizeof(pixel_t),
          nxtf_pitch * sizeof(pixel_t),
          map_pitch, 
          Height, Width, 
          actual_tbuffer_pitch, env);
      else
        buildDiffMapPlaneYUY2(
          reinterpret_cast<const uint8_t*>(prvnf),
          reinterpret_cast<const uint8_t*>(nxtnf),
          mapn, 
          prvf_pitch * sizeof(pixel_t),
          nxtf_pitch * sizeof(pixel_t),
          map_pitch, 
          Height, Width, 
          actual_tbuffer_pitch, env);
    }

    const int Const23 = 23 << (bits_per_pixel - 8);
    const int Const42 = 42 << (bits_per_pixel - 8);
    if (fieldt == 0)
    {
      for (int y = 2; y < Height - 2; y += 2)
      {
        for (int x = startx; x < stopx; x++) {
          int map_flag = (mapp[x] << 3) + mapn[x];
          if (map_flag == 0)
              continue;

          int cur_sum = curpf[x] + (curf[x] << 2) + curnf[x];

          int diff_p = abs((prvpf[x] + prvnf[x]) * 3 - cur_sum);
          if (diff_p > Const23) {
            if ((map_flag & 9) != 0)
              accumPns += diff_p;
            if (diff_p > Const42) {
              if ((map_flag & 18) != 0)
                accumPms += diff_p;
              if ((map_flag & 36) != 0)
                accumPmls += diff_p;
            }
          }

          int diff_n = abs((nxtpf[x] + nxtnf[x]) * 3 - cur_sum);
          if (diff_n > Const23) {
            if ((map_flag & 9) != 0)
              accumNns += diff_n;
            if (diff_n > Const42) {
              if ((map_flag & 18) != 0)
                accumNms += diff_n;
              if ((map_flag & 36) != 0)
                accumNmls += diff_n;
            }
          }
          // this part is plus to the version'1'
          if ((map_flag & 56) != 0) {

            int cur_sum = (curpf[x] + curf[x]) * 3;

            int prv_sum = prvppf[x] + (prvpf[x] << 2) + prvnf[x];

            int diff_p = abs(cur_sum - prv_sum);

            if (diff_p > Const23) {
              if ((map_flag & 8) != 0)
                accumPns += diff_p;
              if (diff_p > Const42) {
                if ((map_flag & 16) != 0)
                  accumPms += diff_p;
                if ((map_flag & 32) != 0)
                  accumPmls += diff_p;
              }
            }

            int nxt_sum = nxtppf[x] + (nxtpf[x] << 2) + nxtnf[x];

            int diff_n = abs(cur_sum - nxt_sum);

            if (diff_n > Const23) {
              if ((map_flag & 8) != 0)
                accumNns += diff_n;
              if (diff_n > Const42) {
                if ((map_flag & 16) != 0)
                  accumNms += diff_n;
                if ((map_flag & 32) != 0)
                  accumNmls += diff_n;
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
          int map_flag = (mapp[x] << 3) + mapn[x];
          if (map_flag == 0)
            continue;

          int cur_sum = curpf[x] + (curf[x] << 2) + curnf[x];

          int diff_p = abs((prvpf[x] + prvnf[x]) * 3 - cur_sum);
          if (diff_p > Const23) {
            if ((map_flag & 9) != 0)
              accumPns += diff_p;
            if (diff_p > Const42) {
              if ((map_flag & 18) != 0)
                accumPms += diff_p;
              if ((map_flag & 36) != 0)
                accumPmls += diff_p;
            }
          }

          int diff_n = abs((nxtpf[x] + nxtnf[x]) * 3 - cur_sum);
          if (diff_n > Const23) {
            if ((map_flag & 9) != 0)
              accumNns += diff_n;
            if (diff_n > Const42) {
              if ((map_flag & 18) != 0)
                accumNms += diff_n;
              if ((map_flag & 36) != 0)
                accumNmls += diff_n;
            }
          }

          // p61: this part is plus to the version'1'
          if ((map_flag & 7) != 0) {

            int cur_sum = (curf[x] + curnf[x]) * 3;

            int prv_sum = prvpf[x] + (prvnf[x] << 2) + prvnnf[x];

            int diff_p = abs(cur_sum - prv_sum);

            if (diff_p > Const23) {
              if ((map_flag & 1) != 0)
                accumPns += diff_p;
              if (diff_p > Const42) {
                if ((map_flag & 2) != 0)
                  accumPms += diff_p;
                if ((map_flag & 4) != 0)
                  accumPmls += diff_p;
              }
            }

            int nxt_sum = nxtpf[x] + (nxtnf[x] << 2) + nxtnnf[x];

            int diff_n = abs(cur_sum - nxt_sum);

            if (diff_n > Const23) {
              if ((map_flag & 1) != 0)
                accumNns += diff_n;
              if (diff_n > Const42) {
                if ((map_flag & 2) != 0)
                  accumNms += diff_n;
                if ((map_flag & 4) != 0)
                  accumNmls += diff_n;
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

  const int Const500 = 500 << (bits_per_pixel - 8);
  if (accumPms < Const500 && 
    accumNms < Const500 && 
    (accumPmls >= Const500 || accumNmls >= Const500) &&
    std::max(accumPmls, accumNmls) > 3 * std::min(accumPmls, accumNmls))
  {
    accumPms = accumPmls;
    accumNms = accumNmls;
  }
  // back to 8 bit magnitude
  const double factor = 1.0 / (1 << (bits_per_pixel - 8));
  aPn = int(accumPns * factor / 6.0 + 0.5);
  aNn = int(accumNns * factor / 6.0 + 0.5);
  aPm = int(accumPms * factor / 6.0 + 0.5);
  aNm = int(accumNms * factor / 6.0 + 0.5);
}

void TDeinterlace::dispatch_mapColorsPlanar(PVideoFrame& dst, PVideoFrame& mask, const VideoInfo& vi)
{
  switch (vi.BitsPerComponent()) {
  case 8: mapColorsPlanar<uint8_t, 8>(dst, mask); break;
  case 10: mapColorsPlanar<uint16_t, 10>(dst, mask); break;
  case 12: mapColorsPlanar<uint16_t, 12>(dst, mask); break;
  case 14: mapColorsPlanar<uint16_t, 14>(dst, mask); break;
  case 16: mapColorsPlanar<uint16_t, 16>(dst, mask); break;
  }
}


template<typename pixel_t, int bits_per_pixel>
void TDeinterlace::mapColorsPlanar(PVideoFrame &dst, PVideoFrame &mask)
{
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    
    // mask is always 8 bits
    const uint8_t *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const int Height = mask->GetHeight(plane);
    const int Width = mask->GetRowSize(plane);

    pixel_t *dstp = reinterpret_cast<pixel_t *>(dst->GetWritePtr(plane));
    const int dst_pitch = dst->GetPitch(plane) / sizeof(pixel_t);
    
    for (int y = 0; y < Height; ++y)
    {
      for (int x = 0; x < Width; ++x)
      {
        if (maskp[x] == 10) dstp[x] = 0;
        else if (maskp[x] == 20) dstp[x] = 51 << (bits_per_pixel - 8);
        else if (maskp[x] == 30) dstp[x] = 102 << (bits_per_pixel - 8);
        else if (maskp[x] == 40) dstp[x] = 153 << (bits_per_pixel - 8);
        else if (maskp[x] == 50) dstp[x] = 204 << (bits_per_pixel - 8);
        else if (maskp[x] == 60) dstp[x] = 255 << (bits_per_pixel - 8);
        else if (maskp[x] == 70) dstp[x] = 230 << (bits_per_pixel - 8);
      }
      maskp += mask_pitch;
      dstp += dst_pitch;
    }
  }
}

void TDeinterlace::dispatch_mapMergePlanar(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo& vi)
{
  const int bits_per_pixel = vi.BitsPerComponent();
  switch (bits_per_pixel) {
  case 8: mapMergePlanar<uint8_t>(dst, mask, prv, src, nxt, bits_per_pixel); break;
  case 10:
  case 12:
  case 14:
  case 16: mapMergePlanar<uint16_t>(dst, mask, prv, src, nxt, bits_per_pixel); break;
  }
}

template<typename pixel_t>
void TDeinterlace::mapMergePlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int bits_per_pixel)
{
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };

  const int max_pixel_value = (1 << bits_per_pixel) - 1;

  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    // mask is a special clip, always 8 bits
    const uint8_t *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    
    const int Height = mask->GetHeight(plane);
    const int Width = mask->GetRowSize(plane); // mask is always 8 bits
    
    pixel_t *dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));
    const int dst_pitch = dst->GetPitch(plane) / sizeof(pixel_t);
    
    const pixel_t*prvp = reinterpret_cast<const pixel_t*>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);
    
    const pixel_t*srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);

    const pixel_t *nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);

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
        else if (maskp[x] == 60) dstp[x] = max_pixel_value; // max_pixel_value
      }
      prvp += prv_pitch;
      srcp += src_pitch;
      nxtp += nxt_pitch;
      maskp += mask_pitch;
      dstp += dst_pitch;
    }
  }
}

void TDeinterlace::dispatch_eDeintPlanar(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, PVideoFrame& efrm, const VideoInfo& vi) {
  switch (vi.BitsPerComponent()) {
  case 8: eDeintPlanar<uint8_t>(dst, mask, prv, src, nxt, efrm); break;
  case 10:
  case 12:
  case 14:
  case 16: eDeintPlanar<uint16_t>(dst, mask, prv, src, nxt, efrm); break;
  }
}

// HBD ready OK
template<typename pixel_t>
void TDeinterlace::eDeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, PVideoFrame &efrm)
{
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const pixel_t *prvp = reinterpret_cast<const pixel_t*>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);
    
    const pixel_t *srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    const int width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int height = src->GetHeight(plane);

    const pixel_t *nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane);

    // mask is a special clip, always 8 bits
    const uint8_t *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);

    const pixel_t *efrmp = reinterpret_cast<const pixel_t*>(efrm->GetReadPtr(plane));
    const int efrm_pitch = efrm->GetPitch(plane) / sizeof(pixel_t);

    pixel_t *dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));
    const int dst_pitch = dst->GetPitch(plane) / sizeof(pixel_t);

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

void TDeinterlace::dispatch_cubicDeintPlanar(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo& vi)
{
  switch (vi.BitsPerComponent()) {
  case 8: cubicDeintPlanar<uint8_t, 8>(dst, mask, prv, src, nxt); break;
  case 10: cubicDeintPlanar<uint16_t, 10>(dst, mask, prv, src, nxt); break;
  case 12: cubicDeintPlanar<uint16_t, 12>(dst, mask, prv, src, nxt); break;
  case 14: cubicDeintPlanar<uint16_t, 14>(dst, mask, prv, src, nxt); break;
  case 16: cubicDeintPlanar<uint16_t, 16>(dst, mask, prv, src, nxt); break;
  }
}

template<typename pixel_t, int bits_per_pixel>
void TDeinterlace::cubicDeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];

    const pixel_t *prvp = reinterpret_cast<const pixel_t*>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);

    const pixel_t*srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    const int src_pitch2 = src_pitch << 1;
    const int Width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = src->GetHeight(plane);

    const pixel_t* nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);
    
    pixel_t*dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));
    const int dst_pitch = dst->GetPitch(plane) / sizeof(pixel_t);
    
    const uint8_t *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    
    const pixel_t*srcpp = srcp - src_pitch;
    const pixel_t*srcppp = srcpp - src_pitch2;
    const pixel_t*srcpn = srcp + src_pitch;
    const pixel_t*srcpnn = srcpn + src_pitch2;

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
          else dstp[x] = cubicInt<bits_per_pixel>(srcppp[x], srcpp[x], srcpn[x], srcpnn[x]);
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

void TDeinterlace::dispatch_ELADeintPlanar(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo &vi)
{
  switch (vi.BitsPerComponent()) {
  case 8: ELADeintPlanar<uint8_t, 8>(dst, mask, prv, src, nxt); break;
  case 10: ELADeintPlanar<uint16_t, 10>(dst, mask, prv, src, nxt); break;
  case 12: ELADeintPlanar<uint16_t, 12>(dst, mask, prv, src, nxt); break;
  case 14: ELADeintPlanar<uint16_t, 14>(dst, mask, prv, src, nxt); break;
  case 16: ELADeintPlanar<uint16_t, 16>(dst, mask, prv, src, nxt); break;
  }
}

// HDB ready
// we have scaled constant inside, bits_per_pixel template may help speed
template<typename pixel_t, int bits_per_pixel>
void TDeinterlace::ELADeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];

    const pixel_t *prvp = reinterpret_cast<const pixel_t *>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);
    
    const pixel_t *srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    const int Width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = src->GetHeight(plane);
    
    const pixel_t *nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);
    
    pixel_t* dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));
    const int dst_pitch = dst->GetPitch(plane) / sizeof(pixel_t);

    const uint8_t *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);

    const pixel_t *srcpp = srcp - src_pitch;
    const pixel_t *srcpn = srcp + src_pitch;

    const int ustop = 8 >> vi.GetPlaneWidthSubsampling(plane);
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
          const int Const10 = 10 << (bits_per_pixel - 8);
          const int Const2 = 2 << (bits_per_pixel - 8);
          if (y == 0) 
            dstp[x] = srcpn[x];
          else if (y == Height - 1) 
            dstp[x] = srcpp[x];
          else if (x < 2 || x > Width - 3 ||
            (abs(srcpp[x] - srcpn[x]) < Const10 &&
              abs(srcpp[x - 2] - srcpp[x + 2]) < Const10 &&
              abs(srcpn[x - 2] - srcpn[x + 2]) < Const10))
          {
            dstp[x] = (srcpp[x] + srcpn[x] + 1) >> 1;
          }
          else
          {
            const int stop = std::min(x - 1, std::min(ustop, Width - 2 - x));
            const int minf = std::min(srcpp[x], srcpn[x]) - Const2;
            const int maxf = std::max(srcpp[x], srcpn[x]) + Const2;
            int val = (srcpp[x] + srcpn[x] + 1) >> 1;
            const int ConstMin450 = 450 << (bits_per_pixel - 8);
            
            int min = ConstMin450;

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
                if (temp1 < ConstMin450 && temp2 >= minf && temp2 <= maxf)
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

void TDeinterlace::dispatch_kernelDeintPlanar(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo& vi) {
  switch (vi.BitsPerComponent()) {
  case 8: kernelDeintPlanar<uint8_t, 8>(dst, mask, prv, src, nxt); break;
  case 10: kernelDeintPlanar<uint16_t, 10>(dst, mask, prv, src, nxt); break;
  case 12: kernelDeintPlanar<uint16_t, 12>(dst, mask, prv, src, nxt); break;
  case 14: kernelDeintPlanar<uint16_t, 14>(dst, mask, prv, src, nxt); break;
  case 16: kernelDeintPlanar<uint16_t, 16>(dst, mask, prv, src, nxt); break;
  }
}

// HBD ready
// we have scaled constant inside, bits_per_pixel template may help speed
template<typename pixel_t, int bits_per_pixel>
void TDeinterlace::kernelDeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const pixel_t *prvp = reinterpret_cast<const pixel_t*>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);
    const int prv_pitch2 = prv_pitch << 1;
    const pixel_t *srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    const int src_pitch2 = src_pitch << 1;
    const int Width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = src->GetHeight(plane);
    const pixel_t *nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);
    const int nxt_pitch2 = nxt_pitch << 1;
    pixel_t*dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));
    const int dst_pitch = dst->GetPitch(plane) / sizeof(pixel_t);
    // mask is 8 bits
    const uint8_t *maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);

    const pixel_t *srcpp = srcp - src_pitch;
    const pixel_t*srcppp = srcpp - src_pitch2;
    const pixel_t*srcpn = srcp + src_pitch;
    const pixel_t*srcpnn = srcpn + src_pitch2;
    const pixel_t *kerc, *kerp, *kerpp, *kern, *kernn;
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
            const int temp = (int)((
              0.526*(srcpp[x] + srcpn[x]) +
              0.170*(kerc[x]) -
              0.116*(kerp[x] + kern[x]) -
              0.026*(srcppp[x] + srcpnn[x]) +
              0.031*(kerpp[x] + kernn[x])) + 0.5f);
            dstp[x] = std::min(std::max(temp, 0), max_pixel_value);
          }
          else if (y > 1 && y < Height - 2)
          {
            const int temp = (((srcpp[x] + srcpn[x]) << 3) +
              (kerc[x] << 1) - (kerp[x] + kern[x]) + 8) >> 4;
            dstp[x] = std::min(std::max(temp, 0), max_pixel_value);
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

void dispatch_smartELADeintPlanar(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt, const VideoInfo& vi) {
  switch (vi.BitsPerComponent()) {
  case 8: smartELADeintPlanar<uint8_t, 8>(dst, mask, prv, src, nxt); break;
  case 10: smartELADeintPlanar<uint16_t, 10>(dst, mask, prv, src, nxt); break;
  case 12: smartELADeintPlanar<uint16_t, 12>(dst, mask, prv, src, nxt); break;
  case 14: smartELADeintPlanar<uint16_t, 14>(dst, mask, prv, src, nxt); break;
  case 16: smartELADeintPlanar<uint16_t, 16>(dst, mask, prv, src, nxt); break;
  }
}

// HBD ready
template<typename pixel_t, int bits_per_pixel>
void smartELADeintPlanar(PVideoFrame &dst, PVideoFrame &mask,
  PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt)
{
  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  const pixel_t *prvpY = reinterpret_cast<const pixel_t *>(prv->GetReadPtr(PLANAR_Y));
  const pixel_t*prvpV = reinterpret_cast<const pixel_t*>(prv->GetReadPtr(PLANAR_V));
  const pixel_t*prvpU = reinterpret_cast<const pixel_t*>(prv->GetReadPtr(PLANAR_U));
  const int prv_pitchY = prv->GetPitch(PLANAR_Y) / sizeof(pixel_t);
  const int prv_pitchUV = prv->GetPitch(PLANAR_V) / sizeof(pixel_t);
  
  const pixel_t*srcpY = reinterpret_cast<const pixel_t*>(src->GetReadPtr(PLANAR_Y));
  const pixel_t*srcpV = reinterpret_cast<const pixel_t*>(src->GetReadPtr(PLANAR_V));
  const pixel_t*srcpU = reinterpret_cast<const pixel_t*>(src->GetReadPtr(PLANAR_U));
  const int src_pitchY = src->GetPitch(PLANAR_Y) / sizeof(pixel_t);
  const int src_pitchY2 = src_pitchY << 1;
  const int src_pitchUV = src->GetPitch(PLANAR_V) / sizeof(pixel_t);
  const int src_pitchUV2 = src_pitchUV << 1;
  
  const int WidthY = src->GetRowSize(PLANAR_Y) / sizeof(pixel_t);
  const int WidthUV = src->GetRowSize(PLANAR_V) / sizeof(pixel_t);
  const int HeightY = src->GetHeight(PLANAR_Y);
  const int HeightUV = src->GetHeight(PLANAR_V);
  
  const pixel_t*nxtpY = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(PLANAR_Y));
  const pixel_t*nxtpV = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(PLANAR_V));
  const pixel_t*nxtpU = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(PLANAR_U));
  const int nxt_pitchY = nxt->GetPitch(PLANAR_Y) / sizeof(pixel_t);
  const int nxt_pitchUV = nxt->GetPitch(PLANAR_V) / sizeof(pixel_t);
  
  pixel_t*dstpY = reinterpret_cast<pixel_t*>(dst->GetWritePtr(PLANAR_Y));
  pixel_t*dstpV = reinterpret_cast<pixel_t*>(dst->GetWritePtr(PLANAR_V));
  pixel_t*dstpU = reinterpret_cast<pixel_t*>(dst->GetWritePtr(PLANAR_U));
  const int dst_pitchY = dst->GetPitch(PLANAR_Y) / sizeof(pixel_t);
  const int dst_pitchUV = dst->GetPitch(PLANAR_V) / sizeof(pixel_t);
  
  const uint8_t *maskpY = mask->GetReadPtr(PLANAR_Y);
  const uint8_t *maskpV = mask->GetReadPtr(PLANAR_V);
  const uint8_t *maskpU = mask->GetReadPtr(PLANAR_U);
  const int mask_pitchY = mask->GetPitch(PLANAR_Y);
  const int mask_pitchUV = mask->GetPitch(PLANAR_V);
  
  const pixel_t*srcppY = srcpY - src_pitchY;
  const pixel_t*srcpppY = srcppY - src_pitchY2;
  const pixel_t*srcpnY = srcpY + src_pitchY;
  const pixel_t*srcpnnY = srcpnY + src_pitchY2;
  const pixel_t*srcppV = srcpV - src_pitchUV;
  const pixel_t*srcpppV = srcppV - src_pitchUV2;
  const pixel_t*srcpnV = srcpV + src_pitchUV;
  const pixel_t*srcpnnV = srcpnV + src_pitchUV2;
  const pixel_t*srcppU = srcpU - src_pitchUV;
  const pixel_t*srcpppU = srcppU - src_pitchUV2;
  const pixel_t*srcpnU = srcpU + src_pitchUV;
  const pixel_t*srcpnnU = srcpnU + src_pitchUV2;
  
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
          const int Const1600 = (40 << (bits_per_pixel - 8)) * (40 << (bits_per_pixel - 8));
          const int Const10 = 10 << (bits_per_pixel - 8);
          if (edgeS1 < Const1600 && edgeS2 < Const1600)
          {
            dstpY[x] = (srcppY[x] + srcpnY[x] + 1) >> 1;
            continue;
          }
          if (abs(srcppY[x] - srcpnY[x]) < Const10 && (edgeS1 < Const1600 || edgeS2 < Const1600))
          {
            dstpY[x] = (srcppY[x] + srcpnY[x] + 1) >> 1;
            continue;
          }
          const int sum = srcppY[x - 1] + srcppY[x] + srcppY[x + 1] + srcpnY[x - 1] + srcpnY[x] + srcpnY[x + 1];
          const int sumsq = srcppY[x - 1] * srcppY[x - 1] + srcppY[x] * srcppY[x] + srcppY[x + 1] * srcppY[x + 1] +
            srcpnY[x - 1] * srcpnY[x - 1] + srcpnY[x] * srcpnY[x] + srcpnY[x + 1] * srcpnY[x + 1];

          const int Const432 = 432 << (2 * (bits_per_pixel - 8)); // squared scale
          if ((6 * sumsq - sum*sum) < Const432)
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
          const int Const3600 = (60 << (bits_per_pixel - 8)) * (60 << (bits_per_pixel - 8));
          const int Const5000 = 5000 << (2 * (bits_per_pixel - 8)); // squared scale, 5000 has no nice sqrt
          if (fabs(dir1 - dir2) < 0.5)
          {
            if (edgeS1 >= Const3600 && edgeS2 >= Const3600) dir = (dir1 + dir2) * 0.5f;
            else dir = edgeS1 >= edgeS2 ? dir1 : dir2;
          }
          else
          {
            if (edgeS1 >= Const5000 && edgeS2 >= Const5000)
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
          double dirF = 0.5 / tan(dir);
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
                      temp1 = srcppY[x + 4];
                      temp2 = srcpnY[x - 4];
                      temp = (temp1 + temp2 + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcpnY[x];
                      temp = cubicInt<bits_per_pixel>(srcpppY[x], srcppY[x], srcpnY[x], srcpnnY[x]);
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
                      temp = (temp1 + temp2 + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcpnY[x];
                      temp = cubicInt<bits_per_pixel>(srcpppY[x], srcppY[x], srcpnY[x], srcpnnY[x]);
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

          const int Const20 = 20 << (bits_per_pixel - 8);
          const int Const25 = 25 << (bits_per_pixel - 8);
          const int Const60 = 60 << (bits_per_pixel - 8);
          const int maxN = std::max(srcppY[x], srcpnY[x]) + Const25;
          const int minN = std::min(srcppY[x], srcpnY[x]) - Const25;
          if (abs(temp1 - temp2) > Const20 || 
            abs(srcppY[x] + srcpnY[x] - 2*temp) > Const60 
            || temp < minN 
            || temp > maxN)
          {
            temp = cubicInt<bits_per_pixel>(srcpppY[x], srcppY[x], srcpnY[x], srcpnnY[x]);
          }
          dstpY[x] = std::min(std::max(temp, 0), max_pixel_value);
        }
        else
        {
          if (y == 0) dstpY[x] = srcpnY[x];
          else if (y == HeightY - 1) dstpY[x] = srcppY[x];
          else if (y<3 || y>HeightY - 4) dstpY[x] = (srcpnY[x] + srcppY[x] + 1) >> 1;
          else dstpY[x] = cubicInt<bits_per_pixel>(srcpppY[x], srcppY[x], srcpnY[x], srcpnnY[x]);
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
        else dstpV[x] = cubicInt<bits_per_pixel>(srcpppV[x], srcppV[x], srcpnV[x], srcpnnV[x]);
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
        else dstpU[x] = cubicInt<bits_per_pixel>(srcpppU[x], srcppU[x], srcpnU[x], srcpnnU[x]);
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

void TDeinterlace::dispatch_blendDeint(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt,
  const VideoInfo& vi, IScriptEnvironment* env) {
  switch (vi.BitsPerComponent()) {
  case 8: blendDeint<uint8_t>(dst, mask, prv, src, nxt, env); break;
  case 10:
  case 12:
  case 14:
  case 16: blendDeint<uint16_t>(dst, mask, prv, src, nxt, env); break;
  }
}

// common YUY2 Planar
template<typename pixel_t>
void TDeinterlace::blendDeint(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt,
  IScriptEnvironment* env)
{
  PVideoFrame tf = env->NewVideoFrame(vi_saved);
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];

    const pixel_t* prvp = reinterpret_cast<const pixel_t*>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);
    
    const pixel_t* srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    const int Width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = src->GetHeight(plane);
    
    const pixel_t* nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);
    
    pixel_t* dstp = reinterpret_cast<pixel_t*>(tf->GetWritePtr(plane));
    const int dst_pitch = tf->GetPitch(plane) / sizeof(pixel_t);
    
    const uint8_t* maskp = mask->GetReadPtr(plane);
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
  copyFrame(dst, tf, vi, env);
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    
    const pixel_t* srcp = reinterpret_cast<const pixel_t*>(tf->GetReadPtr(plane));
    const int src_pitch = tf->GetPitch(plane) / sizeof(pixel_t);
    const int Width = tf->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = tf->GetHeight(plane);
    const pixel_t* srcpp = srcp - src_pitch;
    const pixel_t* srcpn = srcp + src_pitch;
    
    pixel_t* dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));
    const int dst_pitch = dst->GetPitch(plane) / sizeof(pixel_t);

    const uint8_t* maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const uint8_t* maskpp = maskp - mask_pitch;
    const uint8_t* maskpn = maskp + mask_pitch;

    for (int y = 0; y < Height; ++y)
    {
      for (int x = 0; x < Width; ++x)
      {
        if (maskp[x] == 60 || (y != 0 && maskpp[x] == 60) ||
          (y != Height - 1 && maskpn[x] == 60))
        {
          if (y == 0)
            dstp[x] = (srcpn[x] + srcp[x] + 1) >> 1;
          else if (y == Height - 1)
            dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
          else
            dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
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

void TDeinterlace::dispatch_blendDeint2(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt,
  const VideoInfo& vi, IScriptEnvironment* env) {
  switch (vi.BitsPerComponent()) {
  case 8: blendDeint2<uint8_t>(dst, mask, prv, src, nxt, env); break;
  case 10:
  case 12:
  case 14:
  case 16: blendDeint2<uint16_t>(dst, mask, prv, src, nxt, env); break;
  }
}


// both YUY2 and planar
template<typename pixel_t>
void TDeinterlace::blendDeint2(PVideoFrame& dst, PVideoFrame& mask,
  PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt,
  IScriptEnvironment* env)
{
  PVideoFrame tf = env->NewVideoFrame(vi_saved);
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    
    const pixel_t* prvp = reinterpret_cast<const pixel_t *>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);
    
    const pixel_t* srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    const int Width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = src->GetHeight(plane);
    
    const pixel_t* nxtp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);
    
    pixel_t* dstp = reinterpret_cast<pixel_t*>(tf->GetWritePtr(plane));
    const int dst_pitch = tf->GetPitch(plane) / sizeof(pixel_t);

    const uint8_t* maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    
    const pixel_t* kerp;
    int ker_pitch;
    if (field ^ order)
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
  copyFrame(dst, tf, vi, env);
  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const pixel_t* srcp = reinterpret_cast<const pixel_t*>(tf->GetReadPtr(plane));
    const int src_pitch = tf->GetPitch(plane) / sizeof(pixel_t);
    const int Width = tf->GetRowSize(plane) / sizeof(pixel_t);
    const int Height = tf->GetHeight(plane);
    const pixel_t* srcpp = srcp - src_pitch;
    const pixel_t* srcpn = srcp + src_pitch;

    pixel_t* dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));
    const int dst_pitch = dst->GetPitch(plane) / sizeof(pixel_t);

    const uint8_t* maskp = mask->GetReadPtr(plane);
    const int mask_pitch = mask->GetPitch(plane);
    const uint8_t* maskpp = maskp - mask_pitch;
    const uint8_t* maskpn = maskp + mask_pitch;
    
    for (int y = 0; y < Height; ++y)
    {
      for (int x = 0; x < Width; ++x)
      {
        if (maskp[x] == 60 || (y != 0 && maskpp[x] == 60) ||
          (y != Height - 1 && maskpn[x] == 60))
        {
          if (y == 0)
            dstp[x] = (srcpn[x] + srcp[x] + 1) >> 1;
          else if (y == Height - 1)
            dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
          else
            dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
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


// no special HBD
// for both YUY2 and planar
void TDeinterlace::createWeaveFrame(PVideoFrame &dst, PVideoFrame &prv,
  PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env)
{
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
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