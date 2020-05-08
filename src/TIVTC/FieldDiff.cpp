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
#include "FieldDiff.h"
#include <emmintrin.h>

FieldDiff::FieldDiff(PClip _child, int _nt, bool _chroma, bool _display, bool _debug,
  bool _sse, int _opt, IScriptEnvironment *env) : GenericVideoFilter(_child),
  nt(_nt), opt(_opt),
  chroma(_chroma), debug(_debug), display(_display), sse(_sse)
{
  if (!vi.IsYUV())
    env->ThrowError("FieldDiff:  only YUV input supported!");
  if (vi.height & 1)
    env->ThrowError("FieldDiff:  height must be mod 2!");
  if (vi.height < 8)
    env->ThrowError("FieldDiff:  height must be at least 8!");
  if (opt < 0 || opt > 4)
    env->ThrowError("FieldDiff:  opt must be set to 0, 1, 2, 3, or 4!");
  nfrms = vi.num_frames - 1;
  if (debug)
  {
    sprintf(buf, "FieldDiff:  %s by tritical\n", VERSION);
    OutputDebugString(buf);
  }
  child->SetCacheHints(CACHE_NOTHING, 0);
}

FieldDiff::~FieldDiff()
{
  /* nothing to free */
}

AVSValue FieldDiff::ConditionalFieldDiff(int n, IScriptEnvironment* env)
{
  if (n < 0) n = 0;
  else if (n > nfrms) n = nfrms;
  int64_t diff = 0;
  PVideoFrame src = child->GetFrame(n, env);
  const int np = vi.IsPlanar() ? 3 : 1;
  
  if (sse) 
    diff = getDiff_SSE(src, np, chroma, nt, opt, env);
  else 
    diff = getDiff_SAD(src, np, chroma, nt, opt, env);
  
  if (debug)
  {
    if (sse) sprintf(buf, "FieldDiff:  Frame = %d  Diff = %I64d (sse)\n", n, diff);
    else sprintf(buf, "FieldDiff:  Frame = %d  Diff = %I64d (sad)\n", n, diff);
    OutputDebugString(buf);
  }
  return double(diff); 
  // fixme (cannot fix though for big frame sizes)
  // the value could be outside of int range and AVSValue doesn't
  // support int64_t... so convert it to float
}

PVideoFrame __stdcall FieldDiff::GetFrame(int n, IScriptEnvironment *env)
{
  if (n < 0) n = 0;
  else if (n > nfrms) n = nfrms;
  PVideoFrame src = child->GetFrame(n, env);
  int64_t diff = 0;
  const int np = vi.IsPlanar() ? 3 : 1;
  if (sse) 
    diff = getDiff_SSE(src, np, chroma, nt, opt, env);
  else 
    diff = getDiff_SAD(src, np, chroma, nt, opt, env);
  if (debug)
  {
    if (sse) sprintf(buf, "FieldDiff:  Frame = %d  Diff = %I64d (sse)\n", n, diff);
    else sprintf(buf, "FieldDiff:  Frame = %d  Diff = %I64d (sad)\n", n, diff);
    OutputDebugString(buf);
  }
  if (display)
  {
    env->MakeWritable(&src);
    sprintf(buf, "FieldDiff %s by tritical", VERSION);
    // fixme: use display drawing which supports any videoformat like in mvtools2
    if (vi.IsPlanar()) TFM::DrawYV12(src, 0, 0, buf);
    else TFM::DrawYUY2(src, 0, 0, buf);
    if (sse) sprintf(buf, "Frame = %d  Diff = %I64d (sse)", n, diff);
    else sprintf(buf, "Frame = %d  Diff = %I64d (sad)", n, diff);
    if (vi.IsPlanar()) TFM::DrawYV12(src, 0, 1, buf);
    else TFM::DrawYUY2(src, 0, 1, buf);
    return src;
  }
  return src;
}

