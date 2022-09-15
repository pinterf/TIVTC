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

#include "TFMPP.h"
#include "internal.h"
#include "TCommonASM.h"
#include "emmintrin.h"
#include "smmintrin.h"
#include "info.h"


PVideoFrame __stdcall TFMPP::GetFrame(int n, IScriptEnvironment *env)
{
  if (n < 0) n = 0;
  else if (n > nfrms) n = nfrms;

  bool combed;
  int fieldSrc, field;
  unsigned int hint;
  PVideoFrame src = child->GetFrame(n, env);
  bool has_hint = false;
  bool has_props = false;
#ifdef DONT_USE_FRAMEPROPS
  has_hint = getHint(vi, src, fieldSrc, combed, hint);
#else
  if(has_at_least_v8) {
    has_hint = getProperties(src, fieldSrc, combed, env);  // try frame props first
    has_props = true;
  }
  if(!has_hint)
    has_hint = getHint(vi, src, fieldSrc, combed, hint);
#endif
  if (!combed)
  {
    if (usehints || !has_hint) return src;
    if(!has_props) { // destroy hint only if real hints not frame props
      env->MakeWritable(&src);
      destroyHint(vi, src, hint);
    }
    return src;
  }
  getSetOvr(n);
  PVideoFrame dst;
  if (PP > 4)
  {
    int use = 0;
    unsigned int hintt;
    PVideoFrame prv = child->GetFrame(n > 0 ? n - 1 : 0, env);
#ifdef DONT_USE_FRAMEPROPS
    getHint(vi, prv, field, combed, hintt);
#else
    has_hint = false;
    if (has_at_least_v8)
      has_hint = getProperties(prv, field, combed, env); // try frame props first
    if(!has_hint)
      getHint(vi, prv, field, combed, hintt);
#endif
    if (!combed && field != -1 && n != 0) ++use;
    PVideoFrame nxt = child->GetFrame(n < nfrms ? n + 1 : nfrms, env);
#ifdef DONT_USE_FRAMEPROPS
    getHint(vi, nxt, field, combed, hintt);
#else
    has_hint = false;
    if (has_at_least_v8)
      has_hint = getProperties(nxt, field, combed, env); // try frame props first
    if(!has_hint)
      getHint(vi, nxt, field, combed, hintt);
#endif
    if (!combed && field != -1 && n != nfrms) use += 2;
    if (use > 0)
    {
      dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi);
      buildMotionMask(prv, src, nxt, mmask, use, vi);
      if (uC2) {
        PVideoFrame frame = clip2->GetFrame(n, env);
        maskClip2(src, frame, mmask, dst, vi);
      }
      else
      {
        if (PP == 5)
          BlendDeint(src, mmask, dst, false, vi);
        else
        {
          if (PP == 6)
          {
            copyField(dst, src, vi, fieldSrc);
            CubicDeint(src, mmask, dst, false, fieldSrc, vi);
          }
          else
          {
            copyFrame(dst, src, vi);
            elaDeint(dst, mmask, src, false, fieldSrc, vi);
          }
        }
      }
    }
    else
    {
      if (uC2)
      {
        dst = clip2->GetFrame(n, env);
        env->MakeWritable(&dst);
      }
      else
      {
        dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi);
        if (PP == 5) 
          BlendDeint(src, mmask, dst, true, vi);
        else
        {
          if (PP == 6)
          {
            copyField(dst, src, vi, fieldSrc);
            CubicDeint(src, mmask, dst, true, fieldSrc, vi);
          }
          else
          {
            copyFrame(dst, src, vi);
            elaDeint(dst, mmask, src, true, fieldSrc, vi);
          }
        }
      }
    }
  }
  else
  {
    // PP <= 4
    if (uC2)
    {
      dst = clip2->GetFrame(n, env);
      env->MakeWritable(&dst);
    }
    else
    {
      dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi);
      if (PP == 2)
        BlendDeint(src, mmask, dst, true, vi);
      else
      {
        if (PP == 3)
        {
          copyField(dst, src, vi, fieldSrc);
          CubicDeint(src, mmask, dst, true, fieldSrc, vi);
        }
        else
        {
          copyFrame(dst, src, vi);
          elaDeint(dst, mmask, src, true, fieldSrc, vi);
        }
      }
    }
  }
  if (display) writeDisplay(dst, vi, n, fieldSrc);
  // VapourSynth port version doesn't write hint properties. Why?
  // Probably frame props are preserved - unlike bitmapped hints
  if (usehints) {
    if (!has_props) putHint(vi, dst, fieldSrc, hint);
  }
  else {
    if (!has_props) destroyHint(vi, dst, hint);
  }
  return dst;
}

void TFMPP::buildMotionMask(PVideoFrame& prv, PVideoFrame& src, PVideoFrame& nxt,
  PlanarFrame* mask, int use, const VideoInfo& vi) const
{
  if (vi.ComponentSize() == 1)
    buildMotionMask_core<uint8_t>(prv, src, nxt, mask, use, vi);
  else
    buildMotionMask_core<uint16_t>(prv, src, nxt, mask, use, vi);
}

template<typename pixel_t>
void TFMPP::buildMotionMask_core(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt,
  PlanarFrame *mask, int use, const VideoInfo &vi) const
{
  bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const pixel_t *prvpp = reinterpret_cast<const pixel_t *>(prv->GetReadPtr(plane));
    const int prv_pitch = prv->GetPitch(plane) / sizeof(pixel_t);
    const pixel_t*prvp = prvpp + prv_pitch;
    const pixel_t*prvpn = prvp + prv_pitch;

    const pixel_t *srcpp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    
    const int width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int height = src->GetHeight(plane);

    const pixel_t *srcp = srcpp + src_pitch;
    const pixel_t *srcpn = srcp + src_pitch;

    const pixel_t *nxtpp = reinterpret_cast<const pixel_t*>(nxt->GetReadPtr(plane));
    const int nxt_pitch = nxt->GetPitch(plane) / sizeof(pixel_t);
    const pixel_t *nxtp = nxtpp + nxt_pitch;
    const pixel_t *nxtpn = nxtp + nxt_pitch;

    uint8_t *maskw = mask->GetPtr(b);
    const int msk_pitch = mask->GetPitch(b);

    maskw += msk_pitch;
    
    const int mthresh_scaled = mthresh << (vi.BitsPerComponent() - 8);

    if (use == 1)
    {
      // fixme: hbd SIMD
      if (sizeof(pixel_t) == 1 && use_sse2)
        buildMotionMask1_SSE2((const uint8_t *)srcp, (const uint8_t*)prvp, maskw, src_pitch, prv_pitch, msk_pitch, width, height - 2);
      else
      {
        memset(maskw - msk_pitch, 0xFF, msk_pitch*height);
        for (int y = 1; y < height - 1; ++y)
        {
          for (int x = 0; x < width; ++x)
          {
            if (!(abs(prvpp[x] - srcpp[x]) > mthresh_scaled || abs(prvp[x] - srcp[x]) > mthresh_scaled ||
              abs(prvpn[x] - srcpn[x]) > mthresh_scaled)) maskw[x] = 0;
          }
          prvpp += prv_pitch;
          prvp += prv_pitch;
          prvpn += prv_pitch;
          srcpp += src_pitch;
          srcp += src_pitch;
          srcpn += src_pitch;
          maskw += msk_pitch;
        }
      }
    }
    else if (use == 2)
    {
      // fixme: hbd SIMD
      if (sizeof(pixel_t) == 1 && use_sse2)
        buildMotionMask1_SSE2((const uint8_t*)srcp, (const uint8_t*)nxtp, maskw, src_pitch, nxt_pitch, msk_pitch, width, height - 2);
      else
      {
        memset(maskw - msk_pitch, 0xFF, msk_pitch*height);
        for (int y = 1; y < height - 1; ++y)
        {
          for (int x = 0; x < width; ++x)
          {
            if (!(abs(nxtpp[x] - srcpp[x]) > mthresh_scaled || abs(nxtp[x] - srcp[x]) > mthresh_scaled ||
              abs(nxtpn[x] - srcpn[x]) > mthresh_scaled)) maskw[x] = 0;
          }
          srcpp += src_pitch;
          srcp += src_pitch;
          srcpn += src_pitch;
          nxtpp += nxt_pitch;
          nxtp += nxt_pitch;
          nxtpn += nxt_pitch;
          maskw += msk_pitch;
        }
      }
    }
    else
    {
      // fixme: hbd SIMD
      // use not 1 or 2
      if (sizeof(pixel_t) == 1 && use_sse2)
      {
        buildMotionMask2_SSE2((const uint8_t*)prvp, (const uint8_t*)srcp, (const uint8_t*)nxtp, maskw, prv_pitch, src_pitch, nxt_pitch, msk_pitch, width, height - 2);
        for (int y = 1; y < height; ++y)
        {
          for (int x = 0; x < width; ++x)
          {
            if (!maskw[x]) continue;
            if (((maskw[x] & 0x8) && (maskw[x] & 0x15)) ||
              ((maskw[x] & 0x4) && (maskw[x] & 0x2A)) ||
              ((maskw[x] & 0x22) && ((maskw[x] & 0x11) == 0x11)) ||
              ((maskw[x] & 0x11) && ((maskw[x] & 0x22) == 0x22)))
              maskw[x] = 0xFF;
            else maskw[x] = 0;
          }
          maskw += msk_pitch;
        }
      }
      else
      {
        memset(maskw - msk_pitch, 0xFF, msk_pitch*height);
        for (int y = 1; y < height - 1; ++y)
        {
          for (int x = 0; x < width; ++x)
          {
            if (!(((abs(prvp[x] - srcp[x]) > mthresh_scaled) && (abs(nxtpp[x] - srcpp[x]) > mthresh_scaled ||
              abs(nxtp[x] - srcp[x]) > mthresh_scaled || abs(nxtpn[x] - srcpn[x]) > mthresh_scaled)) ||
              ((abs(nxtp[x] - srcp[x]) > mthresh_scaled) && (abs(prvpp[x] - srcpp[x]) > mthresh_scaled ||
                abs(prvp[x] - srcp[x]) > mthresh_scaled || abs(prvpn[x] - srcpn[x]) > mthresh_scaled)) ||
                (abs(prvpp[x] - srcpp[x]) > mthresh_scaled && abs(prvpn[x] - srcpn[x]) > mthresh_scaled &&
              (abs(nxtpp[x] - srcpp[x]) > mthresh_scaled || abs(nxtpn[x] - srcpn[x]) > mthresh_scaled)) ||
                  ((abs(prvpp[x] - srcpp[x]) > mthresh_scaled || abs(prvpn[x] - srcpn[x]) > mthresh_scaled) &&
                    abs(nxtpp[x] - srcpp[x]) > mthresh_scaled && abs(nxtpn[x] - srcpn[x]) > mthresh_scaled)))
              maskw[x] = 0;
          }
          prvpp += prv_pitch;
          prvp += prv_pitch;
          prvpn += prv_pitch;
          srcpp += src_pitch;
          srcp += src_pitch;
          srcpn += src_pitch;
          nxtpp += nxt_pitch;
          nxtp += nxt_pitch;
          nxtpn += nxt_pitch;
          maskw += msk_pitch;
        }
      }
    }
  }
  if (vi.IsPlanar())
  {
    denoisePlanar(mask);
    if (vi.Is420())
      linkPlanar<420>(mask);
    else if (vi.Is422())
      linkPlanar<422>(mask);
    else if (vi.Is444())
      linkPlanar<444>(mask);
    else if (vi.IsYV411())
      linkPlanar<411>(mask);
  }
  else
  {
    denoiseYUY2(mask);
    linkYUY2(mask);
  }
}

