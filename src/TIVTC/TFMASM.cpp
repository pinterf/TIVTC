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

#include "TFMasm.h"
#include "emmintrin.h"

void checkSceneChangeYV12_1_SSE2(const unsigned char *prvp, const unsigned char *srcp,
  int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp)
{
  __m128i sum = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 16)
    {
      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i *>(prvp + x));
      __m128i src2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      __m128i sad = _mm_sad_epu8(src1, src2);
      sum = _mm_add_epi32(sum, sad);
    }
    prvp += prv_pitch;
    srcp += src_pitch;
  }
  __m128i res = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
  diffp = _mm_cvtsi128_si32(res);
}


void checkSceneChangeYV12_2_SSE2(const unsigned char *prvp, const unsigned char *srcp,
  const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch,
  int nxt_pitch, unsigned long &diffp, unsigned long &diffn)
{
  __m128i sump = _mm_setzero_si128();
  __m128i sumn = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 16)
    {
      __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i *>(prvp + x));
      __m128i src_curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i *>(nxtp + x));
      __m128i sadp = _mm_sad_epu8(src_prev, src_curr);
      __m128i sadn = _mm_sad_epu8(src_next, src_curr);
      sump = _mm_add_epi32(sump, sadp);
      sumn = _mm_add_epi32(sumn, sadn);
    }
    prvp += prv_pitch;
    srcp += src_pitch;
    nxtp += nxt_pitch;
  }
  __m128i resp = _mm_add_epi32(sump, _mm_srli_si128(sump, 8));
  diffp = _mm_cvtsi128_si32(resp);
  __m128i resn = _mm_add_epi32(sumn, _mm_srli_si128(sumn, 8));
  diffn = _mm_cvtsi128_si32(resn);
}


void checkSceneChangeYUY2_1_SSE2(const unsigned char *prvp, const unsigned char *srcp,
  int height, int width, int prv_pitch, int src_pitch, unsigned long &diffp)
{
  __m128i sum = _mm_setzero_si128();
  __m128i lumaMask = _mm_set1_epi16(0x00FF);
  while (height--) {
    for (int x = 0; x < width; x += 16)
    {
      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i *>(prvp + x));
      __m128i src2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      src1 = _mm_and_si128(src1, lumaMask);
      src2 = _mm_and_si128(src2, lumaMask);
      __m128i sad = _mm_sad_epu8(src1, src2);
      sum = _mm_add_epi32(sum, sad);
    }
    prvp += prv_pitch;
    srcp += src_pitch;
  }
  __m128i res = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
  diffp = _mm_cvtsi128_si32(res);
}


void checkSceneChangeYUY2_2_SSE2(const unsigned char *prvp, const unsigned char *srcp,
  const unsigned char *nxtp, int height, int width, int prv_pitch, int src_pitch,
  int nxt_pitch, unsigned long &diffp, unsigned long &diffn)
{
  __m128i sump = _mm_setzero_si128();
  __m128i sumn = _mm_setzero_si128();
  __m128i lumaMask = _mm_set1_epi16(0x00FF);
  while (height--) {
    for (int x = 0; x < width; x += 16)
    {
      __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i *>(prvp + x));
      __m128i src_curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i *>(nxtp + x));
      src_prev = _mm_and_si128(src_prev, lumaMask);
      src_curr = _mm_and_si128(src_curr, lumaMask);
      src_next = _mm_and_si128(src_next, lumaMask);
      __m128i sadp = _mm_sad_epu8(src_prev, src_curr);
      __m128i sadn = _mm_sad_epu8(src_next, src_curr);
      sump = _mm_add_epi32(sump, sadp);
      sumn = _mm_add_epi32(sumn, sadn);
    }
    prvp += prv_pitch;
    srcp += src_pitch;
    nxtp += nxt_pitch;
  }
  __m128i resp = _mm_add_epi32(sump, _mm_srli_si128(sump, 8));
  diffp = _mm_cvtsi128_si32(resp);
  __m128i resn = _mm_add_epi32(sumn, _mm_srli_si128(sumn, 8));
  diffn = _mm_cvtsi128_si32(resn);
}