int64_t FieldDiff::getDiff_SAD(PVideoFrame &src, int np, bool chromaIn, int ntIn, int opti,
  IScriptEnvironment *env)
{
  int x, y;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = chromaIn ? np : 1; // YUY2 or Planar Luma-only is 1 plane
  const int inc = (np == 1 && !chromaIn) ? 2 : 1; // YUY2 lumaonly is 2, planar and YUY2 LumaChroma is 1
  const unsigned char *srcp, *srcpp, *src2p, *srcpn, *src2n;
  int src_pitch, width, widtha, widtha1, widtha2, height, temp;

  int64_t diff = 0;
  if (ntIn > 255) ntIn = 255;
  else if (ntIn < 0) ntIn = 0;
  const int nt6 = ntIn * 6;
  
  long cpu = env->GetCPUFlags();
  // if (opt == 0) cpu = 0; // no opt parameter for this conditional function

  __m128i nt6_si128 = _mm_set1_epi16(nt6);

  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    srcp = src->GetReadPtr(plane);
    src_pitch = src->GetPitch(plane);
    width = src->GetRowSize(plane);
    widtha1 = (width >> 3) << 3; // mod8
    widtha2 = (width >> 4) << 4; // mod16
    height = src->GetHeight(plane);
    // top line
    src2p = srcp - src_pitch * 2;
    srcpp = srcp - src_pitch;
    srcpn = srcp + src_pitch;
    src2n = srcp + src_pitch * 2;
    for (x = 0; x < width; x += inc) // YUY2 luma only steps by 2
    {
      temp = abs((src2n[x] + (srcp[x] << 2) + src2n[x]) - 3 * (srcpn[x] + srcpn[x]));
      if (temp > nt6) diff += temp;
    }
    src2p += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    src2n += src_pitch;
    // 2nd line
    for (x = 0; x < width; x += inc) // YUY2 luma only steps by 2
    {
      temp = abs((src2n[x] + (srcp[x] << 2) + src2n[x]) - 3 * (srcpp[x] + srcpn[x]));
      if (temp > nt6) diff += temp;
    }
    src2p += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    src2n += src_pitch;
    // middle lines, top 2 and bottom 2 is handled separately
    if (cpu&CPUF_SSE2)
    {
      if (widtha2 >= 16) // aligned and min width
      {
        if (inc == 1) // Planar or YUY2_Luma_chroma
          calcFieldDiff_SAD_SSE2_16(src2p, src_pitch, widtha2, height - 4, nt6_si128, diff);
        else
          calcFieldDiff_SAD_SSE2_YUY2_LumaOnly_16(src2p, src_pitch, widtha2, height - 4, nt6_si128, diff);
        widtha = widtha2;
      }
      else { // no minimum 16 width
        if (inc == 1)
          calcFieldDiff_SAD_SSE2_8(src2p, src_pitch, widtha1, height - 4, nt6_si128, diff);
        else
          calcFieldDiff_SAD_SSE2_YUY2_LumaOnly_8(src2p, src_pitch, widtha1, height - 4, nt6_si128, diff);
        widtha = widtha1;
      }
      // rest on the right side
      for (y = 2; y < height - 2; ++y)
      {
        for (x = widtha; x < width; x += inc) // YUY2 luma only is step by 2
        {
          temp = abs((src2p[x] + (srcp[x] << 2) + src2n[x]) - 3 * (srcpp[x] + srcpn[x]));
          if (temp > nt6) diff += temp;
        }
        src2p += src_pitch;
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        src2n += src_pitch;
      }
    }
    else
    {
      // full C
      for (y = 2; y < height - 2; ++y)
      {
        for (x = 0; x < width; x += inc)
        {
          temp = abs((src2p[x] + (srcp[x] << 2) + src2n[x]) - 3 * (srcpp[x] + srcpn[x]));
          if (temp > nt6) diff += temp;
        }
        src2p += src_pitch;
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        src2n += src_pitch;
      }
    }
    // bottom 2 lines
    for (x = 0; x < width; x += inc)
    {
      temp = abs((src2p[x] + (srcp[x] << 2) + src2p[x]) - 3 * (srcpp[x] + srcpn[x]));
      if (temp > nt6) diff += temp;
    }
    src2p += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    src2n += src_pitch;
    // bottom line
    for (x = 0; x < width; x += inc)
    {
      temp = abs((src2p[x] + (srcp[x] << 2) + src2p[x]) - 3 * (srcpp[x] + srcpp[x]));
      if (temp > nt6) diff += temp;
    }
  }
  return (diff / 6);
}

