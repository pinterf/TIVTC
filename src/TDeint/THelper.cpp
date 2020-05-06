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

#include "THelper.h"
#include "emmintrin.h"

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
#ifdef AVISYNTH_2_5
  child->SetCacheHints(CACHE_RANGE, 3);
#else
  child->SetCacheHints(CACHE_GENERIC, 3);
#endif
  if (_lim < 0.0) lim = ULONG_MAX;
  else
  {
    double dlim = _lim*vi.height*vi.width*219.0 / 100.0;
    lim = min(max(0, int(dlim)), ULONG_MAX);
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
  else
    tdptr->subtractFields(prv, src, nxt, vi, norm1, norm2, mtn1,
      mtn2, field, order, opt, true, slow, env);
  if (debug)
  {
    sprintf(buf, "TDeint2:  frame %d:  n1 = %u  n2 = %u  m1 = %u  m2 = %u\n",
      n >> 1, norm1, norm2, mtn1, mtn2);
    OutputDebugString(buf);
  }
  if (lim != ULONG_MAX)
  {
    unsigned long d1 = subtractFrames(prv, src, env);
    unsigned long d2 = subtractFrames(src, nxt, env);
    if (debug)
    {
      sprintf(buf, "TDeint2:  frame %d:  d1 = %u  d2 = %u  lim = %u\n", n >> 1, (int)d1, (int)d2, (int)lim);
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
  if (ret == 0) blendFrames(prv, src, dst, env);
  else if (ret == 1) blendFrames(src, nxt, dst, env);
  else env->ThrowError("TDeint:  mode 2 internal error!");
  if (debug)
  {
    sprintf(buf, "TDeint2:  frame %d:  blending with %s\n", n >> 1, ret ? "nxt" : "prv");
    OutputDebugString(buf);
  }
  return dst;
}

unsigned long TDHelper::subtractFrames(PVideoFrame &src1, PVideoFrame &src2, IScriptEnvironment *env)
{
  unsigned long diff = 0;
  const unsigned char *srcp1 = src1->GetReadPtr();
  const int src1_pitch = src1->GetPitch();
  const int height = src1->GetHeight();
  const int width = (src1->GetRowSize() >> 4) << 4; // mod 16
  const unsigned char *srcp2 = src2->GetReadPtr();
  const int src2_pitch = src2->GetPitch();
  const int inc = vi.IsPlanar() ? 1 : 2;

  long cpu = env->GetCPUFlags();
  if (opt == 0) cpu = 0;
  bool use_sse2 = (cpu & CPUF_SSE2) ? true : false;

  if (use_sse2)
    subtractFrames_SSE2(srcp1, src1_pitch, srcp2, src2_pitch, height, width, inc, diff);
  else
  {
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; x += inc)
        diff += abs(srcp1[x] - srcp2[x]);
      srcp1 += src1_pitch;
      srcp2 += src2_pitch;
    }
  }
  return diff;
}

void TDHelper::blendFrames(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst, IScriptEnvironment *env)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int stop = vi.IsYUY2() ? 1 : 3;

  long cpu = env->GetCPUFlags();
  if (opt == 0) cpu = 0;
  bool use_sse2 = (cpu & CPUF_SSE2) ? true : false;

  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const unsigned char *srcp1 = src1->GetReadPtr(plane);
    const int src1_pitch = src1->GetPitch(plane);
    const int height = src1->GetHeight(plane);
    const int width = src1->GetRowSize(plane);
    const unsigned char *srcp2 = src2->GetReadPtr(plane);
    const int src2_pitch = src2->GetPitch(plane);
    unsigned char *dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    if (use_sse2)
      blendFrames_SSE2(srcp1, src1_pitch, srcp2, src2_pitch, dstp, dst_pitch, height, width);
    else
    {
      for (int y = 0; y < height; ++y)
      {
        for (int x = 0; x < width; ++x)
          dstp[x] = (srcp1[x] + srcp2[x] + 1) >> 1;
        srcp1 += src1_pitch;
        srcp2 += src2_pitch;
        dstp += dst_pitch;
      }
    }
  }
}

void subtractFrames_SSE2(const unsigned char *srcp1, int src1_pitch,
  const unsigned char *srcp2, int src2_pitch, int height, int width, int inc,
  unsigned long &diff)
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
    auto sum = _mm_setzero_si128();
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; x += 16)
      {
        auto src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp1 + x));
        auto src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp2 + x));
        auto diff = _mm_sad_epu8(src1, src2);
        sum = _mm_add_epi64(sum, diff);
      }
      srcp1 += src1_pitch;
      srcp2 += src2_pitch;
    }
    sum = _mm_add_epi64(sum, _mm_srli_si128(sum, 8));
    diff += _mm_cvtsi128_si32(sum);
  }
  else
  {
    // inc = 2
    static const __m128i lumaMask = _mm_set1_epi16(0x00FF);

    auto sum = _mm_setzero_si128();
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; x += 16)
      {
        auto src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp1 + x));
        src1 = _mm_and_si128(src1, lumaMask);
        auto src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp2 + x));
        src1 = _mm_and_si128(src2, lumaMask);
        auto diff = _mm_sad_epu8(src1, src2);
        sum = _mm_add_epi64(sum, diff);
      }
      srcp1 += src1_pitch;
      srcp2 += src2_pitch;
    }
    sum = _mm_add_epi64(sum, _mm_srli_si128(sum, 8));
    diff += _mm_cvtsi128_si32(sum);
  }
}

void blendFrames_SSE2(const unsigned char *srcp1, int src1_pitch,
  const unsigned char *srcp2, int src2_pitch, unsigned char *dstp, int dst_pitch,
  int height, int width)
{
/*
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
      dstp[x] = (srcp1[x] + srcp2[x] + 1) >> 1;
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
    dstp += dst_pitch;
  }
*/
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; x += 16)
    {
      auto result = _mm_avg_epu8(
        _mm_load_si128(reinterpret_cast<const __m128i*>(srcp1 + x)),
        _mm_load_si128(reinterpret_cast<const __m128i*>(srcp2 + x)));
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), result);
    }
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
    dstp += dst_pitch;
  }
}

