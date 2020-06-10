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
#include "FieldDiff.h"
#include <emmintrin.h>
#include <smmintrin.h>
#include <inttypes.h>
#include "info.h"

FieldDiff::FieldDiff(PClip _child, int _nt, bool _chroma, bool _display, bool _debug,
  bool _sse, int _opt, IScriptEnvironment *env) : GenericVideoFilter(_child),
  nt(_nt), opt(_opt),
  chroma(_chroma), debug(_debug), display(_display), sse(_sse)
{

  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  cpuFlags = env->GetCPUFlags();
  if (opt == 0) cpuFlags = 0;

  if (vi.BitsPerComponent() > 16)
    env->ThrowError("FieldDiff:  Only 8-16 bit clip data supported!");
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

  const int bits_per_pixel = vi.BitsPerComponent();

  if (sse) {
    diff =
      bits_per_pixel == 8 ?
      getDiff_SADorSSE<uint8_t, false>(src, vi, chroma, nt, cpuFlags) :
      getDiff_SADorSSE<uint16_t, false>(src, vi, chroma, nt, cpuFlags);
  }
  else {
    diff =
      bits_per_pixel == 8 ?
      getDiff_SADorSSE<uint8_t, true>(src, vi, chroma, nt, cpuFlags) :
      getDiff_SADorSSE<uint16_t, true>(src, vi, chroma, nt, cpuFlags);
  }
  if (debug)
  {
    if (sse) sprintf(buf, "FieldDiff:  Frame = %d  Diff = %" PRId64 " (sse)\n", n, diff);
    else sprintf(buf, "FieldDiff:  Frame = %d  Diff = %" PRId64 " (sad)\n", n, diff);
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

  const int bits_per_pixel = vi.BitsPerComponent();

  if (sse) {
    diff =
      bits_per_pixel == 8 ?
      getDiff_SADorSSE<uint8_t, false>(src, vi, chroma, nt, cpuFlags) :
      getDiff_SADorSSE<uint16_t, false>(src, vi, chroma, nt, cpuFlags);
  }
  else {

    diff =
      bits_per_pixel == 8 ?
      getDiff_SADorSSE<uint8_t, true>(src, vi, chroma, nt, cpuFlags) :
      getDiff_SADorSSE<uint16_t, true>(src, vi, chroma, nt, cpuFlags);
  }

  if (debug)
  {
    if (sse) sprintf(buf, "FieldDiff:  Frame = %d  Diff = %" PRId64 " (sse)\n", n, diff);
    else sprintf(buf, "FieldDiff:  Frame = %d  Diff = %" PRId64 " (sad)\n", n, diff);
    OutputDebugString(buf);
  }
  if (display)
  {
    env->MakeWritable(&src);
    sprintf(buf, "FieldDiff %s by tritical", VERSION);
    Draw(src, 0, 0, buf, vi);
    if (sse) sprintf(buf, "Frame = %d  Diff = %" PRId64 " (sse)", n, diff);
    else sprintf(buf, "Frame = %d  Diff = %" PRId64 " (sad)", n, diff);
    Draw(src, 0, 1, buf, vi);
    return src;
  }
  return src;
}

// SSE= sum of squared errors
// SAD: true
// SSE: false
template<typename pixel_t, bool SAD>
int64_t FieldDiff::getDiff_SADorSSE(PVideoFrame &src, const VideoInfo &vi, bool chromaIn, int ntIn, int cpuFlags)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;
  const int stop = chromaIn ? np : 1;
  const int inc = (vi.IsYUY2() && !chromaIn) ? 2 : 1;
  const int bits_per_pixel = vi.BitsPerComponent();

  int widtha;

  int64_t diff = 0;

  // validity check
  if (ntIn > 255)
    ntIn = 255;
  else if (ntIn < 0)
    ntIn = 0;

  // scale original parameter to actual bit depth
  ntIn = ntIn << (bits_per_pixel - 8);

  const int nt6 = ntIn * 6;

  const bool use_sse2 = cpuFlags & CPUF_SSE2;
  const bool use_sse4 = cpuFlags & CPUF_SSE4_1;

  // Cumulative values would exceed even int64 at a worst case pixel differences and 20+MPix 4:4:4@16 bit frame
  // we are normalize results back to 8 bit case
  int64_t diff_line;
  // max per pixel: SAD: 6*(255-0) @8 bit   6*(65535-0) @16 bits
  //                SSE: square of SAD.
  //                     23 B824            23 FFB8 0024
  // Enough for                             59.6 mpixel/plane, worst case 20MPix/4:4:4@16bit frame
  // gather statistics for each line then scale back to 8 bit case
  // SSE and high bit depth: scale back to 8 bits magnitude after each collected line
  int bit_shift;
  if constexpr (SAD)
    bit_shift = (bits_per_pixel - 8); // SAD
  else
    bit_shift = 2 * (bits_per_pixel - 8); // SSE
  const int diffline_rounder = 1 << (bit_shift - 1);


  for (int b = 0; b < stop; ++b)
  {
    const int plane = planes[b];
    const pixel_t* srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
    const ptrdiff_t src_pitch = src->GetPitch(plane) / sizeof(pixel_t);
    const int width = src->GetRowSize(plane) / sizeof(pixel_t);
    const int widthMod8 = (width >> 3) << 3;
    const int widthMod16 = (width >> 4) << 4;
    const int height = src->GetHeight(plane);
    const pixel_t* srcppp = srcp - src_pitch * 2;
    const pixel_t* srcpp = srcp - src_pitch;
    const pixel_t* srcpn = srcp + src_pitch;
    const pixel_t* srcpnn = srcp + src_pitch * 2;

    // top line
    diff_line = 0;
    for (int x = 0; x < width; x += inc)
    { // not prev and prev-prev at the top
      const int temp = abs((srcpnn[x] + (srcp[x] << 2) + srcpnn[x]) - 3 * (srcpn[x] + srcpn[x]));
      if (temp > nt6) { if constexpr (SAD) diff_line += temp; else diff_line += (int64_t)temp * temp; }
    }
    diff += (diff_line + diffline_rounder) >> bit_shift;

    srcppp += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    srcpnn += src_pitch;

    // 2nd line
    diff_line = 0;
    for (int x = 0; x < width; x += inc)
    {
      // not prev at the top+1
      const int temp = abs((srcpnn[x] + (srcp[x] << 2) + srcpnn[x]) - 3 * (srcpp[x] + srcpn[x]));
      if (temp > nt6) { if constexpr (SAD) diff_line += temp; else diff_line += (int64_t)temp * temp; }
    }
    diff += (diff_line + diffline_rounder) >> bit_shift;

    srcppp += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    srcpnn += src_pitch;

    widtha = 0;
    
    // middle lines, top 2 and bottom 2 is handled separately
    if (use_sse2)
    {
      if constexpr (sizeof(pixel_t) == 1)
      {
        if (widthMod16 >= 16)
        {
          if constexpr (SAD) {
            // Planar or YUY2_Luma_chroma
            if (inc == 1) calcFieldDiff_SAD_SSE2(srcppp, src_pitch, widthMod16, height - 4, nt6, diff);
            else calcFieldDiff_SAD_SSE2_YUY2_LumaOnly(srcppp, src_pitch, widthMod16, height - 4, nt6, diff);
          }
          else {
            // SSE
            if (inc == 1) calcFieldDiff_SSE_SSE2(srcppp, src_pitch, widthMod16, height - 4, nt6, diff);
            else calcFieldDiff_SSE_SSE2_YUY2_LumaOnly(srcppp, src_pitch, widthMod16, height - 4, nt6, diff); // YUY2 luma only
          }
          widtha = widthMod16;
        }
      }
      else if constexpr (sizeof(pixel_t) == 2)
      {
        // 10-16 bits
        if (widthMod8 >= 8 && use_sse4)
        {
          if constexpr (SAD)
            calcFieldDiff_SAD_uint16_SSE4((const uint8_t*)srcppp, src_pitch * sizeof(uint16_t), widthMod8 * sizeof(uint16_t), height - 4, nt6, diff, bits_per_pixel);
          else {
            calcFieldDiff_SSE_uint16_SSE4((const uint8_t*)srcppp, src_pitch * sizeof(uint16_t), widthMod8 * sizeof(uint16_t), height - 4, nt6, diff, bits_per_pixel);
          }
          widtha = widthMod8;
        }
      }
    }
    if (widtha < width) {
      // rest from the beginning or on the right: C
      for (int y = 2; y < height - 2; ++y)
      {
        diff_line = 0;
        for (int x = widtha; x < width; x += inc)
        {
          const int temp = abs((srcppp[x] + (srcp[x] << 2) + srcpnn[x]) - 3 * (srcpp[x] + srcpn[x]));
          if (temp > nt6) { if constexpr (SAD) diff_line += temp; else diff_line += (int64_t)temp * temp; }
        }
        diff += (diff_line + diffline_rounder) >> bit_shift;
        srcppp += src_pitch;
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
        srcpnn += src_pitch;
      }
    }
    else {
      const auto lines_processed = height - 4;
      srcppp += src_pitch * lines_processed;
      srcpp += src_pitch * lines_processed;
      srcp += src_pitch * lines_processed;
      srcpn += src_pitch * lines_processed;
      srcpnn += src_pitch * lines_processed;
    }
    // bottom 2 lines
    diff_line = 0;
    for (int x = 0; x < width; x += inc)
    { // no next-next the the bottom-1
      const int temp = abs((srcppp[x] + (srcp[x] << 2) + srcppp[x]) - 3 * (srcpp[x] + srcpn[x]));
      if (temp > nt6) { if constexpr (SAD) diff_line += temp; else diff_line += (int64_t)temp * temp; }
    }
    diff += (diff_line + diffline_rounder) >> bit_shift;

    srcppp += src_pitch;
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;
    srcpnn += src_pitch;

    // bottom line
    diff_line = 0;
    for (int x = 0; x < width; x += inc)
    { // no next and next-next the the bottom
      const int temp = abs((srcppp[x] + (srcp[x] << 2) + srcppp[x]) - 3 * (srcpp[x] + srcpp[x]));
      if (temp > nt6) { if constexpr (SAD) diff_line += temp; else diff_line += (int64_t)temp * temp; }
    }
    diff += (diff_line + diffline_rounder) >> bit_shift;
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


// SSE: sum of squared errors (not the CPU)
template<bool ssd_mode>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif 
static void calcFieldDiff_SSEorSSD_uint16_SSE4_simd_8(const uint8_t *srcp_pp, ptrdiff_t src_pitch,
  int rowsize, int height, int nt6, int64_t &diff, int bits_per_pixel)
{
  // nt is already scaled to bit depth
  __m128i nt = _mm_set1_epi32(nt6);
  __m128i zero = _mm_setzero_si128();

  int bit_shift;
  if constexpr (!ssd_mode)
    bit_shift = (bits_per_pixel - 8); // SAD
  else
    bit_shift = 2 * (bits_per_pixel - 8); // SSE
  const int diffline_rounder = 1 << (bit_shift - 1);
  const auto rounder = _mm_set1_epi64x(diffline_rounder);

  const uint8_t *src2p_odd = srcp_pp + src_pitch;
  auto diff64 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&diff)); // target
  while (height--) {
    auto line_diff = _mm_setzero_si128();
    for (int x = 0; x < rowsize; x += 8) {
      auto _src_pp = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp_pp + x));
      auto _src_curr = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp_pp + src_pitch * 2 + x));
      auto _src_nn = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp_pp + src_pitch * 4 + x));

      // lower 4 pixel (8 bytes) to 4 int32 (16 bytes)
      auto _src_pp_lo = _mm_unpacklo_epi16(_src_pp, zero);
      auto _src_curr_lo = _mm_unpacklo_epi16(_src_curr, zero);
      auto _src_nn_lo = _mm_unpacklo_epi16(_src_nn, zero);
      auto sum1_lo = _mm_add_epi32(_mm_add_epi32(_src_pp_lo, _src_nn_lo), _mm_slli_epi32(_src_curr_lo, 2)); // pp + 4*c + nn
      // upper 4 pixel
      auto _src_pp_hi = _mm_unpackhi_epi16(_src_pp, zero);
      auto _src_curr_hi = _mm_unpackhi_epi16(_src_curr, zero);
      auto _src_nn_hi = _mm_unpackhi_epi16(_src_nn, zero);
      auto sum1_hi = _mm_add_epi32(_mm_add_epi32(_src_pp_hi, _src_nn_hi), _mm_slli_epi32(_src_curr_hi, 2)); // pp + 4*c + nn

      auto _src_p = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(src2p_odd + x));
      auto _src_n = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(src2p_odd + src_pitch * 2 + x));

      auto three = _mm_set1_epi32(3);

      // lower 4 pixels
      auto _src_p_lo = _mm_unpacklo_epi16(_src_p, zero);
      auto _src_n_lo = _mm_unpacklo_epi16(_src_n, zero);
      auto sum2_lo = _mm_mullo_epi32(_mm_add_epi32(_src_p_lo, _src_n_lo), three); // 3*(p + n)
      // upper 4 pixel
      auto _src_p_hi = _mm_unpackhi_epi16(_src_p, zero);
      auto _src_n_hi = _mm_unpackhi_epi16(_src_n, zero);
      auto sum2_hi = _mm_mullo_epi32(_mm_add_epi32(_src_p_hi, _src_n_hi), three); // 3*(p + n)

      auto absdiff_lo = _mm_abs_epi32(_mm_sub_epi32(sum1_lo, sum2_lo));
      auto absdiff_hi = _mm_abs_epi32(_mm_sub_epi32(sum1_hi, sum2_hi));

      auto res_lo = _mm_and_si128(absdiff_lo, _mm_cmpgt_epi32(absdiff_lo, nt)); // keep if > nt, 0 otherwise
      auto res_hi = _mm_and_si128(absdiff_hi, _mm_cmpgt_epi32(absdiff_hi, nt)); // keep if > nt, 0 otherwise

      if (ssd_mode) {
        line_diff = _mm_add_epi64(line_diff, _mm_mul_epu32(res_lo, res_lo)); // Sum Of Squares
        res_lo = _mm_srli_epi64(res_lo, 32);
        line_diff = _mm_add_epi64(line_diff, _mm_mul_epu32(res_lo, res_lo)); // Sum Of Squares

        line_diff = _mm_add_epi64(line_diff, _mm_mul_epu32(res_hi, res_hi)); // Sum Of Squares
        res_hi = _mm_srli_epi64(res_hi, 32);
        line_diff = _mm_add_epi64(line_diff, _mm_mul_epu32(res_hi, res_hi)); // Sum Of Squares
      }
      else {
        line_diff = _mm_add_epi64(line_diff, _mm_unpacklo_epi32(res_lo, zero)); // plain sum
        line_diff = _mm_add_epi64(line_diff, _mm_unpackhi_epi32(res_lo, zero));

        line_diff = _mm_add_epi64(line_diff, _mm_unpacklo_epi32(res_hi, zero)); // plain sum
        line_diff = _mm_add_epi64(line_diff, _mm_unpackhi_epi32(res_hi, zero));
      }
    }
    // avoid overflow, normalize result back to 8 bit scale, per line
    // line diff has two 64 bit sum content
    line_diff = _mm_add_epi64(line_diff, _mm_srli_si128(line_diff, 8));
    // single 64 bit
    line_diff = _mm_add_epi64(line_diff, rounder);
    line_diff = _mm_srli_epi64(line_diff, bit_shift);
    // update output, a single 64 bit number
    diff64 = _mm_add_epi64(diff64, line_diff);
    src2p_odd += src_pitch;
    srcp_pp += src_pitch;
  }
  _mm_storel_epi64(reinterpret_cast<__m128i *>(&diff), diff64);
}