// SSE= sum of squared errors
int64_t FieldDiff::getDiff_SSE(PVideoFrame &src, int np, bool chromaIn, int ntIn, int opti,
  IScriptEnvironment *env)
{
  int x, y;
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = chromaIn ? np : 1;
  const int inc = (np == 1 && !chromaIn) ? 2 : 1;
  const unsigned char *srcp, *srcpp, *src2p, *srcpn, *src2n;
  int src_pitch, width, widtha, widtha1, widtha2, height, temp;

  int64_t diff = 0;
  if (ntIn > 255) ntIn = 255;
  else if (ntIn < 0) ntIn = 0;
  const int nt6 = ntIn * 6;

  long cpu = env->GetCPUFlags();
  // if (opt == 0) cpu = 0; // no opt parameter for this conditional function

  __m128i nt6_si128 = _mm_set1_epi16(nt6);

  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    srcp = src->GetReadPtr(plane);
    src_pitch = src->GetPitch(plane);
    width = src->GetRowSize(plane);
    widtha1 = (width >> 3) << 3; // mod 8
    widtha2 = (width >> 4) << 4; // mod 16 for sse2 check
    height = src->GetHeight(plane);
    src2p = srcp - src_pitch * 2;
    srcpp = srcp - src_pitch;
    srcpn = srcp + src_pitch;
    src2n = srcp + src_pitch * 2;
    // top line
    for (x = 0; x < width; x += inc)
    {
      temp = abs((src2n[x] + (srcp[x] << 2) + src2n[x]) - 3 * (srcpn[x] + srcpn[x]));
      if (temp > nt6) diff += temp * temp;
    }
    src2p += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    src2n += src_pitch;
    // 2nd line
    for (x = 0; x < width; x += inc)
    {
      temp = abs((src2n[x] + (srcp[x] << 2) + src2n[x]) - 3 * (srcpp[x] + srcpn[x]));
      if (temp > nt6) diff += temp * temp;
    }
    src2p += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    src2n += src_pitch;
    // middle lines, top 2 and bottom 2 is handles separately
    if (cpu & CPUF_SSE2)
    {
      if (widtha2 >= 16) // aligned + minimum width
      {
        if (inc == 1)
          calcFieldDiff_SSE_SSE2_16(src2p, src_pitch, widtha2, height - 4, nt6_si128, diff);
        else
          calcFieldDiff_SSE_SSE2_Luma_16(src2p, src_pitch, widtha2, height - 4, nt6_si128, diff);
        widtha = widtha2;
      }
      else // less than 16, SSE2, 8
      {
        if (inc == 1)
          calcFieldDiff_SSE_SSE2_8(src2p, src_pitch, widtha1, height - 4, nt6_si128, diff);
        else
          calcFieldDiff_SSE_SSE2_Luma_8(src2p, src_pitch, widtha1, height - 4, nt6_si128, diff);
        widtha = widtha1;
      }
      // rest on the right: C
      for (y = 2; y < height - 2; ++y)
      {
        for (x = widtha; x < width; x += inc)
        {
          temp = abs((src2p[x] + (srcp[x] << 2) + src2n[x]) - 3 * (srcpp[x] + srcpn[x]));
          if (temp > nt6) diff += temp * temp;
        }
        src2p += src_pitch;
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        src2n += src_pitch;
      }
    }
    else
    { // pure C
      for (y = 2; y < height - 2; ++y)
      {
        for (x = 0; x < width; x += inc)
        {
          temp = abs((src2p[x] + (srcp[x] << 2) + src2n[x]) - 3 * (srcpp[x] + srcpn[x]));
          if (temp > nt6) diff += temp*temp;
        }
        src2p += src_pitch;
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        src2n += src_pitch;
      }
    }
    // bottom 2 lines
    for (x = 0; x < width; x += inc)
    {
      temp = abs((src2p[x] + (srcp[x] << 2) + src2p[x]) - 3 * (srcpp[x] + srcpn[x]));
      if (temp > nt6) diff += temp*temp;
    }
    src2p += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    src2n += src_pitch;
    // bottom line
    for (x = 0; x < width; x += inc)
    {
      temp = abs((src2p[x] + (srcp[x] << 2) + src2p[x]) - 3 * (srcpp[x] + srcpp[x]));
      if (temp > nt6) diff += temp*temp;
    }
  }
  return (diff / 6);
}

