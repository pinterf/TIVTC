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

#include "THelper.h"
#include "emmintrin.h"
#include <inttypes.h>

TDHelper::~TDHelper()
{
  // nothing to free
}

TDHelper::TDHelper(PClip _child, int _order, int _field, double _lim, bool _debug,
  int _opt, std::vector<int> &_sa, int _slow, TDeinterlace * _tdptr, IScriptEnvironment *env) :
  GenericVideoFilter(_child), order(_order), field(_field), debug(_debug), opt(_opt),
  sa(_sa), slow(_slow), tdptr(_tdptr)
{
  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  cpuFlags = env->GetCPUFlags();
  if (opt == 0) cpuFlags = 0;

  if (vi.BitsPerComponent() > 16)
    env->ThrowError("TDHelper:  only 8-16 bit formats supported!");
  if (!vi.IsYUV())
    env->ThrowError("TDHelper:  YUV colorspaces only!");
  if (order != -1 && order != 0 && order != 1)
    env->ThrowError("TDHelper:  order must be set to -1, 0, or 1!");
  if (field != -1 && field != 0 && field != 1)
    env->ThrowError("TDHelper:  field must be set to -1, 0, or 1!");
  if (opt < 0 || opt > 4)
    env->ThrowError("TDHelper:  opt must be set to 0, 1, 2, 3, or 4!");
  if (slow < 0 || slow > 2)
    env->ThrowError("TDHelper:  slow must be set to 0, 1, or 2!");
  if (!tdptr)
    env->ThrowError("TDHelper:  tdptr not set!");
  if (order == -1) order = child->GetParity(0);
  if (field == -1) field = child->GetParity(0);
  nfrms = vi.num_frames;
  vi.num_frames >>= 1;
  vi.SetFPS(vi.fps_numerator, vi.fps_denominator * 2);
  child->SetCacheHints(CACHE_GENERIC, 3);

  const int bits_per_pixel = vi.BitsPerComponent();

  if (_lim < 0.0)
    lim = ULONG_MAX;
  else
  {
    // 219: 235-16 max Y difference
    double dlim = _lim * vi.height * vi.width * 219.0 * (1 << (bits_per_pixel - 8)) / 100.0;
    lim = min(max(0, int64_t(dlim)), ULONG_MAX);  // scaled!
  }
}

int TDHelper::mapn(int n)
{
  if (n == -1) n += 2;
  else if (n == nfrms) n -= 2;
  if (n < 0) return 0;
  if (n >= nfrms) return nfrms;
  return n;
}