template<bool with_luma_mask>
static void check_combing_SSE2_generic_simd(const unsigned char *srcp, unsigned char *dstp, int width,
  int height, int src_pitch, int src_pitch2, int dst_pitch, __m128i threshb, __m128i thresh6w)
{
  __m128i all_ff = _mm_set1_epi8(-1);
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto next = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch + x));
      auto curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      auto prev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch + x));
      auto diff_curr_next = _mm_subs_epu8(curr, next);
      auto diff_next_curr = _mm_subs_epu8(next, curr);
      auto diff_curr_prev = _mm_subs_epu8(curr, prev);
      auto diff_prev_curr = _mm_subs_epu8(prev, curr);
      // max(min(p-s,n-s), min(s-n,s-p))
      auto xmm2_max = _mm_max_epu8(_mm_min_epu8(diff_prev_curr, diff_next_curr), _mm_min_epu8(diff_curr_next, diff_curr_prev));
      auto xmm2_cmp = _mm_cmpeq_epi8(_mm_adds_epu8(xmm2_max, threshb), all_ff);
      if (with_luma_mask) {
        __m128i lumaMask = _mm_set1_epi16(0x00FF);
        xmm2_cmp = _mm_and_si128(xmm2_cmp, lumaMask);
      }
      auto res_part1 = xmm2_cmp;
      bool cmpres_is_allzero;
#ifdef _M_X64
      cmpres_is_allzero = (_mm_cvtsi128_si64(xmm2_cmp) | _mm_cvtsi128_si64(_mm_srli_si128(xmm2_cmp, 8))) == 0; // _si64: only at x64 platform
#else
      cmpres_is_allzero = (_mm_cvtsi128_si32(xmm2_cmp) |
        _mm_cvtsi128_si32(_mm_srli_si128(xmm2_cmp, 4)) |
        _mm_cvtsi128_si32(_mm_srli_si128(xmm2_cmp, 8)) |
        _mm_cvtsi128_si32(_mm_srli_si128(xmm2_cmp, 12))
        ) == 0;
#endif
        if (!cmpres_is_allzero) {
          // output2
          auto zero = _mm_setzero_si128();
          // compute 3*(p+n)
          auto next_lo = _mm_unpacklo_epi8(next, zero);
          auto prev_lo = _mm_unpacklo_epi8(prev, zero);
          auto next_hi = _mm_unpackhi_epi8(next, zero);
          auto prev_hi = _mm_unpackhi_epi8(prev, zero);
          __m128i threeMask = _mm_set1_epi16(3);
          auto mul_lo = _mm_mullo_epi16(_mm_adds_epu16(next_lo, prev_lo), threeMask);
          auto mul_hi = _mm_mullo_epi16(_mm_adds_epu16(next_hi, prev_hi), threeMask);

          // compute (pp+c*4+nn)
          auto prevprev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch * 2 + x));
          auto prevprev_lo = _mm_unpacklo_epi8(prevprev, zero);
          auto prevprev_hi = _mm_unpackhi_epi8(prevprev, zero);
          auto curr_lo = _mm_unpacklo_epi8(curr, zero);
          auto curr_hi = _mm_unpackhi_epi8(curr, zero);
          auto sum2_lo = _mm_adds_epu16(_mm_slli_epi16(curr_lo, 2), prevprev_lo); // pp + c*4
          auto sum2_hi = _mm_adds_epu16(_mm_slli_epi16(curr_hi, 2), prevprev_hi); // pp + c*4

          auto nextnext = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch * 2 + x));
          auto nextnext_lo = _mm_unpacklo_epi8(nextnext, zero);
          auto nextnext_hi = _mm_unpackhi_epi8(nextnext, zero);
          auto sum3_lo = _mm_adds_epu16(sum2_lo, nextnext_lo);
          auto sum3_hi = _mm_adds_epu16(sum2_hi, nextnext_hi);

          // working with sum3=(pp+c*4+nn)   and  mul=3*(p+n)
          auto diff_sum3lo_mullo = _mm_subs_epu16(sum3_lo, mul_lo);
          auto diff_mullo_sum3lo = _mm_subs_epu16(mul_lo, sum3_lo);
          auto diff_sum3hi_mulhi = _mm_subs_epu16(sum3_hi, mul_hi);
          auto diff_mulhi_sum3hi = _mm_subs_epu16(mul_hi, sum3_hi);
          // abs( (pp+c*4+nn) - mul=3*(p+n) )
          auto max_lo = _mm_max_epi16(diff_sum3lo_mullo, diff_mullo_sum3lo);
          auto max_hi = _mm_max_epi16(diff_sum3hi_mulhi, diff_mulhi_sum3hi);
          // abs( (pp+c*4+nn) - mul=3*(p+n) ) + thresh6w
          auto lo_thresh6w_added = _mm_adds_epu16(max_lo, thresh6w);
          auto hi_thresh6w_added = _mm_adds_epu16(max_hi, thresh6w);
          // maximum reached?
          auto cmp_lo = _mm_cmpeq_epi16(lo_thresh6w_added, all_ff);
          auto cmp_hi = _mm_cmpeq_epi16(hi_thresh6w_added, all_ff);

          auto res_lo = _mm_srli_epi16(cmp_lo, 8);
          auto res_hi = _mm_srli_epi16(cmp_hi, 8);
          auto res_part2 = _mm_packus_epi16(res_lo, res_hi);

          auto res = _mm_and_si128(res_part1, res_part2);
          _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
        }
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}