AVSValue __cdecl Create_CFieldDiff(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  AVSValue cnt = env->GetVar("current_frame");
  if (!cnt.IsInt())
    env->ThrowError("CFieldDiff:  This filter can only be used within ConditionalFilter!");
  int n = cnt.AsInt();
  FieldDiff *f = new FieldDiff(args[0].AsClip(), args[1].AsInt(3), args[2].AsBool(true),
    false, args[3].AsBool(false), args[4].AsBool(false), args[5].AsInt(4), env);
  AVSValue CFieldDiff = f->ConditionalFieldDiff(n, env);
  delete f;
  return CFieldDiff;
}

AVSValue __cdecl Create_FieldDiff(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  return new FieldDiff(args[0].AsClip(), args[1].AsInt(3), args[2].AsBool(true),
    args[3].AsBool(false), args[4].AsBool(false), args[5].AsBool(false),
    args[6].AsInt(4), env);
}


// SSE here: sum of squared errors (not the CPU)

template<bool yuy2luma_only, bool ssd_mode>
static void calcFieldDiff_SSEorSSD_SSE2_simd_8(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  __m128i zero = _mm_setzero_si128();
  __m128i lumaWordMask = _mm_set1_epi32(0x0000FFFF); // used in YUY2 luma-only mode

  const unsigned char *src2p_odd = src2p + src_pitch;
  auto diff64 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&diff));
  while (height--) {
    __m128i sum = _mm_setzero_si128();
    for (int x = 0; x < width; x += 8) {
      auto _src2p = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(src2p + x)); // xmm0
      auto _srcp = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(src2p + src_pitch * 2 + x)); // xmm1
      auto _src2n = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(src2p + src_pitch * 4 + x)); // xmm2
      auto _src2p_lo = _mm_unpacklo_epi8(_src2p, zero);
      auto _srcp_lo = _mm_unpacklo_epi8(_srcp, zero);
      auto _src2n_lo = _mm_unpacklo_epi8(_src2n, zero);
      auto sum1_lo = _mm_adds_epu16(_mm_adds_epu16(_src2p_lo, _src2n_lo), _mm_slli_epi16(_srcp_lo, 2)); // 2p + 2*p + 2n

      auto _srcpp = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(src2p_odd + x)); // xmm0
      auto _srcpn = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(src2p_odd + src_pitch * 2 + x)); // xmm1
      auto _srcpp_lo = _mm_unpacklo_epi8(_srcpp, zero);
      auto _srcpn_lo = _mm_unpacklo_epi8(_srcpn, zero);
      auto threeMask = _mm_set1_epi16(3);
      auto sum2_lo = _mm_mullo_epi16(_mm_adds_epu16(_srcpp_lo, _srcpn_lo), threeMask); // 3*(pp + pn)

      auto absdiff_lo = _mm_or_si128(_mm_subs_epu16(sum1_lo, sum2_lo), _mm_subs_epu16(sum2_lo, sum1_lo));

      auto res_lo = _mm_and_si128(absdiff_lo, _mm_cmpgt_epi16(absdiff_lo, nt)); // keep if >= nt, 0 otherwise

      if (yuy2luma_only) {
        res_lo = _mm_and_si128(res_lo, lumaWordMask);
      }

      if (ssd_mode) {
        //pmaddwd xmm0, xmm0
        //pmaddwd xmm2, xmm2
        auto res_lo2 = _mm_madd_epi16(res_lo, res_lo); // Sum Of Squares
        sum = _mm_add_epi32(sum, res_lo2); // sum in 4x32 but parts xmm6
      }
      else {
        //paddusw xmm0, xmm2
        //movdqa xmm2, xmm0
        //punpcklwd xmm0, xmm7
        //punpckhwd xmm2, xmm7
        auto res = res_lo;
        auto res_lo2 = _mm_unpacklo_epi16(res, zero);
        auto res_hi2 = _mm_unpackhi_epi16(res, zero);
        sum = _mm_add_epi32(sum, _mm_add_epi32(res_lo2, res_hi2)); // sum in 4x32 but parts xmm6
      }
    }
    // update output
    auto sum2 = _mm_add_epi64(_mm_unpacklo_epi32(sum, zero), _mm_unpackhi_epi32(sum, zero));
    diff64 = _mm_add_epi64(_mm_add_epi64(sum2, _mm_srli_si128(sum2, 8)), diff64);
    src2p_odd += src_pitch;
    src2p += src_pitch;
  }
  _mm_storel_epi64(reinterpret_cast<__m128i *>(&diff), diff64);
}