void TFMPP::buildMotionMask1_SSE2(const uint8_t *srcp1, const uint8_t *srcp2,
  uint8_t *dstp, int s1_pitch, int s2_pitch, int dst_pitch, int width,
  int height) const
{
  memset(dstp - dst_pitch, 0xFF, dst_pitch);
  memset(dstp + dst_pitch*height, 0xFF, dst_pitch);
  __m128i thresh = _mm_set1_epi8((char)(std::max(std::min(255 - mthresh - 1, 255), 0)));
  __m128i full_ff = _mm_set1_epi8(-1);
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto next1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp1 + s1_pitch + x));
      auto next2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp2 + s2_pitch + x));
      auto diff_next12 = _mm_subs_epu8(next1, next2);
      auto diff_next21 = _mm_subs_epu8(next2, next1);
      auto abs_diff_next = _mm_or_si128(diff_next12, diff_next21); // xmm0

      auto curr1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp1 + x));
      auto curr2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp2 + x));
      auto diff_curr12 = _mm_subs_epu8(curr1, curr2);
      auto diff_curr21 = _mm_subs_epu8(curr2, curr1);
      auto abs_diff_curr = _mm_or_si128(diff_curr12, diff_curr21); // xmm2

      auto prev1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp1 - s1_pitch + x));
      auto prev2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp2 - s2_pitch + x));
      auto diff_prev12 = _mm_subs_epu8(prev1, prev2);
      auto diff_prev21 = _mm_subs_epu8(prev2, prev1);
      auto abs_diff_prev = _mm_or_si128(diff_prev12, diff_prev21); // xmm1

      auto cmp_prev = _mm_cmpeq_epi8(_mm_adds_epu8(abs_diff_prev, thresh), full_ff);
      auto cmp_curr = _mm_cmpeq_epi8(_mm_adds_epu8(abs_diff_curr, thresh), full_ff);
      auto cmp_next = _mm_cmpeq_epi8(_mm_adds_epu8(abs_diff_next, thresh), full_ff);
      auto cmp = _mm_or_si128(_mm_or_si128(cmp_prev, cmp_curr), cmp_next);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), cmp);
    }
    srcp1 += s1_pitch;
    srcp2 += s2_pitch;
    dstp += dst_pitch;
  }
}


void TFMPP::buildMotionMask2_SSE2(const uint8_t *srcp1, const uint8_t *srcp2,
  const uint8_t *srcp3, uint8_t *dstp, int s1_pitch, int s2_pitch,
  int s3_pitch, int dst_pitch, int width, int height) const
{
  __m128i thresh = _mm_set1_epi8((char)(std::max(std::min(255 - mthresh - 1, 255), 0)));
  __m128i all_ff = _mm_set1_epi8(-1);
  __m128i onesByte = _mm_set1_epi8(0x01);
  __m128i twosByte = _mm_set1_epi8(0x02);
  __m128i foursByte = _mm_set1_epi8(0x04);
  __m128i eightsByte = _mm_set1_epi8(0x08);
  __m128i sixteensByte = _mm_set1_epi8(0x10);
  __m128i thirtytwosByte = _mm_set1_epi8(0x20);
  memset(dstp - dst_pitch, 0xFF, dst_pitch);
  memset(dstp + dst_pitch*height, 0xFF, dst_pitch);
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto next1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp1 + s1_pitch + x)); // prv?
      auto next2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp2 + s2_pitch + x)); // src?
      auto next3 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp3 + s3_pitch + x)); // nxt?

      auto absdiff12 = _mm_or_si128(_mm_subs_epu8(next1, next2), _mm_subs_epu8(next2, next1));
      auto absdiff23 = _mm_or_si128(_mm_subs_epu8(next2, next3), _mm_subs_epu8(next3, next2));
      auto cmp12 = _mm_cmpeq_epi8(_mm_adds_epu8(absdiff12, thresh), all_ff);
      auto cmp23 = _mm_cmpeq_epi8(_mm_adds_epu8(absdiff23, thresh), all_ff);
      auto masked_by_01_02 = _mm_or_si128(_mm_and_si128(cmp12, onesByte), _mm_and_si128(cmp23, twosByte));

      auto curr1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp1 + x)); // prv?
      auto curr2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp2 + x)); // src?
      auto curr3 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp3 + x)); // nxt?

      absdiff12 = _mm_or_si128(_mm_subs_epu8(curr1, curr2), _mm_subs_epu8(curr2, curr1));
      absdiff23 = _mm_or_si128(_mm_subs_epu8(curr2, curr3), _mm_subs_epu8(curr3, curr2));
      cmp12 = _mm_cmpeq_epi8(_mm_adds_epu8(absdiff12, thresh), all_ff);
      cmp23 = _mm_cmpeq_epi8(_mm_adds_epu8(absdiff23, thresh), all_ff);
      auto masked_by_04_08 = _mm_or_si128(_mm_and_si128(cmp12, foursByte), _mm_and_si128(cmp23, eightsByte));
      
      auto masked_by_01_02_04_08 = _mm_or_si128(masked_by_01_02, masked_by_04_08);

      auto prev1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp1 - s1_pitch + x)); // prv?
      auto prev2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp2 - s2_pitch + x)); // src?
      auto prev3 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp3 - s3_pitch + x)); // nxt?

      absdiff12 = _mm_or_si128(_mm_subs_epu8(prev1, prev2), _mm_subs_epu8(prev2, prev1));
      absdiff23 = _mm_or_si128(_mm_subs_epu8(prev2, prev3), _mm_subs_epu8(prev3, prev2));
      cmp12 = _mm_cmpeq_epi8(_mm_adds_epu8(absdiff12, thresh), all_ff);
      cmp23 = _mm_cmpeq_epi8(_mm_adds_epu8(absdiff23, thresh), all_ff);
      auto masked_by_10_20 = _mm_or_si128(_mm_and_si128(cmp12, sixteensByte), _mm_and_si128(cmp23, thirtytwosByte));

      auto masked_by_01_02_04_08_10_20 = _mm_or_si128(masked_by_01_02_04_08, masked_by_10_20);

      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), masked_by_01_02_04_08_10_20);

    }
    srcp1 += s1_pitch;
    srcp2 += s2_pitch;
    srcp3 += s3_pitch;
    dstp += dst_pitch;
  }
}

// not the same as in TDeint. Here 0xFF instead of 0x3C
void TFMPP::denoiseYUY2(PlanarFrame *mask) const
{
  uint8_t *maskw = mask->GetPtr();
  const int mask_pitch = mask->GetPitch();
  const int Height = mask->GetHeight();
  const int Width = mask->GetWidth();
  uint8_t *maskwp = maskw - mask_pitch;
  uint8_t *maskwn = maskw + mask_pitch;
  for (int y = 1; y < Height - 1; ++y)
  {
    maskwp += mask_pitch;
    maskw += mask_pitch;
    maskwn += mask_pitch;
    for (int x = 4; x < Width - 4; ++x)
    {
      if (maskw[x] == 0xFF)
      {
        if (maskwp[x - 2] == 0xFF) goto check_chroma;
        if (maskwp[x] == 0xFF) goto check_chroma;
        if (maskwp[x + 2] == 0xFF) goto check_chroma;
        if (maskw[x - 2] == 0xFF) goto check_chroma;
        if (maskw[x + 2] == 0xFF) goto check_chroma;
        if (maskwn[x - 2] == 0xFF) goto check_chroma;
        if (maskwn[x] == 0xFF) goto check_chroma;
        if (maskwn[x + 2] == 0xFF) goto check_chroma;
        maskw[x] = 0;
      }
    check_chroma:
      ++x;
      if (maskw[x] == 0xFF)
      {
        if (maskwp[x - 4] == 0xFF) continue;
        if (maskwp[x] == 0xFF) continue;
        if (maskwp[x + 4] == 0xFF) continue;
        if (maskw[x - 4] == 0xFF) continue;
        if (maskw[x + 4] == 0xFF) continue;
        if (maskwn[x - 4] == 0xFF) continue;
        if (maskwn[x] == 0xFF) continue;
        if (maskwn[x + 4] == 0xFF) continue;
        maskw[x] = 0;
      }
    }
  }
}

void TFMPP::linkYUY2(PlanarFrame *mask) const
{
  uint8_t *maskw = mask->GetPtr();
  const int mask_pitch = mask->GetPitch();
  const int Height = mask->GetHeight();
  const int Width = mask->GetWidth() >> 2;
  for (int y = 1; y < Height - 1; ++y)
  {
    maskw += mask_pitch;
    for (int x = 0; x < Width; ++x)
    {
      if ((((unsigned int*)maskw)[x] & 0x00FF00FF) == 0x00FF00FF)
      {
        ((unsigned int*)maskw)[x] = 0xFFFFFFFF;
      }
    }
  }
}

// mask-only no need HBD here
// Differences
// TFMPP::denoisePlanar: PlanarFrame, 0xFF, TDeinterlace:PVideoFrame 0x3C
void TFMPP::denoisePlanar(PlanarFrame *mask) const
{
  const int np = mask->NumComponents();
  for (int b = 0; b < np; ++b)
  {
    uint8_t *maskpp = mask->GetPtr(b);
    const int msk_pitch = mask->GetPitch(b);
    uint8_t *maskp = maskpp + msk_pitch;
    uint8_t *maskpn = maskp + msk_pitch;
    const int Height = mask->GetHeight(b);
    const int Width = mask->GetWidth(b);
    for (int y = 1; y < Height - 1; ++y)
    {
      for (int x = 1; x < Width - 1; ++x)
      {
        if (maskp[x] == 0xFF)
        {
          if (maskpp[x - 1] == 0xFF) continue;
          if (maskpp[x] == 0xFF) continue;
          if (maskpp[x + 1] == 0xFF) continue;
          if (maskp[x - 1] == 0xFF) continue;
          if (maskp[x + 1] == 0xFF) continue;
          if (maskpn[x - 1] == 0xFF) continue;
          if (maskpn[x] == 0xFF) continue;
          if (maskpn[x + 1] == 0xFF) continue;
          maskp[x] = 0;
        }
      }
      maskpp += msk_pitch;
      maskp += msk_pitch;
      maskpn += msk_pitch;
    }
  }
}