// fixme: buggy! delete from here and use tdeintasm!!!
#if 0
// no alignment needed for 8 bytes
void compute_sum_8x8_sse2(const unsigned char *srcp, int pitch, int &sum)
{
  // sums masks
  // if (cmkppT[x + v] == 0xFF && cmkpT[x + v] == 0xFF && cmkpnT[x + v] == 0xFF) sum++;
  __m128i onesMask = _mm_set1_epi8(1);
  __m128i prev0 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp));
  __m128i prev1_currMinus1 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp + pitch));
  __m128i zero = _mm_setzero_si128();
  __m128i summa = _mm_setzero_si128();
  srcp += pitch * 2;
  for (int i = 0; i < 4; i++) { // 4x2=8
    __m128i curr0_prev2 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp));
    __m128i curr1 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp + pitch));

    __m128i prev_anded = _mm_and_si128(prev0, prev1_currMinus1);
    prev_anded = _mm_and_si128(prev_anded, curr0_prev2); // prev0 prev1 prev2
    prev_anded = _mm_and_si128(prev_anded, onesMask);

    __m128i curr_anded = _mm_and_si128(curr0_prev2, curr1);
    curr_anded = _mm_and_si128(curr_anded, prev1_currMinus1); // currMinus1 curr0 curr1
    curr_anded = _mm_and_si128(curr_anded, onesMask);

    prev0 = curr0_prev2;
    prev1_currMinus1 = curr1;

    summa = _mm_adds_epu8(summa, prev_anded);
    summa = _mm_adds_epu8(summa, curr_anded);
    srcp += pitch * 2;
  }
  // now we have to sum up lower 8 bytes
  // in sse2, we use sad
  __m128i tmpsum = _mm_sad_epu8(summa, zero);  // sum(lo 8 bytes)(needed) / sum(hi 8 bytes)(not needed)
  sum = _mm_cvtsi128_si32(tmpsum);
}
#endif

// fixme: buggy! delete from here and use tdeintasm!!!
#if 0
template<bool aligned>
void compute_sum_16x8_sse2_luma(const unsigned char *srcp, int pitch, int &sum)
{
  // sums masks
  // if (cmkppT[x + v] == 0xFF && cmkpT[x + v] == 0xFF && cmkpnT[x + v] == 0xFF) sum++;
  __m128i onesMask = _mm_set1_epi16(1); // onesMaskLuma Word(1)
  __m128i prev0, prev1_currMinus1;
  if (aligned) {
    prev0 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp));
    prev1_currMinus1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + pitch));
  }
  else {
    prev0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcp));
    prev1_currMinus1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcp + pitch));
  }
  __m128i zero = _mm_setzero_si128();
  __m128i summa = _mm_setzero_si128();
  srcp += pitch * 2;
  for (int i = 0; i < 4; i++) { // 4x2=8
    __m128i curr0_prev2, curr1;
    if (aligned) {
      curr0_prev2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp));
      curr1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + pitch));
    }
    else {
      curr0_prev2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcp));
      curr1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcp + pitch));
    }

    __m128i prev_anded = _mm_and_si128(prev0, prev1_currMinus1);
    prev_anded = _mm_and_si128(prev_anded, curr0_prev2); // prev0 prev1 prev2
    prev_anded = _mm_and_si128(prev_anded, onesMask);

    __m128i curr_anded = _mm_and_si128(curr0_prev2, curr1);
    curr_anded = _mm_and_si128(curr_anded, prev1_currMinus1); // currMinus1 curr0 curr1
    curr_anded = _mm_and_si128(curr_anded, onesMask);

    prev0 = curr0_prev2;
    prev1_currMinus1 = curr1;

    summa = _mm_adds_epu8(summa, prev_anded);
    summa = _mm_adds_epu8(summa, curr_anded);
    srcp += pitch * 2;
  }
  // now we have to sum up lower 8 bytes
  // in sse2, we use sad
  __m128i tmpsum = _mm_sad_epu8(summa, zero);  // sum(lo 8 bytes) / sum(hi 8 bytes)
  tmpsum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // lo + hi
  sum = _mm_cvtsi128_si32(tmpsum);
}

// instantiate
template void compute_sum_16x8_sse2_luma<false>(const unsigned char *srcp, int pitch, int &sum);
template void compute_sum_16x8_sse2_luma<true>(const unsigned char *srcp, int pitch, int &sum);
#endif