template<bool yuy2luma_only, bool ssd_mode>
static void calcFieldDiff_SSEorSSD_SSE2_simd_16(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  __m128i zero = _mm_setzero_si128();
  __m128i lumaWordMask = _mm_set1_epi32(0x0000FFFF);

  const unsigned char *src2p_odd = src2p + src_pitch;
  auto diff64 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&diff));
  while (height--) {
    __m128i sum = _mm_setzero_si128();
    for (int x = 0; x < width; x += 16) {
      auto _src2p = _mm_load_si128(reinterpret_cast<const __m128i *>(src2p + x)); // xmm0
      auto _srcp = _mm_load_si128(reinterpret_cast<const __m128i *>(src2p + src_pitch * 2 + x)); // xmm1
      auto _src2n = _mm_load_si128(reinterpret_cast<const __m128i *>(src2p + src_pitch * 4 + x)); // xmm2
      auto _src2p_lo = _mm_unpacklo_epi8(_src2p, zero);
      auto _src2p_hi = _mm_unpackhi_epi8(_src2p, zero);
      auto _srcp_lo = _mm_unpacklo_epi8(_srcp, zero);
      auto _srcp_hi = _mm_unpackhi_epi8(_srcp, zero);
      auto _src2n_lo = _mm_unpacklo_epi8(_src2n, zero);
      auto _src2n_hi = _mm_unpackhi_epi8(_src2n, zero);
      auto sum1_lo = _mm_adds_epu16(_mm_adds_epu16(_src2p_lo, _src2n_lo), _mm_slli_epi16(_srcp_lo, 2)); // 2p + 2*p + 2n
      auto sum1_hi = _mm_adds_epu16(_mm_adds_epu16(_src2p_hi, _src2n_hi), _mm_slli_epi16(_srcp_hi, 2)); // 2p + 2*p + 2n

      auto _srcpp = _mm_load_si128(reinterpret_cast<const __m128i *>(src2p_odd + x)); // xmm0
      auto _srcpn = _mm_load_si128(reinterpret_cast<const __m128i *>(src2p_odd + src_pitch * 2 + x)); // xmm1
      auto _srcpp_lo = _mm_unpacklo_epi8(_srcpp, zero);
      auto _srcpp_hi = _mm_unpackhi_epi8(_srcpp, zero);
      auto _srcpn_lo = _mm_unpacklo_epi8(_srcpn, zero);
      auto _srcpn_hi = _mm_unpackhi_epi8(_srcpn, zero);
      auto threeMask = _mm_set1_epi16(3);
      auto sum2_lo = _mm_mullo_epi16(_mm_adds_epu16(_srcpp_lo, _srcpn_lo), threeMask); // 3*(pp + pn)
      auto sum2_hi = _mm_mullo_epi16(_mm_adds_epu16(_srcpp_hi, _srcpn_hi), threeMask); //

      auto absdiff_lo = _mm_or_si128(_mm_subs_epu16(sum1_lo, sum2_lo), _mm_subs_epu16(sum2_lo, sum1_lo));
      auto absdiff_hi = _mm_or_si128(_mm_subs_epu16(sum1_hi, sum2_hi), _mm_subs_epu16(sum2_hi, sum1_hi));

      auto res_lo = _mm_and_si128(absdiff_lo, _mm_cmpgt_epi16(absdiff_lo, nt)); // keep if >= nt, 0 otherwise
      auto res_hi = _mm_and_si128(absdiff_hi, _mm_cmpgt_epi16(absdiff_hi, nt));

      if (yuy2luma_only) {
        res_lo = _mm_and_si128(res_lo, lumaWordMask);
        res_hi = _mm_and_si128(res_hi, lumaWordMask);
      }

      __m128i res_lo2, res_hi2;

      if (ssd_mode) {
        //pmaddwd xmm0, xmm0
        //pmaddwd xmm2, xmm2
        res_lo2 = _mm_madd_epi16(res_lo, res_lo);
        res_hi2 = _mm_madd_epi16(res_hi, res_hi);
      }
      else {
        //paddusw xmm0, xmm2
        //movdqa xmm2, xmm0
        //punpcklwd xmm0, xmm7
        //punpckhwd xmm2, xmm7
        auto res = _mm_adds_epu16(res_lo, res_hi);
        res_lo2 = _mm_unpacklo_epi16(res, zero);
        res_hi2 = _mm_unpackhi_epi16(res, zero);
      }
      sum = _mm_add_epi32(sum, _mm_add_epi32(res_lo2, res_hi2)); // sum in 4x32 but parts xmm6
    }
    // update output
    auto sum2 = _mm_add_epi64(_mm_unpacklo_epi32(sum, zero), _mm_unpackhi_epi32(sum, zero));
    diff64 = _mm_add_epi64(_mm_add_epi64(sum2, _mm_srli_si128(sum2, 8)), diff64);
    src2p_odd += src_pitch;
    src2p += src_pitch;
  }
  _mm_storel_epi64(reinterpret_cast<__m128i *>(&diff), diff64);

}