PVideoFrame __stdcall TDHelper::GetFrame(int n, IScriptEnvironment *env)
{
  n *= 2;
  if (field != order) ++n;
  PVideoFrame prv = child->GetFrame(mapn(n - 1), env);
  PVideoFrame src = child->GetFrame(mapn(n), env);
  PVideoFrame nxt = child->GetFrame(mapn(n + 1), env);
  int norm1 = -1, norm2 = -1;
  int mtn1 = -1, mtn2 = -1;

  const int bits_per_pixel = vi.BitsPerComponent();

  if (sa.size() > 0)
  {
    for (int i = 0; i < 500; ++i)
    {
      if (sa[i * 5] == n)
      {
        norm1 = sa[i * 5 + 1];
        norm2 = sa[i * 5 + 2];
        mtn1 = sa[i * 5 + 3];
        mtn2 = sa[i * 5 + 4];
        break;
      }
    }
    if (norm1 == -1 || norm2 == -1 || mtn1 == -1 || mtn2 == -1)
      env->ThrowError("TDeint:  mode 2 internal communication problem!");
  }
  else {
    VideoInfo vi_map = vi;
    // for mask vi to 8 bits
    vi.pixel_type = (vi.pixel_type & ~VideoInfo::CS_Sample_Bits_Mask) | VideoInfo::CS_Sample_Bits_8;
    if (bits_per_pixel == 8)
      tdptr->subtractFields<uint8_t>(prv, src, nxt, vi_map, 
        norm1, norm2, mtn1, mtn2, // output is 8 bit normalized
        field, order, true, slow, env);
    else if(bits_per_pixel <= 16)
      tdptr->subtractFields<uint16_t>(prv, src, nxt, vi_map, 
        norm1, norm2, mtn1, mtn2, // output is 8 bit normalized
        field, order, true, slow, env);
  }
  if (debug)
  {
    sprintf(buf, "TDeint2:  frame %d:  n1 = %u  n2 = %u  m1 = %u  m2 = %u\n",
      n >> 1, norm1, norm2, mtn1, mtn2);
    OutputDebugString(buf);
  }
  if (lim != ULONG_MAX)
  {
    uint64_t d1, d2;
    if (bits_per_pixel == 8) {
      d1 = subtractFrames<uint8_t>(prv, src, env);
      d2 = subtractFrames<uint8_t>(src, nxt, env);
    }
    else {
      d1 = subtractFrames<uint16_t>(prv, src, env);
      d2 = subtractFrames<uint16_t>(src, nxt, env);
    }
    if (debug)
    {
      sprintf(buf, "TDeint2:  frame %d:  d1 = %" PRIu64 "   d2 = %" PRIu64 "  lim = %" PRIu64 "\n", n >> 1, d1, d2, lim);
      OutputDebugString(buf);
    }
    if (d1 > lim && d2 > lim)
    {
      if (debug)
      {
        sprintf(buf, "TDeint2:  frame %d:  not blending (returning src)\n", n >> 1);
        OutputDebugString(buf);
      }
      return src;
    }
  }
  PVideoFrame dst = 
    has_at_least_v8 ? 
    env->NewVideoFrameP(vi, &src) :
    env->NewVideoFrame(vi);

  int ret = -1;

  // When subtractFields would not leave at 8 bit scale, norm1, norm2, mtn1, mtn2 must be scaled instead
  // and probably use int64

  float c1 = float(max(norm1, norm2)) / float(max(min(norm1, norm2), 1));
  float c2 = float(max(mtn1, mtn2)) / float(max(min(mtn1, mtn2), 1));
  float mr = float(max(mtn1, mtn2)) / float(max(max(norm1, norm2), 1));
  if (((mtn1 >= 500 || mtn2 >= 500) && (mtn1 * 2 < mtn2 * 1 || mtn2 * 2 < mtn1 * 1)) ||
    ((mtn1 >= 1000 || mtn2 >= 1000) && (mtn1 * 3 < mtn2 * 2 || mtn2 * 3 < mtn1 * 2)) ||
    ((mtn1 >= 2000 || mtn2 >= 2000) && (mtn1 * 5 < mtn2 * 4 || mtn2 * 5 < mtn1 * 4)) ||
    ((mtn1 >= 4000 || mtn2 >= 4000) && c2 > c1))
  {
    if (mtn1 > mtn2) ret = 1;
    else ret = 0;
  }
  else if (mr > 0.005 && max(mtn1, mtn2) > 150 && (mtn1 * 2 < mtn2 * 1 || mtn2 * 2 < mtn1 * 1))
  {
    if (mtn1 > mtn2) ret = 1;
    else ret = 0;
  }
  else
  {
    if (norm1 > norm2) ret = 1;
    else ret = 0;
  }
  
  if (ret == 0) {
    if (bits_per_pixel == 8)
      blendFrames<uint8_t>(prv, src, dst, env);
    else
      blendFrames<uint16_t>(prv, src, dst, env);
  }
  else if (ret == 1) {
    if (bits_per_pixel == 8)
      blendFrames<uint8_t>(src, nxt, dst, env);
    else
      blendFrames<uint16_t>(src, nxt, dst, env);
  }
  else 
    env->ThrowError("TDeint:  mode 2 internal error!");

  if (debug)
  {
    sprintf(buf, "TDeint2:  frame %d:  blending with %s\n", n >> 1, ret ? "nxt" : "prv");
    OutputDebugString(buf);
  }
  return dst;
}

template<typename pixel_t>
uint64_t TDHelper::subtractFrames(PVideoFrame &src1, PVideoFrame &src2, IScriptEnvironment *env)
{
  uint64_t diff = 0;
  // y only
  const uint8_t *srcp1 = src1->GetReadPtr();
  const int src1_pitch = src1->GetPitch();
  const int height = src1->GetHeight();
  const int rowsize = (src1->GetRowSize() >> 4) << 4; // mod 16
  const int width = src1->GetRowSize() / sizeof(pixel_t);
  const uint8_t *srcp2 = src2->GetReadPtr();
  const int src2_pitch = src2->GetPitch();
  const int inc = vi.IsPlanar() ? 1 : 2; // YUY2 lumaonly: step 2

  const bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

  if (use_sse2)
    subtractFrames_SSE2<pixel_t>(srcp1, src1_pitch, srcp2, src2_pitch, height, rowsize, inc, diff);
  else
  {
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; x += inc)
        diff += abs(reinterpret_cast<const pixel_t *>(srcp1)[x] - reinterpret_cast<const pixel_t*>(srcp2)[x]);
      srcp1 += src1_pitch;
      srcp2 += src2_pitch;
    }
  }
  return diff;
}