template<int planarType>
void TFMPP::linkPlanar(PlanarFrame* mask) const
{
  uint8_t* maskpY = mask->GetPtr(0);
  uint8_t* maskpV = mask->GetPtr(2);
  uint8_t* maskpU = mask->GetPtr(1);
  const int mask_pitchY = mask->GetPitch(0);
  const int mask_pitchUV = mask->GetPitch(2);
  const int HeightUV = mask->GetHeight(2);
  const int WidthUV = mask->GetWidth(2);

  if constexpr (planarType == 420) 
  {
    uint8_t* maskppY = maskpY - mask_pitchY; // prev Y use at 420
    uint8_t* maskpnY = maskpY + mask_pitchY; // next Y
    uint8_t* maskpnnY = maskpY + 2 * mask_pitchY; // nextnextY used at 420
    for (int y = 1; y < HeightUV - 1; ++y)
    {
      maskppY = maskpnY; // prev = next
      maskpY = maskpnnY; // current = nextnext
      maskpnY += mask_pitchY * 2; // YV12 vertical subsampling
      maskpnnY += mask_pitchY * 2;
      maskpV += mask_pitchUV;
      maskpU += mask_pitchUV;
      for (int x = 0; x < WidthUV; ++x)
      {
        if ((((unsigned short*)maskpY)[x] == (unsigned short)0xFFFF) &&
          (((unsigned short*)maskpnY)[x] == (unsigned short)0xFFFF) &&
          (((y & 1) && (((unsigned short*)maskppY)[x] == (unsigned short)0xFFFF)) ||
            (!(y & 1) && (((unsigned short*)maskpnnY)[x] == (unsigned short)0xFFFF))))
        {
          maskpV[x] = maskpU[x] = 0xFF;
        }
      }
    }
  }
  else { // 422 444 411
    for (int y = 1; y < HeightUV - 1; ++y)
    {
      maskpY += mask_pitchY;
      maskpV += mask_pitchUV;
      maskpU += mask_pitchUV;
      for (int x = 0; x < WidthUV; ++x)
      {
        if constexpr (planarType == 422) {
          if (((unsigned short*)maskpY)[x] == (unsigned short)0xFFFF) // horizontal subsampling
          {
            maskpV[x] = maskpU[x] = 0xFF;
          }
        }
        else if constexpr (planarType == 444) {
          if (maskpY[x] == 0xFF)
          {
            maskpV[x] = maskpU[x] = 0xFF;
          }
        }
        else if constexpr (planarType == 411) {
          if (((uint32_t*)maskpY)[x] == (uint32_t)0xFFFFFFFF) // horizontal subsampling
          {
            maskpV[x] = maskpU[x] = 0xFF;
          }
        }
      }
    }
  }
}

void TFMPP::BlendDeint(PVideoFrame& src, PlanarFrame* mask, PVideoFrame& dst, bool nomask,
  const VideoInfo& vi) const
{
  if (vi.BitsPerComponent() == 8)
    BlendDeint_core<uint8_t>(src, mask, dst, nomask, vi);
  else
    BlendDeint_core<uint16_t>(src, mask, dst, nomask, vi);
}

template<typename pixel_t>
void TFMPP::BlendDeint_core(PVideoFrame &src, PlanarFrame *mask, PVideoFrame &dst, bool nomask,
  const VideoInfo& vi) const
{
  bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;

  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const pixel_t *srcp = reinterpret_cast<const pixel_t *>(src->GetReadPtr(plane));
    const int src_pitch = src->GetPitch(plane) / sizeof(pixel_t);

    const int rowsize = src->GetRowSize(plane);
    const int width = rowsize / sizeof(pixel_t);
    const int height = src->GetHeight(plane);

    const pixel_t* srcpp = srcp - src_pitch;
    const pixel_t* srcpn = srcp + src_pitch;

    pixel_t *dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));
    const int dst_pitch = dst->GetPitch(plane) / sizeof(pixel_t);

    const uint8_t *maskp = mask->GetPtr(b);
    const int msk_pitch = mask->GetPitch(b);
    
    // top line
    for (int x = 0; x < width; ++x)
      dstp[x] = (srcp[x] + srcpn[x] + 1) >> 1;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    dstp += dst_pitch;
    maskp += msk_pitch;
    const int lines_to_process = height - 2;
    if (nomask)
    {
      // fixme: hbd SIMD
      if (sizeof(pixel_t) == 1 && use_sse2)
        blendDeintMask_SSE2<false>((const uint8_t *)srcp, (uint8_t*)dstp, nullptr, src_pitch, dst_pitch, 0, width, lines_to_process);
      else
        blendDeintMask_C<pixel_t, false>(srcp, dstp, nullptr, src_pitch, dst_pitch, 0, width, lines_to_process);
    }
    else
    {
      // with mask
      if (sizeof(pixel_t) == 1 && use_sse2)
        blendDeintMask_SSE2<true>((const uint8_t*)srcp, (uint8_t*)dstp, maskp, src_pitch, dst_pitch, msk_pitch, width, lines_to_process);
      else
        blendDeintMask_C<pixel_t, true>(srcp, dstp, maskp, src_pitch, dst_pitch, msk_pitch, width, lines_to_process);
    }
    srcpp += src_pitch * lines_to_process;
    srcp += src_pitch * lines_to_process;
    // srcpn += src_pitch * lines_to_process; // no forther use
    // maskp += msk_pitch * lines_to_process; // no further use
    dstp += dst_pitch * lines_to_process;
    // bottom line
    for (int x = 0; x < width; ++x)
      dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
  }
}


static AVS_FORCEINLINE __m128i _MM_BLENDV_EPI8(__m128i const& a, __m128i const& b, __m128i const& mask) {
  //return  _mm_blendv_epi8 (a, b, mask);
  auto andop = _mm_and_si128(mask, b);
  auto andnop = _mm_andnot_si128(mask, a);
  return _mm_or_si128(andop, andnop);
}

template<bool with_mask>
void blendDeintMask_SSE2(const uint8_t *srcp, uint8_t *dstp,
  const uint8_t *maskp, int src_pitch, int dst_pitch, int msk_pitch,
  int width, int height)
{
  auto zero = _mm_setzero_si128();
  auto twosWord = _mm_set1_epi16(2);
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto prev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch + x));
      auto curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      auto next = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch + x));
      auto prev_lo = _mm_unpacklo_epi8(prev, zero);
      auto curr_lo = _mm_unpacklo_epi8(curr, zero);
      auto next_lo = _mm_unpacklo_epi8(next, zero);
      auto prev_hi = _mm_unpackhi_epi8(prev, zero);
      auto curr_hi = _mm_unpackhi_epi8(curr, zero);
      auto next_hi = _mm_unpackhi_epi8(next, zero);
      auto curr_lo_mul2 = _mm_slli_epi16(curr_lo, 1);
      auto curr_hi_mul2 = _mm_slli_epi16(curr_hi, 1);
      auto sum_lo = _mm_add_epi16(prev_lo, _mm_add_epi16(curr_lo_mul2, next_lo));
      auto sum_hi = _mm_add_epi16(prev_hi, _mm_add_epi16(curr_hi_mul2, next_hi));
      auto res_lo = _mm_srli_epi16(_mm_add_epi16(sum_lo, twosWord), 2); // (p + c*2 + n + 2) >> 2
      auto res_hi = _mm_srli_epi16(_mm_add_epi16(sum_hi, twosWord), 2); // (p + c*2 + n + 2) >> 2
      auto res = _mm_packus_epi16(res_lo, res_hi);

      if constexpr (with_mask) {
        auto mask = _mm_load_si128(reinterpret_cast<const __m128i*>(maskp + x));
        res = _MM_BLENDV_EPI8(curr, res, mask); // if mask then res else curr
      } else {
        (void)maskp;
        (void)msk_pitch;
      }
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
    if constexpr(with_mask)
      maskp += msk_pitch;
  }
}

template<typename pixel_t, bool with_mask>
void blendDeintMask_C(const pixel_t* srcp, pixel_t* dstp,
  const uint8_t* maskp, int src_pitch, int dst_pitch, int msk_pitch,
  int width, int height)
{
  while (height--) {
    const pixel_t* srcpp = srcp - src_pitch;
    const pixel_t* srcpn = srcp + src_pitch;
    for (int x = 0; x < width; ++x)
    {
      if (!with_mask || (with_mask && maskp[x] == 0xFF))
        dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;
      else
        dstp[x] = srcp[x];
    }
    srcp += src_pitch;
    dstp += dst_pitch;
    if constexpr (with_mask)
      maskp += msk_pitch;
  }
}

void TFMPP::CubicDeint(PVideoFrame& src, PlanarFrame* mask, PVideoFrame& dst, bool nomask,
  int field, const VideoInfo& vi) const
{
  if (vi.IsYUY2())
    CubicDeint_core<uint8_t, 8>(src, mask, dst, nomask, field, vi);
  else {
    switch (vi.BitsPerComponent()) {
    case 8: CubicDeint_core<uint8_t, 8>(src, mask, dst, nomask, field, vi); break;
    case 10: CubicDeint_core<uint16_t, 10>(src, mask, dst, nomask, field, vi); break;
    case 12: CubicDeint_core<uint16_t, 12>(src, mask, dst, nomask, field, vi); break;
    case 14: CubicDeint_core<uint16_t, 14>(src, mask, dst, nomask, field, vi); break;
    case 16: CubicDeint_core<uint16_t, 16>(src, mask, dst, nomask, field, vi); break;
    }
  }
}