template<bool yuy2luma_only, bool ssd_mode>
static void calcFieldDiff_SSEorSSD_uint8_SSE2_simd(const uint8_t *srcp_pp, ptrdiff_t src_pitch,
  int width, int height, int nt6, int64_t &diff)
{
  __m128i nt = _mm_set1_epi16(nt6);
  __m128i zero = _mm_setzero_si128();
  __m128i lumaWordMask = _mm_set1_epi32(0x0000FFFF);

  const uint8_t *src2p_odd = srcp_pp + src_pitch;
  auto diff64 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&diff));
  while (height--) {
    __m128i sum = _mm_setzero_si128();
    for (int x = 0; x < width; x += 16) {
      auto _src2p = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp_pp + x));
      auto _srcp = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp_pp + src_pitch * 2 + x));
      auto _src2n = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp_pp + src_pitch * 4 + x));
      auto _src_pp_lo = _mm_unpacklo_epi8(_src2p, zero);
      auto _src_pp_hi = _mm_unpackhi_epi8(_src2p, zero);
      auto _src_curr_lo = _mm_unpacklo_epi8(_srcp, zero);
      auto _src_curr_hi = _mm_unpackhi_epi8(_srcp, zero);
      auto _src_nn_lo = _mm_unpacklo_epi8(_src2n, zero);
      auto _src_nn_hi = _mm_unpackhi_epi8(_src2n, zero);
      auto sum1_lo = _mm_adds_epu16(_mm_adds_epu16(_src_pp_lo, _src_nn_lo), _mm_slli_epi16(_src_curr_lo, 2)); // pp + 4*c + nn
      auto sum1_hi = _mm_adds_epu16(_mm_adds_epu16(_src_pp_hi, _src_nn_hi), _mm_slli_epi16(_src_curr_hi, 2)); // pp + 4*c + nn

      auto _src_p = _mm_load_si128(reinterpret_cast<const __m128i *>(src2p_odd + x));
      auto _src_n = _mm_load_si128(reinterpret_cast<const __m128i *>(src2p_odd + src_pitch * 2 + x));
      auto _src_p_lo = _mm_unpacklo_epi8(_src_p, zero);
      auto _src_p_hi = _mm_unpackhi_epi8(_src_p, zero);
      auto _src_n_lo = _mm_unpacklo_epi8(_src_n, zero);
      auto _src_n_hi = _mm_unpackhi_epi8(_src_n, zero);
      auto three = _mm_set1_epi16(3);
      auto sum2_lo = _mm_mullo_epi16(_mm_adds_epu16(_src_p_lo, _src_n_lo), three); // 3*(pp + pn)
      auto sum2_hi = _mm_mullo_epi16(_mm_adds_epu16(_src_p_hi, _src_n_hi), three); //

      auto absdiff_lo = _mm_or_si128(_mm_subs_epu16(sum1_lo, sum2_lo), _mm_subs_epu16(sum2_lo, sum1_lo));
      auto absdiff_hi = _mm_or_si128(_mm_subs_epu16(sum1_hi, sum2_hi), _mm_subs_epu16(sum2_hi, sum1_hi));

      auto res_lo = _mm_and_si128(absdiff_lo, _mm_cmpgt_epi16(absdiff_lo, nt)); // keep if > nt, 0 otherwise
      auto res_hi = _mm_and_si128(absdiff_hi, _mm_cmpgt_epi16(absdiff_hi, nt));

      if (yuy2luma_only) {
        res_lo = _mm_and_si128(res_lo, lumaWordMask);
        res_hi = _mm_and_si128(res_hi, lumaWordMask);
      }

      __m128i res_lo2, res_hi2;

      if (ssd_mode) {
        res_lo2 = _mm_madd_epi16(res_lo, res_lo);
        res_hi2 = _mm_madd_epi16(res_hi, res_hi);
      }
      else {
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
    srcp_pp += src_pitch;
  }
  _mm_storel_epi64(reinterpret_cast<__m128i *>(&diff), diff64);

}