template<typename pixel_t>
void TDHelper::blendFrames(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, IScriptEnvironment *env)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsYUY2() || vi.IsY() ? 1 : 3;

  const bool use_sse2 = (cpuFlags & CPUF_SSE2) ? true : false;

  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const uint8_t *srcp1 = src1->GetReadPtr(plane);
    const int src1_pitch = src1->GetPitch(plane);
    const int height = src1->GetHeight(plane);

    const int rowsize = (src1->GetRowSize(plane) >> 4) << 4; // mod 16
    const int width = src1->GetRowSize(plane) / sizeof(pixel_t);

    const uint8_t *srcp2 = src2->GetReadPtr(plane);
    const int src2_pitch = src2->GetPitch(plane);
    uint8_t *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    if (use_sse2)
      blendFrames_SSE2<pixel_t>(srcp1, src1_pitch, srcp2, src2_pitch, dstp, dst_pitch, height, rowsize);
    else
    {
      for (int y = 0; y < height; ++y)
      {
        for (int x = 0; x < width; ++x)
          reinterpret_cast<pixel_t*>(dstp)[x] = (reinterpret_cast<const pixel_t*>(srcp1)[x] + reinterpret_cast<const pixel_t*>(srcp2)[x] + 1) >> 1;
        srcp1 += src1_pitch;
        srcp2 += src2_pitch;
        dstp += dst_pitch;
      }
    }
  }
}

template<typename pixel_t>
void subtractFrames_SSE2(const uint8_t *srcp1, int src1_pitch,
  const uint8_t *srcp2, int src2_pitch, int height, int rowsize, int inc,
  uint64_t &diff)
{
  // inc: 2 (YUY2 lumaonly chroma skip) or 1 (YV12)
/*
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; x += inc)
      diff += abs(srcp1[x] - srcp2[x]);
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
  }
*/
  if (inc == 1)
  {
    auto zero = _mm_setzero_si128();
    auto sum = _mm_setzero_si128();
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < rowsize; x += 16)
      {
        auto src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp1 + x));
        auto src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp2 + x));
        if constexpr (sizeof(pixel_t) == 1) {
          auto absdiff = _mm_sad_epu8(src1, src2);
          sum = _mm_add_epi64(sum, absdiff);
        }
        else { // uint16_t
          auto diff12 = _mm_subs_epu16(src1, src2);
          auto diff21 = _mm_subs_epu16(src2, src1);
          auto absdiff = _mm_or_si128(diff12, diff21);
          sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
          sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero)); // 4x32 bits
        }
      }
      srcp1 += src1_pitch;
      srcp2 += src2_pitch;
    }
    if constexpr (sizeof(pixel_t) == 1) {
      sum = _mm_add_epi64(sum, _mm_srli_si128(sum, 8)); // 8 bit sad lo64 hi64
    }
    else {
      // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
      __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
      __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
      sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                         // sum here: two 32 bit partial result: sum1 0 sum2 0
      __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
      sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
    }
    diff += _mm_cvtsi128_si32(sum);
  }
  else
  {
    // inc = 2 YUY2 lumaonly
    const __m128i lumaMask = _mm_set1_epi16(0x00FF);

    auto sum = _mm_setzero_si128();
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < rowsize; x += 16)
      {
        auto src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp1 + x));
        src1 = _mm_and_si128(src1, lumaMask);
        auto src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp2 + x));
        src1 = _mm_and_si128(src2, lumaMask);
        auto sad = _mm_sad_epu8(src1, src2);
        sum = _mm_add_epi64(sum, sad);
      }
      srcp1 += src1_pitch;
      srcp2 += src2_pitch;
    }
    sum = _mm_add_epi64(sum, _mm_srli_si128(sum, 8));
    diff += _mm_cvtsi128_si32(sum);
  }
}

template<typename pixel_t>
void blendFrames_SSE2(const uint8_t *srcp1, int src1_pitch,
  const uint8_t *srcp2, int src2_pitch, uint8_t *dstp, int dst_pitch,
  int height, int rowsize)
{
  // simple average
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < rowsize; x += 16)
    {
      auto src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp1 + x));
      auto src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp2 + x));
      __m128i result;
      if constexpr (sizeof(pixel_t) == 1)
        result = _mm_avg_epu8(src1, src2);
      else 
        result = _mm_avg_epu16(src1, src2);
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), result);
    }
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
    dstp += dst_pitch;
  }
}