void FieldDiff::calcFieldDiff_SAD_SSE2_8(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  // luma and chroma, sad_mode
  calcFieldDiff_SSEorSSD_SSE2_simd_8<false, false>(src2p, src_pitch, width, height, nt, diff);
}

void FieldDiff::calcFieldDiff_SAD_SSE2_16(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  // luma and chroma, sad_mode
  calcFieldDiff_SSEorSSD_SSE2_simd_16<false, false>(src2p, src_pitch, width, height, nt, diff);
}


void FieldDiff::calcFieldDiff_SAD_SSE2_YUY2_LumaOnly_8(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  // yuy2 luma only, sad mode
  calcFieldDiff_SSEorSSD_SSE2_simd_8<true, false>(src2p, src_pitch, width, height, nt, diff);
}

void FieldDiff::calcFieldDiff_SAD_SSE2_YUY2_LumaOnly_16(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  // yuy2 luma only, sad mode
  calcFieldDiff_SSEorSSD_SSE2_simd_16<true, false>(src2p, src_pitch, width, height, nt, diff);
}


void FieldDiff::calcFieldDiff_SSE_SSE2_8(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  // w/o luma, ssd mode
  calcFieldDiff_SSEorSSD_SSE2_simd_8<false, true>(src2p, src_pitch, width, height, nt, diff);
}

void FieldDiff::calcFieldDiff_SSE_SSE2_16(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  // w/o luma, ssd mode
  calcFieldDiff_SSEorSSD_SSE2_simd_16<false, true>(src2p, src_pitch, width, height, nt, diff);
}


void FieldDiff::calcFieldDiff_SSE_SSE2_Luma_8(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  // with luma, ssd mode
  calcFieldDiff_SSEorSSD_SSE2_simd_8<true, true>(src2p, src_pitch, width, height, nt, diff);
}

void FieldDiff::calcFieldDiff_SSE_SSE2_Luma_16(const unsigned char *src2p, int src_pitch,
  int width, int height, __m128i nt, int64_t &diff)
{
  // with luma, ssd mode
  calcFieldDiff_SSEorSSD_SSE2_simd_16<true, true>(src2p, src_pitch, width, height, nt, diff);
}