void FieldDiff::calcFieldDiff_SAD_SSE2(const uint8_t *srcp_pp, ptrdiff_t src_pitch,
  int width, int height, int nt6, int64_t &diff)
{
  // luma and chroma, sad_mode
  calcFieldDiff_SSEorSSD_uint8_SSE2_simd<false, false>(srcp_pp, src_pitch, width, height, nt6, diff);
}

void FieldDiff::calcFieldDiff_SAD_SSE2_YUY2_LumaOnly(const uint8_t *srcp_pp, ptrdiff_t src_pitch,
  int width, int height, int nt6, int64_t &diff)
{
  // yuy2 luma only, sad mode
  calcFieldDiff_SSEorSSD_uint8_SSE2_simd<true, false>(srcp_pp, src_pitch, width, height, nt6, diff);
}

void FieldDiff::calcFieldDiff_SSE_SSE2(const uint8_t *srcp_pp, ptrdiff_t src_pitch,
  int width, int height, int nt6, int64_t &diff)
{
  // w/o luma, ssd mode
  calcFieldDiff_SSEorSSD_uint8_SSE2_simd<false, true>(srcp_pp, src_pitch, width, height, nt6, diff);
}

void FieldDiff::calcFieldDiff_SSE_SSE2_YUY2_LumaOnly(const uint8_t *srcp_pp, ptrdiff_t src_pitch,
  int width, int height, int nt6, int64_t &diff)
{
  // with luma, ssd mode
  calcFieldDiff_SSEorSSD_uint8_SSE2_simd<true, true>(srcp_pp, src_pitch, width, height, nt6, diff);
}

#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif 
void FieldDiff::calcFieldDiff_SSE_uint16_SSE4(const uint8_t* srcp_pp, ptrdiff_t src_pitch,
  int width, int height, int nt6, int64_t& diff, int bits_per_pixel)
{
  // ssd mode
  calcFieldDiff_SSEorSSD_uint16_SSE4_simd_8<true>(srcp_pp, src_pitch, width, height, nt6, diff, bits_per_pixel);
}

#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif 
void FieldDiff::calcFieldDiff_SAD_uint16_SSE4(const uint8_t* srcp_pp, ptrdiff_t src_pitch,
  int width, int height, int nt6, int64_t& diff, int bits_per_pixel)
{
  // sad_mode
  calcFieldDiff_SSEorSSD_uint16_SSE4_simd_8<false>(srcp_pp, src_pitch, width, height, nt6, diff, bits_per_pixel);
}