template<typename pixel_t, int bits_per_pixel>
void TFMPP::CubicDeint_core(PVideoFrame &src, PlanarFrame *mask, PVideoFrame &dst, bool nomask,
  int field, const VideoInfo& vi) const
{
  bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;

  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];

    const pixel_t *srcp = reinterpret_cast<const pixel_t *>(src->GetReadPtr(plane));
    // !! yes, double;
    const int src_pitch = src->GetPitch(plane) * 2 / sizeof(pixel_t);

    const int rowsize = src->GetRowSize(plane);
    const int width = rowsize / sizeof(pixel_t);
    const int height = src->GetHeight(plane);

    pixel_t *dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));
    const int dst_pitch = (dst->GetPitch(plane) << 1) / sizeof(pixel_t);

    const uint8_t *maskp = mask->GetPtr(b);
    const int msk_pitch = mask->GetPitch(b) << 1;
    
    srcp += (src_pitch >> 1)*(3 - field);
    dstp += (dst_pitch >> 1)*(2 - field);
    maskp += (msk_pitch >> 1)*(2 - field);

    const pixel_t *srcpp = srcp - src_pitch;
    const pixel_t *srcppp = srcpp - src_pitch;
    const pixel_t* srcpn = srcp + src_pitch;
    const pixel_t*srcr = srcp - (src_pitch >> 1);

    // top orphan
    if (field == 0)
      BitBlt(dst->GetWritePtr(plane), (dst_pitch >> 1) * sizeof(pixel_t),
        src->GetReadPtr(plane) + (src_pitch >> 1) * sizeof(pixel_t), (src_pitch >> 1) * sizeof(pixel_t), rowsize, 1);
    
    if (nomask)
    {
      // top
      for (int x = 0; x < width; ++x)
        dstp[x] = (srcp[x] + srcpp[x] + 1) >> 1;
      srcppp += src_pitch;
      srcpp += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      dstp += dst_pitch;
      // middle
      const int lines_to_process = height / 2 - 3;
      if (bits_per_pixel == 8 && use_sse2)
      {
        // false: no mask
        cubicDeintMask_SSE2<false>((const uint8_t *)srcp, (uint8_t*)dstp, nullptr, src_pitch, dst_pitch, 0, width, lines_to_process);
      }
      else
      {
        cubicDeintMask_C<pixel_t, bits_per_pixel, false>(srcp, dstp, nullptr, src_pitch, dst_pitch, 0, width, lines_to_process);
      }
      srcppp += src_pitch * lines_to_process;
      srcpp += src_pitch * lines_to_process;
      srcp += src_pitch * lines_to_process;
      srcpn += src_pitch * lines_to_process;
      dstp += dst_pitch * lines_to_process;
      // bottom
      for (int x = 0; x < width; ++x)
        dstp[x] = (srcp[x] + srcpp[x] + 1) >> 1;
    }
    else
    {
      // with mask
      // top
      for (int x = 0; x < width; ++x)
      {
        if (maskp[x] == 0xFF)
          dstp[x] = (srcp[x] + srcpp[x] + 1) >> 1;
        else dstp[x] = srcr[x];
      }
      srcppp += src_pitch;
      srcpp += src_pitch;
      srcr += src_pitch;
      srcp += src_pitch;
      srcpn += src_pitch;
      maskp += msk_pitch;
      dstp += dst_pitch;
      // middle
      const int lines_to_process = height / 2 - 3;
      if (bits_per_pixel == 8 && use_sse2)
      {
        // fixme: hbd SIMD sse2 for 10+ bits
        // true: with_mask
        cubicDeintMask_SSE2<true>((const uint8_t*)srcp, (uint8_t*)dstp, maskp, src_pitch, dst_pitch, msk_pitch, width, lines_to_process);
      }
      else
      {
        //for (int y = 4 - field; y < height - 3; y += 2)
        cubicDeintMask_C<pixel_t, bits_per_pixel, true>(srcp, dstp, maskp, src_pitch, dst_pitch, msk_pitch, width, lines_to_process);
      }
      srcppp += src_pitch * lines_to_process;
      srcpp += src_pitch * lines_to_process;
      srcr += src_pitch * lines_to_process;
      srcp += src_pitch * lines_to_process;
      srcpn += src_pitch * lines_to_process;
      maskp += msk_pitch * lines_to_process;
      dstp += dst_pitch * lines_to_process;
      // bottom
      for (int x = 0; x < width; ++x)
      {
        if (maskp[x] == 0xFF)
          dstp[x] = (srcp[x] + srcpp[x] + 1) >> 1;
        else
          dstp[x] = srcr[x];
      }
    }
    // bottom orphan
    if (field == 1)
      BitBlt(dst->GetWritePtr(plane) + (height - 1)*(dst_pitch >> 1) * sizeof(pixel_t), (dst_pitch >> 1) * sizeof(pixel_t),
        src->GetReadPtr(plane) + (height - 2)*(src_pitch >> 1) * sizeof(pixel_t), (src_pitch >> 1) * sizeof(pixel_t), rowsize, 1);
  }
}


template<bool with_mask>
void cubicDeintMask_SSE2(const uint8_t *srcp, uint8_t *dstp,
  const uint8_t *maskp, int src_pitch, int dst_pitch, int msk_pitch,
  int width, int height)
{
  /*
  if (maskp[x] == 0xFF)
  {
    const int temp = (19 * (srcpp[x] + srcp[x]) - 3 * (srcppp[x] + srcpn[x]) + 16) >> 5;
    if (temp > 255) dstp[x] = 255;
    else if (temp < 0) dstp[x] = 0;
    else dstp[x] = temp;
  }
  else 
    dstp[x] = srcr[x];
  */
  const int s1 = src_pitch >> 1; // pitch was multiplied *2 before the call

  auto zero = _mm_setzero_si128();
  auto threeWord = _mm_set1_epi16(3);
  auto sixteenWord = _mm_set1_epi16(16);
  auto nineteenWord = _mm_set1_epi16(19);
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto prevprev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch * 2 + x));
      auto prev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch + x));
      auto curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      auto next = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch + x));
      auto prevprev_lo = _mm_unpacklo_epi8(prevprev, zero);
      auto next_lo = _mm_unpacklo_epi8(next, zero);
      auto prevprev_hi = _mm_unpackhi_epi8(prevprev, zero);
      auto next_hi = _mm_unpackhi_epi8(next, zero);

      auto pp_plus_n_lo = _mm_add_epi16(prevprev_lo, next_lo); // pp_lo + n_lo
      auto pp_plus_n_hi = _mm_add_epi16(prevprev_hi, next_hi); // pp_hi + n_hi
      auto pp_plus_n_mul3_lo = _mm_mullo_epi16(pp_plus_n_lo, threeWord); // *3
      auto pp_plus_n_mul3_hi = _mm_mullo_epi16(pp_plus_n_hi, threeWord);

      auto prev_lo = _mm_unpacklo_epi8(prev, zero);
      auto curr_lo = _mm_unpacklo_epi8(curr, zero);
      auto prev_hi = _mm_unpackhi_epi8(prev, zero);
      auto curr_hi = _mm_unpackhi_epi8(curr, zero);

      auto p_plus_c_lo = _mm_add_epi16(prev_lo, curr_lo); // p_lo + c_lo
      auto p_plus_c_hi = _mm_add_epi16(prev_hi, curr_hi); // p_hi + c_hi
      auto p_plus_c_mul19_lo = _mm_mullo_epi16(p_plus_c_lo, nineteenWord); // *19
      auto p_plus_c_mul19_hi = _mm_mullo_epi16(p_plus_c_hi, nineteenWord);

      auto sub_lo = _mm_subs_epu16(p_plus_c_mul19_lo, pp_plus_n_mul3_lo); // *19 - *3
      auto sub_hi = _mm_subs_epu16(p_plus_c_mul19_hi, pp_plus_n_mul3_hi);

      auto res_lo = _mm_srli_epi16(_mm_add_epi16(sub_lo, sixteenWord), 5); // +16, >> 5
      auto res_hi = _mm_srli_epi16(_mm_add_epi16(sub_hi, sixteenWord), 5);
      auto res = _mm_packus_epi16(res_lo, res_hi);

      if constexpr (with_mask) {
        auto mask = _mm_load_si128(reinterpret_cast<const __m128i*>(maskp + x));
        // s1 is the normal src_pitch (half of the doubled)
        auto curr2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp - s1 + x)); // == srcp - s1 + x
        res = _MM_BLENDV_EPI8(curr2, res, mask); // if mask then res else curr
      } else {
        (void)maskp;
        (void)msk_pitch;
      }
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
    if constexpr(with_mask)
      maskp += msk_pitch;
  }
}

template<typename pixel_t, int bits_per_pixel, bool with_mask>
void cubicDeintMask_C(const pixel_t* srcp, pixel_t* dstp,
  const uint8_t* maskp, int src_pitch, int dst_pitch, int msk_pitch,
  int width, int height)
{
  while (height--) {
    const pixel_t* srcppp = srcp - src_pitch * 2;
    const pixel_t* srcpp = srcp - src_pitch;
    const pixel_t* srcpn = srcp + src_pitch;
    const pixel_t* srcr = srcp - (src_pitch >> 1); // came doubled
    for (int x = 0; x < width; ++x)
    {
      if (!with_mask || (with_mask && maskp[x] == 0xFF))
      {
        const int temp = cubicInt<bits_per_pixel>(srcppp[x], srcpp[x], srcp[x], srcpn[x]);
        dstp[x] = temp;
      }
      else 
        dstp[x] = srcr[x];
    }
    srcp += src_pitch;
    dstp += dst_pitch;
    if constexpr (with_mask)
      maskp += msk_pitch;
  }
}


void TFMPP::destroyHint(const VideoInfo& vi, PVideoFrame& dst, unsigned int hint)
{
  if (vi.ComponentSize() == 1)
    destroyHint_core<uint8_t>(dst, hint);
  else
    destroyHint_core<uint16_t>(dst, hint);
}

template<typename pixel_t>
void TFMPP::destroyHint_core(PVideoFrame &dst, unsigned int hint)
{
  pixel_t* p = reinterpret_cast<pixel_t*>(dst->GetWritePtr(PLANAR_Y));
  uint8_t i;
  if (hint & 0x80)
  {
    hint >>= 8;
    for (i = 0; i < 32; ++i)
    {
      *p &= ~1;
      *p++ |= ((MAGIC_NUMBER_2 & (1 << i)) >> i);
    }
    for (i = 0; i < 32; ++i)
    {
      *p &= ~1;
      *p++ |= ((hint & (1 << i)) >> i);
    }
  }
  else
  {
    for (int i = 0; i < 64; ++i)
      *p++ &= ~1;
  }
}

void TFMPP::putHint(const VideoInfo & vi, PVideoFrame& dst, int field, unsigned int hint)
{
  if (vi.ComponentSize() == 1)
    return putHint_core<uint8_t>(dst, field, hint);
  else
    return putHint_core<uint16_t>(dst, field, hint);
}

template<typename pixel_t>
void TFMPP::putHint_core(PVideoFrame &dst, int field, unsigned int hint)
{
  pixel_t *p = reinterpret_cast<pixel_t *>(dst->GetWritePtr(PLANAR_Y));
  unsigned int i;
  hint &= (D2VFILM | 0xFF80);
  if (field == 1)
  {
    hint |= TOP_FIELD;
    hint |= ISDT;
  }
  else hint |= ISDB;
  for (i = 0; i < 32; ++i)
  {
    *p &= ~1;
    *p++ |= ((MAGIC_NUMBER & (1 << i)) >> i);
  }
  for (i = 0; i < 32; ++i)
  {
    *p &= ~1;
    *p++ |= ((hint & (1 << i)) >> i);
  }
}

bool TFMPP::getHint(const VideoInfo& vi, PVideoFrame& src, int& field, bool& combed, unsigned int& hint)
{
  if (vi.ComponentSize() == 1)
    return getHint_core<uint8_t>(src, field, combed, hint);
  else
    return getHint_core<uint16_t>(src, field, combed, hint);
}

template<typename pixel_t>
bool TFMPP::getHint_core(PVideoFrame &src, int &field, bool &combed, unsigned int &hint)
{
  field = -1; combed = false; hint = 0;
  const pixel_t *srcp = reinterpret_cast<const pixel_t *>(src->GetReadPtr(PLANAR_Y));
  unsigned int i, magic_number = 0;
  for (i = 0; i < 32; ++i)
  {
    magic_number |= ((*srcp++ & 1) << i);
  }
  if (magic_number != MAGIC_NUMBER) return false;
  for (i = 0; i < 32; ++i)
  {
    hint |= ((*srcp++ & 1) << i);
  }
  if (hint & 0xFFFF0000) return false;
  if (hint&TOP_FIELD) field = 1;
  else field = 0;
  if (hint&COMBED) combed = true;
  int value = hint & 0x07;
  if (value == 5) { combed = true; field = 0; }
  else if (value == 6) { combed = true; field = 1; }
  return true;
}

bool TFMPP::getProperties(const PVideoFrame& src, int& field, bool& combed, IScriptEnvironment* env) const
{
  field = -1; combed = false;
  int _field = -1; 
  bool _combed = false;

  const AVSMap* props = env->getFramePropsRO(src);

  if (env->propNumElements(props, PROP_TFMField) == 1)
    _field = int64ToIntS(env->propGetInt(props, PROP_TFMField, 0, nullptr));
  else
    return false; // no such frame prop

  if (env->propNumElements(props, PROP_Combed) == 1)
    _combed = !!env->propGetInt(props, PROP_Combed, 0, nullptr);
  else
    return false; // no such frame prop

  field = _field;
  combed = _combed;
  return true;
}

void TFMPP::getSetOvr(int n)
{
  if (setArray.size() == 0) return;
  mthresh = mthresh_origSaved;
  PP = PP_origSaved;
  for (int x = 0; x < (int)setArray.size(); x += 4)
  {
    if (n >= setArray[x + 1] && n <= setArray[x + 2])
    {
      if (setArray[x] == 80) PP = setArray[x + 3]; // P
      else if (setArray[x] == 77) mthresh = setArray[x + 3]; // M
    }
  }
}

void TFMPP::copyField(PVideoFrame &dst, PVideoFrame &src, const VideoInfo& vi, int field) const
{
  // bit depth independent
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;

  int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    const int dst_pitch = dst->GetPitch(plane);
    const int src_pitch = src->GetPitch(plane);
    if (field == 0)
      BitBlt(dst->GetWritePtr(plane), dst_pitch, src->GetReadPtr(plane) + src_pitch, 
        src_pitch, src->GetRowSize(plane), 1);
    BitBlt(dst->GetWritePtr(plane) + dst_pitch *(1 - field),
      dst->GetPitch(plane) * 2, src->GetReadPtr(plane) + src_pitch *(1 - field),
      src->GetPitch(plane) * 2, src->GetRowSize(plane), src->GetHeight(plane) >> 1);
    if (field == 1)
      BitBlt(dst->GetWritePtr(plane) + dst_pitch *(dst->GetHeight(plane) - 1),
        dst->GetPitch(plane), src->GetReadPtr(plane) + src_pitch *(src->GetHeight(plane) - 2),
        src->GetPitch(plane), src->GetRowSize(plane), 1);
  }
}

void TFMPP::writeDisplay(PVideoFrame &dst, const VideoInfo &vi, int n, int field) const
{
  char buf[256];
  sprintf(buf, "TFMPP %s by tritical ", VERSION);
  Draw(dst, 0, 0, buf, vi);
  sprintf(buf, "field = %d  PP = %d  mthresh = %d ", field, PP, mthresh);
  Draw(dst, 0, 1, buf, vi);
  sprintf(buf, "frame: %d  (COMBED - DEINTERLACED)! ", n);
  Draw(dst, 0, 2, buf, vi);
  /*
  kept for reference: VS does not display debug, instead puts them into a frame property
  VSMap *props = vsapi->getFramePropsRW(dst);
  vsapi->propSetData(props, PROP_TFMDisplay, text.c_str(), text.size(), paReplace);  
  */
}

void TFMPP::elaDeint(PVideoFrame& dst, PlanarFrame* mask, PVideoFrame& src, bool nomask, int field, const VideoInfo& vi) const
{
  if (!vi.IsYUY2()) {
    switch (vi.BitsPerComponent()) {
    case 8: elaDeintPlanar<uint8_t, 8>(dst, mask, src, nomask, field, vi); break;
    case 10: elaDeintPlanar<uint16_t, 10>(dst, mask, src, nomask, field, vi); break;
    case 12: elaDeintPlanar<uint16_t, 12>(dst, mask, src, nomask, field, vi); break;
    case 14: elaDeintPlanar<uint16_t, 14>(dst, mask, src, nomask, field, vi); break;
    case 16: elaDeintPlanar<uint16_t, 16>(dst, mask, src, nomask, field, vi); break;
    }
  }
  else
    elaDeintYUY2(dst, mask, src, nomask, field);
}

// totally different from TDeinterlace ELADeintPlanar
template<typename pixel_t, int bits_per_pixel>
void TFMPP::elaDeintPlanar(PVideoFrame &dst, PlanarFrame *mask, PVideoFrame &src, bool nomask, int field, const VideoInfo &vi) const
{
  const pixel_t *srcpY = reinterpret_cast<const pixel_t *>(src->GetReadPtr(PLANAR_Y));
  const pixel_t *srcpV = reinterpret_cast<const pixel_t*>(src->GetReadPtr(PLANAR_V));
  const pixel_t *srcpU = reinterpret_cast<const pixel_t*>(src->GetReadPtr(PLANAR_U));
  int src_pitchY = src->GetPitch(PLANAR_Y) / sizeof(pixel_t);
  int src_pitchUV = src->GetPitch(PLANAR_V) / sizeof(pixel_t);
  
  const int WidthY = src->GetRowSize(PLANAR_Y) / sizeof(pixel_t);
  const int WidthUV = src->GetRowSize(PLANAR_V) / sizeof(pixel_t);
  const int HeightY = src->GetHeight(PLANAR_Y);
  const int HeightUV = src->GetHeight(PLANAR_V);
  
  pixel_t *dstpY = reinterpret_cast<pixel_t*>(dst->GetWritePtr(PLANAR_Y));
  pixel_t *dstpV = reinterpret_cast<pixel_t*>(dst->GetWritePtr(PLANAR_V));
  pixel_t *dstpU = reinterpret_cast<pixel_t*>(dst->GetWritePtr(PLANAR_U));
  int dst_pitchY = dst->GetPitch(PLANAR_Y) / sizeof(pixel_t);
  int dst_pitchUV = dst->GetPitch(PLANAR_V) / sizeof(pixel_t);

  const uint8_t *maskpY = mask->GetPtr(0);
  const uint8_t *maskpV = mask->GetPtr(2);
  const uint8_t *maskpU = mask->GetPtr(1);
  int mask_pitchY = mask->GetPitch(0);
  int mask_pitchUV = mask->GetPitch(2);

  srcpY += src_pitchY*(3 - field);
  srcpV += src_pitchUV*(3 - field);
  srcpU += src_pitchUV*(3 - field);
  dstpY += dst_pitchY*(2 - field);
  dstpV += dst_pitchUV*(2 - field);
  dstpU += dst_pitchUV*(2 - field);
  maskpY += mask_pitchY*(2 - field);
  maskpV += mask_pitchUV*(2 - field);
  maskpU += mask_pitchUV*(2 - field);
  src_pitchY <<= 1;
  src_pitchUV <<= 1;
  dst_pitchY <<= 1;
  dst_pitchUV <<= 1;
  mask_pitchY <<= 1;
  mask_pitchUV <<= 1;

  const pixel_t *srcppY = srcpY - src_pitchY;
  const pixel_t *srcpppY = srcppY - src_pitchY;
  const pixel_t *srcpnY = srcpY + src_pitchY;
  const pixel_t *srcppV = srcpV - src_pitchUV;
  const pixel_t *srcpppV = srcppV - src_pitchUV;
  const pixel_t *srcpnV = srcpV + src_pitchUV;
  const pixel_t *srcppU = srcpU - src_pitchUV;
  const pixel_t *srcpppU = srcppU - src_pitchUV;
  const pixel_t *srcpnU = srcpU + src_pitchUV;
  int stopx = WidthY;
  int startxuv = 0;
  int x, y;
  int stopxuv = WidthUV;
  int Iy1, Iy2, Iye;
  int Ix1, Ix2;
  int edgeS1, edgeS2;
  int sum, sumsq;
  int temp, temp1, temp2;
  int minN, maxN;
  double dir1, dir2, dir, dirF;

  constexpr int bitshift_to_8 = (bits_per_pixel - 8);

  auto square = [](int i)
  {
    return i * i;
  };

  for (y = 2 - field; y < HeightY - 1; y += 2)
  {
    for (x = 0; x < stopx; ++x)
    {
      if (nomask || maskpY[x] == 0xFF)
      {
        if (y > 2 && y < HeightY - 3 && x>3 && x < WidthY - 4)
        {
          // stay in safe 32 bit int by using 8 bit normalized data
          Iy1 = (-srcpY[x - 1] - srcpY[x] - srcpY[x] - srcpY[x + 1] + srcpppY[x - 1] + srcpppY[x] + srcpppY[x] + srcpppY[x + 1]) >> bitshift_to_8;
          Iy2 = (-srcpnY[x - 1] - srcpnY[x] - srcpnY[x] - srcpnY[x + 1] + srcppY[x - 1] + srcppY[x] + srcppY[x] + srcppY[x + 1]) >> bitshift_to_8;
          Ix1 = (srcpppY[x + 1] + srcppY[x + 1] + srcppY[x + 1] + srcpY[x + 1] - srcpppY[x - 1] - srcppY[x - 1] - srcppY[x - 1] - srcpY[x - 1]) >> bitshift_to_8;
          Ix2 = (srcppY[x + 1] + srcpY[x + 1] + srcpY[x + 1] + srcpnY[x + 1] - srcppY[x - 1] - srcpY[x - 1] - srcpY[x - 1] - srcpnY[x - 1]) >> bitshift_to_8;
          edgeS1 = Ix1 * Ix1 + Iy1 * Iy1;
          edgeS2 = Ix2 * Ix2 + Iy2 * Iy2;
          if (edgeS1 < 1600 && edgeS2 < 1600)
          {
            dstpY[x] = (srcppY[x] + srcpY[x] + 1) >> 1;
            continue;
          }
          constexpr int Const10 = 10 << bitshift_to_8;
          if (abs(srcppY[x] - srcpY[x]) < Const10 && (edgeS1 < 1600 || edgeS2 < 1600))
          {
            dstpY[x] = (srcppY[x] + srcpY[x] + 1) >> 1;
            continue;
          }
          // stay in safe 32 bit int by using 8 bit normalized data
          sum = (srcppY[x - 1] + srcppY[x] + srcppY[x + 1] + srcpY[x - 1] + srcpY[x] + srcpY[x + 1]) >> bitshift_to_8;
          sumsq =
            square(srcppY[x - 1] >> bitshift_to_8) +
            square(srcppY[x] >> bitshift_to_8) +
            square(srcppY[x + 1] >> bitshift_to_8) +
            square(srcpY[x - 1] >> bitshift_to_8) +
            square(srcpY[x] >> bitshift_to_8) +
            square(srcpY[x + 1] >> bitshift_to_8);
          if (6 * sumsq - square(sum) < 432)
          {
            dstpY[x] = (srcppY[x] + srcpY[x] + 1) >> 1;
            continue;
          }
          if (Ix1 == 0) dir1 = 3.1415926;
          else
          {
            dir1 = atan(Iy1 / (Ix1*2.0f)) + 1.5707963;
            if (Iy1 >= 0) { if (Ix1 < 0) dir1 += 3.1415927; }
            else { if (Ix1 >= 0) dir1 += 3.1415927; }
            if (dir1 >= 3.1415927) dir1 -= 3.1415927;
          }
          if (Ix2 == 0) dir2 = 3.1415926;
          else
          {
            dir2 = atan(Iy2 / (Ix2*2.0f)) + 1.5707963;
            if (Iy2 >= 0) { if (Ix2 < 0) dir2 += 3.1415927; }
            else { if (Ix2 >= 0) dir2 += 3.1415927; }
            if (dir2 >= 3.1415927) dir2 -= 3.1415927;
          }
          if (fabs(dir1 - dir2) < 0.5)
          {
            if (edgeS1 >= 3600 && edgeS2 >= 3600) dir = (dir1 + dir2) * 0.5;
            else dir = edgeS1 >= edgeS2 ? dir1 : dir2;
          }
          else
          {
            if (edgeS1 >= 5000 && edgeS2 >= 5000)
            {
              // stay in safe 32 bit int by using 8 bit normalized data
              Iye = (-srcpY[x - 1] - srcpY[x] - srcpY[x] - srcpY[x + 1] + srcppY[x - 1] + srcppY[x] + srcppY[x] + srcppY[x + 1]) >> bitshift_to_8;
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
          dirF = 0.5f / tan(dir);
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
                      temp2 = srcpY[x - 4];
                      temp = (srcppY[x + 4] + srcpY[x - 4] + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcpY[x];
                      temp = cubicInt<bits_per_pixel>(srcpppY[x], srcppY[x], srcpY[x], srcpnY[x]);
                    }
                  }
                  else
                  {
                    temp1 = (int)((dirF - 1.5f)*(srcppY[x + 4]) + (2.0f - dirF)*(srcppY[x + 3]) + 0.5f);
                    temp2 = (int)((dirF - 1.5f)*(srcpY[x - 4]) + (2.0f - dirF)*(srcpY[x - 3]) + 0.5f);
                    temp = (int)((dirF - 1.5f)*(srcppY[x + 4] + srcpY[x - 4]) + (2.0f - dirF)*(srcppY[x + 3] + srcpY[x - 3]) + 0.5f);
                  }
                }
                else
                {
                  temp1 = (int)((dirF - 1.0f)*(srcppY[x + 3]) + (1.5f - dirF)*(srcppY[x + 2]) + 0.5f);
                  temp2 = (int)((dirF - 1.0f)*(srcpY[x - 3]) + (1.5f - dirF)*(srcpY[x - 2]) + 0.5f);
                  temp = (int)((dirF - 1.0f)*(srcppY[x + 3] + srcpY[x - 3]) + (1.5f - dirF)*(srcppY[x + 2] + srcpY[x - 2]) + 0.5f);
                }
              }
              else
              {
                temp1 = (int)((dirF - 0.5f)*(srcppY[x + 2]) + (1.0f - dirF)*(srcppY[x + 1]) + 0.5f);
                temp2 = (int)((dirF - 0.5f)*(srcpY[x - 2]) + (1.0f - dirF)*(srcpY[x - 1]) + 0.5f);
                temp = (int)((dirF - 0.5f)*(srcppY[x + 2] + srcpY[x - 2]) + (1.0f - dirF)*(srcppY[x + 1] + srcpY[x - 1]) + 0.5f);
              }
            }
            else
            {
              temp1 = (int)(dirF*(srcppY[x + 1]) + (0.5f - dirF)*(srcppY[x]) + 0.5f);
              temp2 = (int)(dirF*(srcpY[x - 1]) + (0.5f - dirF)*(srcpY[x]) + 0.5f);
              temp = (int)(dirF*(srcppY[x + 1] + srcpY[x - 1]) + (0.5f - dirF)*(srcppY[x] + srcpY[x]) + 0.5f);
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
                      temp2 = srcpY[x + 4];
                      temp = (srcppY[x - 4] + srcpY[x + 4] + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcpY[x];
                      temp = cubicInt<bits_per_pixel>(srcpppY[x], srcppY[x], srcpY[x], srcpnY[x]);
                    }
                  }
                  else
                  {
                    temp1 = (int)((-dirF - 1.5f)*(srcppY[x - 4]) + (2.0f + dirF)*(srcppY[x - 3]) + 0.5f);
                    temp2 = (int)((-dirF - 1.5f)*(srcpY[x + 4]) + (2.0f + dirF)*(srcpY[x + 3]) + 0.5f);
                    temp = (int)((-dirF - 1.5f)*(srcppY[x - 4] + srcpY[x + 4]) + (2.0f + dirF)*(srcppY[x - 3] + srcpY[x + 3]) + 0.5f);
                  }
                }
                else
                {
                  temp1 = (int)((-dirF - 1.0f)*(srcppY[x - 3]) + (1.5f + dirF)*(srcppY[x - 2]) + 0.5f);
                  temp2 = (int)((-dirF - 1.0f)*(srcpY[x + 3]) + (1.5f + dirF)*(srcpY[x + 2]) + 0.5f);
                  temp = (int)((-dirF - 1.0f)*(srcppY[x - 3] + srcpY[x + 3]) + (1.5f + dirF)*(srcppY[x - 2] + srcpY[x + 2]) + 0.5f);
                }
              }
              else
              {
                temp1 = (int)((-dirF - 0.5f)*(srcppY[x - 2]) + (1.0f + dirF)*(srcppY[x - 1]) + 0.5f);
                temp2 = (int)((-dirF - 0.5f)*(srcpY[x + 2]) + (1.0f + dirF)*(srcpY[x + 1]) + 0.5f);
                temp = (int)((-dirF - 0.5f)*(srcppY[x - 2] + srcpY[x + 2]) + (1.0f + dirF)*(srcppY[x - 1] + srcpY[x + 1]) + 0.5f);
              }
            }
            else
            {
              temp1 = (int)((-dirF)*(srcppY[x - 1]) + (0.5f + dirF)*(srcppY[x]) + 0.5f);
              temp2 = (int)((-dirF)*(srcpY[x + 1]) + (0.5f + dirF)*(srcpY[x]) + 0.5f);
              temp = (int)((-dirF)*(srcppY[x - 1] + srcpY[x + 1]) + (0.5f + dirF)*(srcppY[x] + srcpY[x]) + 0.5f);
            }
          }

          constexpr int Const20 = 20 << bitshift_to_8;
          constexpr int Const25 = 25 << bitshift_to_8;
          constexpr int Const60 = 60 << bitshift_to_8;

          minN = std::min(srcppY[x], srcpY[x]) - Const25;
          maxN = std::max(srcppY[x], srcpY[x]) + Const25;
          if (abs(temp1 - temp2) > Const20 || abs(srcppY[x] + srcpY[x] - temp - temp) > Const60 || temp < minN || temp > maxN)
          {
            temp = cubicInt<bits_per_pixel>(srcpppY[x], srcppY[x], srcpY[x], srcpnY[x]);
          }
          else {
            // clamp to valid. cubicint clamps O.K.
            constexpr int max_pixel_value = (1 << bits_per_pixel) - 1;
            if (temp > max_pixel_value) temp = max_pixel_value;
            else if (temp < 0) temp = 0;
          }
          dstpY[x] = temp;
        }
        else
        {
          if (y<3 || y>HeightY - 4) dstpY[x] = ((srcpY[x] + srcppY[x] + 1) >> 1);
          else dstpY[x] = cubicInt<bits_per_pixel>(srcpppY[x], srcppY[x], srcpY[x], srcpnY[x]);
        }
      }
    }
    srcpppY = srcppY;
    srcppY = srcpY;
    srcpY = srcpnY;
    srcpnY += src_pitchY;
    maskpY += mask_pitchY;
    dstpY += dst_pitchY;
  }
  for (y = 2 - field; y < HeightUV - 1; y += 2)
  {
    for (x = startxuv; x < stopxuv; ++x)
    {
      if (nomask || maskpV[x] == 0xFF)
      {
        if (y<3 || y>HeightUV - 4) dstpV[x] = ((srcpV[x] + srcppV[x] + 1) >> 1);
        else dstpV[x] = cubicInt<bits_per_pixel>(srcpppV[x], srcppV[x], srcpV[x], srcpnV[x]);
      }
      if (nomask || maskpU[x] == 0xFF)
      {
        if (y<3 || y>HeightUV - 4) dstpU[x] = ((srcpU[x] + srcppU[x] + 1) >> 1);
        else dstpU[x] = cubicInt<bits_per_pixel>(srcpppU[x], srcppU[x], srcpU[x], srcpnU[x]);
      }
    }
    srcpppV = srcppV;
    srcppV = srcpV;
    srcpV = srcpnV;
    srcpnV += src_pitchUV;
    srcpppU = srcppU;
    srcppU = srcpU;
    srcpU = srcpnU;
    srcpnU += src_pitchUV;
    maskpV += mask_pitchUV;
    maskpU += mask_pitchUV;
    dstpV += dst_pitchUV;
    dstpU += dst_pitchUV;
  }
}

void TFMPP::elaDeintYUY2(PVideoFrame &dst, PlanarFrame *mask, PVideoFrame &src, bool nomask, int field) const
{
  const uint8_t *srcp = src->GetReadPtr();
  int src_pitch = src->GetPitch();
  int Width = src->GetRowSize();
  int Height = src->GetHeight();
  uint8_t *dstp = dst->GetWritePtr();
  int dst_pitch = dst->GetPitch();
  const uint8_t *maskp = mask->GetPtr();
  int mask_pitch = mask->GetPitch();
  srcp += src_pitch*(3 - field);
  dstp += dst_pitch*(2 - field);
  maskp += mask_pitch*(2 - field);
  src_pitch <<= 1;
  dst_pitch <<= 1;
  mask_pitch <<= 1;
  const uint8_t *srcpp = srcp - src_pitch;
  const uint8_t *srcppp = srcpp - src_pitch;
  const uint8_t *srcpn = srcp + src_pitch;
  int stopx = Width;
  int Iy1, Iy2, Iye, Ix1, Ix2, edgeS1, edgeS2, sum, sumsq, temp, temp1, temp2, minN, maxN, x, y;
  double dir1, dir2, dir, dirF;
  for (y = 2 - field; y < Height - 1; y += 2)
  {
    for (x = 0; x < stopx; ++x)
    {
      if (nomask || maskp[x] == 0xFF)
      {
        if (y > 2 && y < Height - 3 && x>7 && x < Width - 9)
        {
          Iy1 = -srcp[x - 2] - srcp[x] - srcp[x] - srcp[x + 2] + srcppp[x - 2] + srcppp[x] + srcppp[x] + srcppp[x + 2];
          Iy2 = -srcpn[x - 2] - srcpn[x] - srcpn[x] - srcpn[x + 2] + srcpp[x - 2] + srcpp[x] + srcpp[x] + srcpp[x + 2];
          Ix1 = srcppp[x + 2] + srcpp[x + 2] + srcpp[x + 2] + srcp[x + 2] - srcppp[x - 2] - srcpp[x - 2] - srcpp[x - 2] - srcp[x - 2];
          Ix2 = srcpp[x + 2] + srcp[x + 2] + srcp[x + 2] + srcpn[x + 2] - srcpp[x - 2] - srcp[x - 2] - srcp[x - 2] - srcpn[x - 2];
          edgeS1 = Ix1*Ix1 + Iy1*Iy1;
          edgeS2 = Ix2*Ix2 + Iy2*Iy2;
          if (edgeS1 < 1600 && edgeS2 < 1600)
          {
            dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
            goto chromajump;
          }
          if (abs(srcpp[x] - srcp[x]) < 10 && (edgeS1 < 1600 || edgeS2 < 1600))
          {
            dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
            goto chromajump;
          }
          sum = srcpp[x - 2] + srcpp[x] + srcpp[x + 2] + srcp[x - 2] + srcp[x] + srcp[x + 2];
          sumsq = srcpp[x - 2] * srcpp[x - 2] + srcpp[x] * srcpp[x] + srcpp[x + 2] * srcpp[x + 2] +
            srcp[x - 2] * srcp[x - 2] + srcp[x] * srcp[x] + srcp[x + 2] * srcp[x + 2];
          if ((6 * sumsq - sum*sum) < 432)
          {
            dstp[x] = (srcpp[x] + srcp[x] + 1) >> 1;
            goto chromajump;
          }
          if (Ix1 == 0) dir1 = 3.1415926;
          else
          {
            dir1 = atan(Iy1 / (Ix1*2.0f)) + 1.5707963;
            if (Iy1 >= 0) { if (Ix1 < 0) dir1 += 3.1415927; }
            else { if (Ix1 >= 0) dir1 += 3.1415927; }
            if (dir1 >= 3.1415927) dir1 -= 3.1415927;
          }
          if (Ix2 == 0) dir2 = 3.1415926;
          else
          {
            dir2 = atan(Iy2 / (Ix2*2.0f)) + 1.5707963;
            if (Iy2 >= 0) { if (Ix2 < 0) dir2 += 3.1415927; }
            else { if (Ix2 >= 0) dir2 += 3.1415927; }
            if (dir2 >= 3.1415927) dir2 -= 3.1415927;
          }
          if (fabs(dir1 - dir2) < 0.5f)
          {
            if (edgeS1 >= 3600 && edgeS2 >= 3600) dir = (dir1 + dir2) * 0.5f;
            else dir = edgeS1 >= edgeS2 ? dir1 : dir2;
          }
          else
          {
            if (edgeS1 >= 5000 && edgeS2 >= 5000)
            {
              Iye = -srcp[x - 2] - srcp[x] - srcp[x] - srcp[x + 2] + srcpp[x - 2] + srcpp[x] + srcpp[x] + srcpp[x + 2];
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
          dirF = 0.5f / tan(dir);
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
                      temp2 = srcp[x - 8];
                      temp = (srcpp[x + 8] + srcp[x - 8] + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcp[x];
                      temp = cubicInt<8>(srcppp[x], srcpp[x], srcp[x], srcpn[x]);
                    }
                  }
                  else
                  {
                    temp1 = (int)((dirF - 1.5f)*(srcpp[x + 8]) + (2.0f - dirF)*(srcpp[x + 6]) + 0.5f);
                    temp2 = (int)((dirF - 1.5f)*(srcp[x - 8]) + (2.0f - dirF)*(srcp[x - 6]) + 0.5f);
                    temp = (int)((dirF - 1.5f)*(srcpp[x + 8] + srcp[x - 8]) + (2.0f - dirF)*(srcpp[x + 6] + srcp[x - 6]) + 0.5f);
                  }
                }
                else
                {
                  temp1 = (int)((dirF - 1.0f)*(srcpp[x + 6]) + (1.5f - dirF)*(srcpp[x + 4]) + 0.5f);
                  temp2 = (int)((dirF - 1.0f)*(srcp[x - 6]) + (1.5f - dirF)*(srcp[x - 4]) + 0.5f);
                  temp = (int)((dirF - 1.0f)*(srcpp[x + 6] + srcp[x - 6]) + (1.5f - dirF)*(srcpp[x + 4] + srcp[x - 4]) + 0.5f);
                }
              }
              else
              {
                temp1 = (int)((dirF - 0.5f)*(srcpp[x + 4]) + (1.0f - dirF)*(srcpp[x + 2]) + 0.5f);
                temp2 = (int)((dirF - 0.5f)*(srcp[x - 4]) + (1.0f - dirF)*(srcp[x - 2]) + 0.5f);
                temp = (int)((dirF - 0.5f)*(srcpp[x + 4] + srcp[x - 4]) + (1.0f - dirF)*(srcpp[x + 2] + srcp[x - 2]) + 0.5f);
              }
            }
            else
            {
              temp1 = (int)(dirF*(srcpp[x + 2]) + (0.5f - dirF)*(srcpp[x]) + 0.5f);
              temp2 = (int)(dirF*(srcp[x - 2]) + (0.5f - dirF)*(srcp[x]) + 0.5f);
              temp = (int)(dirF*(srcpp[x + 2] + srcp[x - 2]) + (0.5f - dirF)*(srcpp[x] + srcp[x]) + 0.5f);
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
                      temp2 = srcp[x + 8];
                      temp = (srcpp[x - 8] + srcp[x + 8] + 1) >> 1;
                    }
                    else
                    {
                      temp1 = temp2 = srcp[x];
                      temp = cubicInt<8>(srcppp[x], srcpp[x], srcp[x], srcpn[x]);
                    }
                  }
                  else
                  {
                    temp1 = (int)((-dirF - 1.5f)*(srcpp[x - 8]) + (2.0f + dirF)*(srcpp[x - 6]) + 0.5f);
                    temp2 = (int)((-dirF - 1.5f)*(srcp[x + 8]) + (2.0f + dirF)*(srcp[x + 6]) + 0.5f);
                    temp = (int)((-dirF - 1.5f)*(srcpp[x - 8] + srcp[x + 8]) + (2.0f + dirF)*(srcpp[x - 6] + srcp[x + 6]) + 0.5f);
                  }
                }
                else
                {
                  temp1 = (int)((-dirF - 1.0f)*(srcpp[x - 6]) + (1.5f + dirF)*(srcpp[x - 4]) + 0.5f);
                  temp2 = (int)((-dirF - 1.0f)*(srcp[x + 6]) + (1.5f + dirF)*(srcp[x + 4]) + 0.5f);
                  temp = (int)((-dirF - 1.0f)*(srcpp[x - 6] + srcp[x + 6]) + (1.5f + dirF)*(srcpp[x - 4] + srcp[x + 4]) + 0.5f);
                }
              }
              else
              {
                temp1 = (int)((-dirF - 0.5f)*(srcpp[x - 4]) + (1.0f + dirF)*(srcpp[x - 2]) + 0.5f);
                temp2 = (int)((-dirF - 0.5f)*(srcp[x + 4]) + (1.0f + dirF)*(srcp[x + 2]) + 0.5f);
                temp = (int)((-dirF - 0.5f)*(srcpp[x - 4] + srcp[x + 4]) + (1.0f + dirF)*(srcpp[x - 2] + srcp[x + 2]) + 0.5f);
              }
            }
            else
            {
              temp1 = (int)((-dirF)*(srcpp[x - 2]) + (0.5f + dirF)*(srcpp[x]) + 0.5f);
              temp2 = (int)((-dirF)*(srcp[x + 2]) + (0.5f + dirF)*(srcp[x]) + 0.5f);
              temp = (int)((-dirF)*(srcpp[x - 2] + srcp[x + 2]) + (0.5f + dirF)*(srcpp[x] + srcp[x]) + 0.5f);
            }
          }
          minN = std::min(srcpp[x], srcp[x]) - 25;
          maxN = std::max(srcpp[x], srcp[x]) + 25;
          if (abs(temp1 - temp2) > 20 || abs(srcpp[x] + srcp[x] - temp - temp) > 60 || temp < minN || temp > maxN)
          {
            temp = cubicInt<8>(srcppp[x], srcpp[x], srcp[x], srcpn[x]);
          }
          if (temp > 255) temp = 255;
          else if (temp < 0) temp = 0;
          dstp[x] = temp;
        }
        else
        {
          if (y<3 || y>Height - 4) dstp[x] = ((srcp[x] + srcpp[x] + 1) >> 1);
          else dstp[x] = cubicInt<8>(srcppp[x], srcpp[x], srcp[x], srcpn[x]);
        }
      }
    chromajump:
      ++x;
      if (nomask || maskp[x] == 0xFF)
      {
        if (y<3 || y>Height - 4) dstp[x] = ((srcp[x] + srcpp[x] + 1) >> 1);
        else dstp[x] = cubicInt<8>(srcppp[x], srcpp[x], srcp[x], srcpn[x]);
      }
    }
    srcppp = srcpp;
    srcpp = srcp;
    srcp = srcpn;
    srcpn += src_pitch;
    maskp += mask_pitch;
    dstp += dst_pitch;
  }
}

// hbd ready
void TFMPP::maskClip2(PVideoFrame &src, PVideoFrame &deint, PlanarFrame *mask,
  PVideoFrame &dst, const VideoInfo& vi) const
{
  const bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;
  const bool use_sse4 = (cpuFlags & CPUF_SSE4_1) ? true : false;

  const uint8_t *srcp, *maskp, *dntp;
  uint8_t *dstp;
  int src_pitch, msk_pitch, dst_pitch, dnt_pitch;

  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int pixelsize = vi.ComponentSize();

  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    srcp = src->GetReadPtr(plane);
    const int rowsize = src->GetRowSize(plane); // YUY2: vi.width is not GetRowSize
    const int width = rowsize / pixelsize;
    const int height = src->GetHeight(plane);
    src_pitch = src->GetPitch(plane);

    maskp = mask->GetPtr(b);
    msk_pitch = mask->GetPitch(b);

    dntp = deint->GetReadPtr(plane);
    dnt_pitch = deint->GetPitch(plane);
    dstp = dst->GetWritePtr(plane);
    dst_pitch = dst->GetPitch(plane);

    using maskClip2_fn_t = decltype(maskClip2_SSE2);
    maskClip2_fn_t* maskClip2_fn;

    if (pixelsize == 1) {
      if (use_sse4)
        maskClip2_fn = maskClip2_SSE4<uint8_t>;
      else if (use_sse2)
        maskClip2_fn = maskClip2_SSE2;
      else
        maskClip2_fn = maskClip2_C<uint8_t>;
    }
    else if (pixelsize == 2) {
      if (use_sse4)
        maskClip2_fn = maskClip2_SSE4<uint16_t>;
      else
        maskClip2_fn = maskClip2_C<uint16_t>;
    }
    else {
      return; // n/a no float support
    }

    maskClip2_fn(srcp, dntp, maskp, dstp, src_pitch, dnt_pitch, msk_pitch, dst_pitch, width, height);
  }
}


template<typename pixel_t>
void maskClip2_C(const uint8_t* srcp, const uint8_t* dntp,
  const uint8_t* maskp, uint8_t* dstp, int src_pitch, int dnt_pitch,
  int msk_pitch, int dst_pitch, int width, int height)
{
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      if (maskp[x] == 0xFF)
        reinterpret_cast<pixel_t*>(dstp)[x] = reinterpret_cast<const pixel_t*>(dntp)[x];
      else
        reinterpret_cast<pixel_t*>(dstp)[x] = reinterpret_cast<const pixel_t*>(srcp)[x];
    }
    maskp += msk_pitch;
    srcp += src_pitch;
    dntp += dnt_pitch;
    dstp += dst_pitch;
  }
}

template<typename pixel_t>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif 
void maskClip2_SSE4(const uint8_t* srcp, const uint8_t* dntp,
  const uint8_t* maskp, uint8_t* dstp, int src_pitch, int dnt_pitch,
  int msk_pitch, int dst_pitch, int width, int height)
{
  // mask is always 8 bits 0x00 or 0xFF
  while (height--) {
    for (int x = 0; x < width; x += 16 / sizeof(pixel_t)) {
      __m128i mask;
      if constexpr(sizeof(pixel_t) == 1)
        mask = _mm_load_si128(reinterpret_cast<const __m128i*>(maskp + x));
      else
        mask = _mm_cvtepi8_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(maskp + x))); // keep FF to FFFF
      // if mask is FF (FFFF) then use dnt else use src
      auto dnt = _mm_load_si128(reinterpret_cast<const __m128i*>(dntp + x * sizeof(pixel_t)));
      auto src = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * sizeof(pixel_t)));
      auto res = _mm_blendv_epi8(src, dnt, mask); // a, b, mask: if mask then b else a
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x * sizeof(pixel_t)), res);
    }
    srcp += src_pitch;
    dntp += dnt_pitch;
    dstp += dst_pitch;
    maskp += msk_pitch;
  }
}

// 8 bit only
void maskClip2_SSE2(const uint8_t *srcp, const uint8_t *dntp,
  const uint8_t *maskp, uint8_t *dstp, int src_pitch, int dnt_pitch,
  int msk_pitch, int dst_pitch, int width, int height)
{
  // mask is always 8 bits
  __m128i onesMask = _mm_set1_epi8(-1);
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto mask = _mm_load_si128(reinterpret_cast<const __m128i *>(maskp + x));
      // if mask is FF then use dnt else use src
      auto dnt_masked = _mm_and_si128(_mm_load_si128(reinterpret_cast<const __m128i *>(dntp + x)), mask);
      auto src = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      auto src_masked = _mm_and_si128(_mm_xor_si128(mask, onesMask), src); // masked with inverse mask
      auto res = _mm_or_si128(src_masked, dnt_masked);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dntp += dnt_pitch;
    dstp += dst_pitch;
    maskp += msk_pitch;
  }
}


TFMPP::TFMPP(PClip _child, int _PP, int _mthresh, const char* _ovr, bool _display,
  PClip _clip2, bool _usehints, int _opt, IScriptEnvironment* env) : GenericVideoFilter(_child),
  PP(_PP), mthresh(_mthresh), ovr(_ovr), display(_display), clip2(_clip2),
  usehints(_usehints), opt(_opt)
{
  mmask = nullptr;
  int w, i, z, b, q, countOvrS;
  char linein[1024], *linep, *linet;
  FILE *f = nullptr;

  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  cpuFlags = env->GetCPUFlags();
  if (opt == 0) cpuFlags = 0;

  if (vi.BitsPerComponent() > 16)
    env->ThrowError("TFMPP:  only 8-16 bit formats supported!");
  if (!vi.IsYUV())
    env->ThrowError("TFMPP:  YUV data only!");
  if (vi.height & 1 || vi.width & 1)
    env->ThrowError("TFMPP:  height and width must be divisible by 2!");
  if (PP < 2 || PP > 7)
    env->ThrowError("TFMPP:  PP must be set to 2, 3, 4, 5, 6, or 7!");
  if (opt < 0 || opt > 4)
    env->ThrowError("TFMPP:  opt must be set to 0, 1, 2, 3, or 4!");
  if (clip2)
  {
    uC2 = true;
    const VideoInfo &vi2 = clip2->GetVideoInfo();
    if (vi2.BitsPerComponent() != vi.BitsPerComponent())
      env->ThrowError("TFMPP:  clip2 bit depth do not match input clip!!");
    if (!vi2.IsYUV())
      env->ThrowError("TFMPP:  clip2 must be in YUV colorspace!");
    if (!vi.IsSameColorspace(vi2))
      env->ThrowError("TFMPP:  clip2 colorspace must be the same as input clip!");
    if (vi2.height != vi.height || vi2.width != vi.width)
      env->ThrowError("TFMPP:  clip2 frame dimensions do not match input clip!");
    if (vi2.num_frames != vi.num_frames)
      env->ThrowError("TFMPP:  clip2 does not have the same number of frames as input clip!");
    clip2->SetCacheHints(CACHE_NOTHING, 0);
  }
  else 
    uC2 = false;

  child->SetCacheHints(CACHE_GENERIC, 3); // fixed to diameter (07/30/2005)

  nfrms = vi.num_frames - 1;
  PP_origSaved = PP;
  mthresh_origSaved = mthresh;

  i = 0;
  if (ovr.size())
  {
    if ((f = tivtc_fopen(ovr.c_str(), "r")) != nullptr)
    {
      countOvrS = 0;
      while (fgets(linein, 1024, f) != nullptr)
      {
        if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' || linein[0] == ';' || linein[0] == '#')
          continue;
        linep = linein;
        while (*linep != 'M' && *linep != 'P' && *linep != 0) linep++;
        if (*linep != 0) ++countOvrS;
      }
      fclose(f);
      f = nullptr;
      if (countOvrS == 0) { goto emptyovrFM; }
      ++countOvrS;
      countOvrS *= 4;
      setArray.resize(countOvrS, 0xffffffff);

      if ((f = tivtc_fopen(ovr.c_str(), "r")) != nullptr)
      {
        while (fgets(linein, 1024, f) != nullptr)
        {
          if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' || linein[0] == ';' || linein[0] == '#')
            continue;
          linep = linein;
          while (*linep != 0 && *linep != ' ' && *linep != ',') linep++;
          if (*linep == ' ')
          {
            linet = linein;
            while (*linet != 0)
            {
              if (*linet != ' ' && *linet != 10) break;
              linet++;
            }
            if (*linet == 0) { continue; }
            linep++;
            if (*linep == 'M' || *linep == 'P')
            {
              sscanf(linein, "%d", &z);
              if (z<0 || z>nfrms)
              {
                fclose(f);
                env->ThrowError("TFMPP:  ovr input error (out of range frame #)!");
              }
              linep = linein;
              while (*linep != ' ' && *linep != 0) linep++;
              if (*linep != 0)
              {
                linep++;
                if (*linep == 'P' || *linep == 'M')
                {
                  q = *linep;
                  linep++;
                  linep++;
                  if (*linep == 0) continue;
                  sscanf(linep, "%d", &b);
                  if (q == 80 && (b < 0 || b > 7))
                  {
                    fclose(f);
                    env->ThrowError("TFMPP:  ovr input error (bad PP value)!");
                  }
                  else if (q != 80 && q != 77) continue;
                  setArray[i] = q; ++i;
                  setArray[i] = z; ++i;
                  setArray[i] = z; ++i;
                  setArray[i] = b; ++i;
                }
              }
            }
          }
          else if (*linep == ',')
          {
            while (*linep != ' ' && *linep != 0) linep++;
            if (*linep == 0) continue;
            linep++;
            if (*linep == 'P' || *linep == 'M')
            {
              sscanf(linein, "%d,%d", &z, &w);
              if (w == 0) w = nfrms;
              if (z<0 || z>nfrms || w<0 || w>nfrms || w < z)
              {
                fclose(f);
                env->ThrowError("TFMPP: ovr input error (invalid frame range)!");
              }
              linep = linein;
              while (*linep != ' ' && *linep != 0) linep++;
              if (*linep != 0)
              {
                linep++;
                if (*linep == 'M' || *linep == 'P')
                {
                  q = *linep;
                  linep++;
                  linep++;
                  if (*linep == 0) continue;
                  sscanf(linep, "%d", &b);
                  if (q == 80 && (b < 0 || b > 7))
                  {
                    fclose(f);
                    env->ThrowError("TFMPP:  ovr input error (bad PP value)!");
                  }
                  else if (q != 77 && q != 80) continue;
                  setArray[i] = q; ++i;
                  setArray[i] = z; ++i;
                  setArray[i] = w; ++i;
                  setArray[i] = b; ++i;
                }
              }
            }
          }
        }
        fclose(f);
        f = nullptr;
      }
      else env->ThrowError("TFMPP:  ovr file error (could not open file)!");
    }
    else env->ThrowError("TFMPP:  ovr input error (could not open ovr file)!");
  }
emptyovrFM:
  if (f != nullptr) fclose(f);
  mmask = new PlanarFrame(vi, true, cpuFlags); // true: packed yuy2
}

TFMPP::~TFMPP()
{
  if (mmask) delete mmask;
}