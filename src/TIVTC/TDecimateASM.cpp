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

#include "TDecimate.h"
#include "TDecimateASM.h"
#include "TCommonASM.h"
#include "emmintrin.h"
#include "smmintrin.h" // SSE4
#include <assert.h>

static void blend_uint8_c(uint8_t* dstp, const uint8_t* srcp1,
  const uint8_t* srcp2, int width, int height, int dst_pitch,
  int src1_pitch, int src2_pitch, int weight_i)
{
  // weight_i is 16 bit scaled
  assert(weight_i != 0 && weight_i != 65536);

  const int invweight_i = 65536 - weight_i;

  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      dstp[x] = (weight_i * srcp1[x] + invweight_i * srcp2[x] + 32768) >> 16;
    }
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
    dstp += dst_pitch;
  }
}

static void blend_uint16_c(uint8_t* dstp, const uint8_t* srcp1,
  const uint8_t* srcp2, int width, int height, int dst_pitch,
  int src1_pitch, int src2_pitch, int weight_i, int bits_per_pixel)
{
  // weight_i is 15 bit scaled
  // min and max cases handled earlier
  assert(weight_i != 0 && weight_i != 32768);

  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      const int src1 = reinterpret_cast<const uint16_t*>(srcp1)[x];
      const int src2 = reinterpret_cast<const uint16_t*>(srcp2)[x];
      const int result = src2 + (((src1 - src2) * weight_i + 16384) >> 15);
      reinterpret_cast<uint16_t*>(dstp)[x] = std::max(std::min(result, max_pixel_value), 0);
      //  (reinterpret_cast<const uint16_t*>(srcp1)[x] * weight_i + reinterpret_cast<const uint16_t*>(srcp2)[x] * invweight_i + 16384) >> 15;
    }
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
    dstp += dst_pitch;
  }
}

static void blend_uint8_SSE2(uint8_t* dstp, const uint8_t* srcp1,
  const uint8_t* srcp2, int width, int height, int dst_pitch,
  int src1_pitch, int src2_pitch, int weight_i)
{
  // weight_i is 16 bit scaled
  assert(weight_i != 0 && weight_i != 65536);
  // 0 and max weights are handled earlier
  __m128i iw1 = _mm_set1_epi16((short)weight_i);
  __m128i iw2 = _mm_set1_epi16((short)(65536 - weight_i));
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp1 + x));
      __m128i src2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp2 + x));
      __m128i src1_lo = _mm_unpacklo_epi8(src1, src1);
      __m128i src2_lo = _mm_unpacklo_epi8(src2, src2);
      __m128i src1_hi = _mm_unpackhi_epi8(src1, src1);
      __m128i src2_hi = _mm_unpackhi_epi8(src2, src2);
      // small note: mulhi does not round. difference from C
      // mulhi: instead of >> 16 we get the hi16 bit immediately
      __m128i mulres_lo = _mm_adds_epu16(_mm_mulhi_epu16(src1_lo, iw1), _mm_mulhi_epu16(src2_lo, iw2));
      __m128i mulres_hi = _mm_adds_epu16(_mm_mulhi_epu16(src1_hi, iw1), _mm_mulhi_epu16(src2_hi, iw2));

      mulres_lo = _mm_srli_epi16(mulres_lo, 8);
      mulres_hi = _mm_srli_epi16(mulres_hi, 8);

      __m128i res = _mm_packus_epi16(mulres_lo, mulres_hi);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    dstp += dst_pitch;
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
  }
}


template<bool lessThan16bits>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif 
static void blend_uint16_SSE4(uint8_t* dstp, const uint8_t* srcp1, const uint8_t* srcp2,
  int width, int height,
  int dst_pitch, int src1_pitch, int src2_pitch, int weight_i, int bits_per_pixel)
{
  assert(weight_i != 0 && weight_i != 32768);
  // full copy cases have to be handled earlier
  // 15 bit integer arithwetic
  auto round_mask = _mm_set1_epi32(0x4000); // 32768/2
  auto weight = _mm_set1_epi32(weight_i);
  auto zero = _mm_setzero_si128();

  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  auto max_pixel_value_128 = _mm_set1_epi16((short)max_pixel_value);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width * (int)sizeof(uint16_t); x += 16) {
      auto src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp1 + x));
      auto src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp2 + x));

      auto src1_lo = _mm_unpacklo_epi16(src1, zero);
      auto src1_hi = _mm_unpackhi_epi16(src1, zero);

      auto src2_lo = _mm_unpacklo_epi16(src2, zero);
      auto src2_hi = _mm_unpackhi_epi16(src2, zero);

      // return src2 +(((src1 - src2) * weight_15bits + round) >> 15);

      auto diff_lo = _mm_sub_epi32(src1_lo, src2_lo);
      auto diff_hi = _mm_sub_epi32(src1_hi, src2_hi);

      auto lerp_lo = _mm_mullo_epi32(diff_lo, weight);
      auto lerp_hi = _mm_mullo_epi32(diff_hi, weight);

      lerp_lo = _mm_srai_epi32(_mm_add_epi32(lerp_lo, round_mask), 15);
      lerp_hi = _mm_srai_epi32(_mm_add_epi32(lerp_hi, round_mask), 15);

      auto result_lo = _mm_add_epi32(src2_lo, lerp_lo);
      auto result_hi = _mm_add_epi32(src2_hi, lerp_hi);

      auto result = _mm_packus_epi32(result_lo, result_hi);
      if constexpr(lessThan16bits) // otherwise no clamp needed
        result = _mm_min_epu16(result, max_pixel_value_128);

      _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), result);
    }

    dstp += dst_pitch;
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
  }
}

// handles 50% special case as well
// hbd ready
void dispatch_blend(uint8_t* dstp, const uint8_t* srcp1, const uint8_t* srcp2, int width, int height,
  int dst_pitch, int src1_pitch, int src2_pitch, int weight_i, int bits_per_pixel, int cpuFlags)
{
  const bool use_sse2 = cpuFlags & CPUF_SSE2;
  const bool use_sse4 = cpuFlags & CPUF_SSE4_1;

  // weight_i 0 and max --> copy is already handled!
  // weight_i is of 15 bit scale

  // special 50% case
  if (weight_i == 32768 / 2) {
    if (bits_per_pixel == 8) {
      if (use_sse2)
        blend_5050_SSE2<uint8_t>(dstp, srcp1, srcp2, width, height, dst_pitch, src1_pitch, src2_pitch);
      else
        blend_5050_c<uint8_t>(dstp, srcp1, srcp2, width, height, dst_pitch, src1_pitch, src2_pitch);
    }
    else {
      if (use_sse2)
        blend_5050_SSE2<uint16_t>(dstp, srcp1, srcp2, width, height, dst_pitch, src1_pitch, src2_pitch);
      else
        blend_5050_c<uint16_t>(dstp, srcp1, srcp2, width, height, dst_pitch, src1_pitch, src2_pitch);
    }
    return;
  }

  // arbitrary blend
  if (bits_per_pixel == 8) {
    // using 16 bit scaled values inside instead of 15 bit scaled
    if(use_sse2)
      blend_uint8_SSE2(dstp, srcp1, srcp2, width, height, dst_pitch, src1_pitch, src2_pitch, weight_i * 2);
    else
      blend_uint8_c(dstp, srcp1, srcp2, width, height, dst_pitch, src1_pitch, src2_pitch, weight_i * 2);
    return;
  }

  // 10-16 bits
  if (use_sse4) {
    if (bits_per_pixel < 16)
      blend_uint16_SSE4<true>(dstp, srcp1, srcp2, width, height, dst_pitch, src1_pitch, src2_pitch, weight_i, bits_per_pixel);
    else
      blend_uint16_SSE4<false>(dstp, srcp1, srcp2, width, height, dst_pitch, src1_pitch, src2_pitch, weight_i, bits_per_pixel);
  }
  else {
    blend_uint16_c(dstp, srcp1, srcp2, width, height, dst_pitch, src1_pitch, src2_pitch, weight_i, bits_per_pixel);
  }
}


void calcLumaDiffYUY2SAD_SSE2_16(const uint8_t *prvp, const uint8_t *nxtp,
  int width, int height, int prv_pitch, int nxt_pitch, uint64_t &sad)
{
  sad = 0; 
  __m128i sum = _mm_setzero_si128();
  const __m128i lumaMask = _mm_set1_epi16(0x00FF);
  while (height--) {
    for (int x = 0; x < width; x += 16)
    {
      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i *>(prvp + x));
      __m128i src2 = _mm_load_si128(reinterpret_cast<const __m128i *>(nxtp + x));
      src1 = _mm_and_si128(src1, lumaMask);
      src2 = _mm_and_si128(src2, lumaMask);
      __m128i tmp = _mm_sad_epu8(src1, src2);
      sum = _mm_add_epi64(sum, tmp);
    }
    prvp += prv_pitch;
    nxtp += nxt_pitch;
  }
  sum = _mm_add_epi64(sum, _mm_srli_si128(sum, 8)); // add lo, hi
  _mm_storel_epi64(reinterpret_cast<__m128i*>(&sad), sum);
}


void calcLumaDiffYUY2SSD_SSE2_16(const uint8_t *prvp, const uint8_t *nxtp,
  int width, int height, int prv_pitch, int nxt_pitch, uint64_t &ssd)
{
  ssd = 0; // sum of squared differences
  const __m128i lumaMask = _mm_set1_epi16(0x00FF);
  while (height--) {
    __m128i zero = _mm_setzero_si128();
    __m128i rowsum = _mm_setzero_si128(); // pxor xmm6, xmm6

    for (int x = 0; x < width; x += 16)
    {
      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i *>(prvp + x)); // movdqa tmp, [edi + eax]
      __m128i src2 = _mm_load_si128(reinterpret_cast<const __m128i *>(nxtp + x)); // movdqa xmm1, [esi + eax]
      __m128i diff12 = _mm_subs_epu8(src1, src2);
      __m128i diff21 = _mm_subs_epu8(src2, src1);
      __m128i tmp = _mm_or_si128(diff12, diff21);
      tmp = _mm_and_si128(tmp, lumaMask);
      tmp = _mm_madd_epi16(tmp, tmp);
      rowsum = _mm_add_epi32(rowsum, tmp);
    }
    __m128i sum_lo = _mm_unpacklo_epi32(rowsum, zero); // punpckldq xmm6, xmm5
    __m128i sum_hi = _mm_unpackhi_epi32(rowsum, zero); // punpckhdq tmp, xmm5
    __m128i sum = _mm_add_epi64(sum_lo, sum_hi); // paddq xmm6, tmp

    __m128i res = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&ssd)); // movq xmm1, qword ptr[eax]
    // low 64
    res = _mm_add_epi64(res, sum);
    // high 64
    res = _mm_add_epi64(res, _mm_srli_si128(sum, 8));
    _mm_storel_epi64(reinterpret_cast<__m128i*>(&ssd), res);
    prvp += prv_pitch;
    nxtp += nxt_pitch;
  }
}

template<int blkSizeY>
void calcSAD_SSE2_16xN(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& sad)
{
  assert(0 == blkSizeY % 8);

  __m128i tmpsum = _mm_setzero_si128();
  // unrolled loop
  for (int i = 0; i < blkSizeY / 8; i++) {
    __m128i xmm0, xmm1;
    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    __m128i tmp1 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    __m128i tmp2 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    __m128i tmp3 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    __m128i tmp4 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_add_epi32(tmp1, tmp2);
    xmm1 = _mm_add_epi32(tmp3, tmp4);
    tmpsum = _mm_add_epi32(tmpsum, xmm0);
    tmpsum = _mm_add_epi32(tmpsum, xmm1);
  }
  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  sad = _mm_cvtsi128_si32(sum);
}

// only 411 uses
template<int blkSizeY>
void calcSAD_C_2xN(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& sad)
{
  int tmpsum = 0;
  for (int i = 0; i < blkSizeY; i++) {
    tmpsum += abs(ptr1[0] - ptr2[0]);
    tmpsum += abs(ptr1[1] - ptr2[1]);
    ptr1 += pitch1;
    ptr2 += pitch2;
  }

  sad = tmpsum;
}

template<int blkSizeY>
void calcSSD_C_2xN(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& sad)
{
  int tmpsum = 0;
  for (int i = 0; i < blkSizeY; i++) {
    const int tmp0 = ptr1[0] - ptr2[0];
    const int tmp1 = ptr1[1] - ptr2[1];
    tmpsum += tmp0 * tmp0 + tmp1 * tmp1;
    ptr1 += pitch1;
    ptr2 += pitch2;
  }

  sad = tmpsum;
}


template<int blkSizeY>
void calcSAD_SSE2_4xN(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &sad)
{
  assert(0 == blkSizeY % 4);

  __m128i tmpsum = _mm_setzero_si128();
  // unrolled loop
  for (int i = 0; i < blkSizeY / 4; i++) {
    __m128i xmm0, xmm1;
    xmm0 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr1)));
    xmm1 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr1 + pitch1)));
    xmm0 = _mm_sad_epu8(xmm0, _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr2))));
    xmm1 = _mm_sad_epu8(xmm1, _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr2 + pitch2))));
    __m128i tmp1 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr1)));
    xmm1 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr1 + pitch1)));
    xmm0 = _mm_sad_epu8(xmm0, _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr2))));
    xmm1 = _mm_sad_epu8(xmm1, _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr2 + pitch2))));
    __m128i tmp2 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_add_epi32(tmp1, tmp2);
    tmpsum = _mm_add_epi32(tmpsum, xmm0);
  }

  sad = _mm_cvtsi128_si32(tmpsum); // we have only lo
}

template<int blkSizeY>
void calcSAD_SSE2_8xN(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &sad)
{
  assert(0 == blkSizeY % 8);

  __m128i tmpsum = _mm_setzero_si128();
  // blkSizeY should be multiple of 8
  // unrolled loop
  for (int i = 0; i < blkSizeY / 8; i++) {
    __m128i xmm0, xmm1;
    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    __m128i tmp1 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    __m128i tmp2 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    __m128i tmp3 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    __m128i tmp4 = _mm_add_epi32(xmm0, xmm1);
    ptr1 += pitch1 * 2; // if last, no need more, hope compiler solves it
    ptr2 += pitch2 * 2;

    xmm0 = _mm_add_epi32(tmp1, tmp2);
    xmm1 = _mm_add_epi32(tmp3, tmp4);
    tmpsum = _mm_add_epi32(tmpsum, xmm0);
    tmpsum = _mm_add_epi32(tmpsum, xmm1);
  }

  sad = _mm_cvtsi128_si32(tmpsum); // we have only lo
}

// new
void calcSAD_SSE2_8x8_YUY2_lumaonly(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &sad)
{
  __m128i tmpsum = _mm_setzero_si128();
  const __m128i lumaMask = _mm_set1_epi16(0x00FF);
  // unrolled loop
  __m128i xmm0, xmm1;
  xmm0 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1)), lumaMask);
  xmm1 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1 + pitch1)), lumaMask);
  xmm0 = _mm_sad_epu8(xmm0, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2)), lumaMask));
  xmm1 = _mm_sad_epu8(xmm1, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2 + pitch2)), lumaMask));
  __m128i tmp1 = _mm_add_epi32(xmm0, xmm1);
  ptr1 += pitch1 * 2;
  ptr2 += pitch2 * 2;

  xmm0 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1)), lumaMask);
  xmm1 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1 + pitch1)), lumaMask);
  xmm0 = _mm_sad_epu8(xmm0, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2)), lumaMask));
  xmm1 = _mm_sad_epu8(xmm1, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2 + pitch2)), lumaMask));
  __m128i tmp2 = _mm_add_epi32(xmm0, xmm1);
  ptr1 += pitch1 * 2;
  ptr2 += pitch2 * 2;

  xmm0 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1)), lumaMask);
  xmm1 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1 + pitch1)), lumaMask);
  xmm0 = _mm_sad_epu8(xmm0, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2)), lumaMask));
  xmm1 = _mm_sad_epu8(xmm1, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2 + pitch2)), lumaMask));
  __m128i tmp3 = _mm_add_epi32(xmm0, xmm1);
  ptr1 += pitch1 * 2;
  ptr2 += pitch2 * 2;

  xmm0 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1)), lumaMask);
  xmm1 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1 + pitch1)), lumaMask);
  xmm0 = _mm_sad_epu8(xmm0, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2)), lumaMask));
  xmm1 = _mm_sad_epu8(xmm1, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2 + pitch2)), lumaMask));
  __m128i tmp4 = _mm_add_epi32(xmm0, xmm1);
  // ptr1 += pitch1 * 2; // last one, no need more 
  // ptr2 += pitch2 * 2;

  xmm0 = _mm_add_epi32(tmp1, tmp2);
  xmm1 = _mm_add_epi32(tmp3, tmp4);
  tmpsum = _mm_add_epi32(tmpsum, xmm0);
  tmpsum = _mm_add_epi32(tmpsum, xmm1);

  sad = _mm_cvtsi128_si32(tmpsum); // we have only lo
}

// really YUY2 16x16 with chroma
void calcSAD_SSE2_32x16(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int& sad)
{
  __m128i tmpsum = _mm_setzero_si128();
  // unrolled loop 4 lines
  for (int i = 0; i < 16 / 4; i++) {
    __m128i xmm0, xmm1, xmm2, xmm3;
    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + 16));
    xmm2 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm3 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1 + 16));

    xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + 16)));
    xmm2 = _mm_sad_epu8(xmm2, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    xmm3 = _mm_sad_epu8(xmm3, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2 + 16)));

    __m128i tmp1 = _mm_add_epi32(xmm0, xmm1);
    __m128i tmp2 = _mm_add_epi32(xmm2, xmm3);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + 16));
    xmm2 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm3 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1 + 16));
    xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + 16)));
    xmm2 = _mm_sad_epu8(xmm2, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    xmm3 = _mm_sad_epu8(xmm3, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2 + 16)));

    __m128i tmp3 = _mm_add_epi32(xmm0, xmm1);
    __m128i tmp4 = _mm_add_epi32(xmm2, xmm3);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_add_epi32(tmp1, tmp2);
    xmm1 = _mm_add_epi32(tmp3, tmp4);
    tmpsum = _mm_add_epi32(tmpsum, xmm0);
    tmpsum = _mm_add_epi32(tmpsum, xmm1);
  }
  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  sad = _mm_cvtsi128_si32(sum);
}

// really YUY2 16x16 no chroma
void calcSAD_SSE2_32x16_YUY2_lumaonly(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &sad)
{
  __m128i tmpsum = _mm_setzero_si128();
  // unrolled loop
  const __m128i luma = _mm_set1_epi16(0x00FF);

  for (int i = 0; i < 16/2; i++) {
    __m128i xmm0, xmm1, xmm2, xmm3;
    xmm0 = _mm_and_si128(_mm_load_si128(reinterpret_cast<const __m128i *>(ptr1)), luma);
    xmm1 = _mm_and_si128(_mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + 16)), luma);
    xmm2 = _mm_and_si128(_mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1)), luma);
    xmm3 = _mm_and_si128(_mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1 + 16)), luma);
    
    xmm0 = _mm_sad_epu8(xmm0, _mm_and_si128(_mm_load_si128(reinterpret_cast<const __m128i *>(ptr2)), luma));
    xmm1 = _mm_sad_epu8(xmm1, _mm_and_si128(_mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + 16)), luma));
    xmm2 = _mm_sad_epu8(xmm2, _mm_and_si128(_mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2)), luma));
    xmm3 = _mm_sad_epu8(xmm3, _mm_and_si128(_mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2 + 16)), luma));

    __m128i tmp1 = _mm_add_epi32(xmm0, xmm1);
    __m128i tmp2 = _mm_add_epi32(xmm2, xmm3);
    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;

    xmm0 = _mm_add_epi32(tmp1, tmp2);
    tmpsum = _mm_add_epi32(tmpsum, xmm0);
  }
  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  sad = _mm_cvtsi128_si32(sum);
}


template<int blkSizeY>
void calcSSD_SSE2_4xN(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &ssd)
{
  assert(0 == blkSizeY % 2);

  __m128i tmpsum = _mm_setzero_si128();
  __m128i zero = _mm_setzero_si128();
  // two lines at a time -> 4 = 2x2
  for (int i = 0; i < blkSizeY / 2; i++) {
    __m128i xmm0, xmm1, xmm2, xmm3;
    __m128i tmp0, tmp1, tmp0lo, tmp1lo;
    // two lines, 4 byte / 32 bit loads
    xmm0 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr1)));
    xmm1 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float *>(ptr1 + pitch1)));
    xmm2 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float *>(ptr2)));
    xmm3 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float *>(ptr2 + pitch2)));

    tmp0 = _mm_or_si128(_mm_subs_epu8(xmm0, xmm2), _mm_subs_epu8(xmm2, xmm0)); // only low 4 bytes are valid
    tmp1 = _mm_or_si128(_mm_subs_epu8(xmm1, xmm3), _mm_subs_epu8(xmm3, xmm1));

    tmp0lo = _mm_unpacklo_epi8(tmp0, zero); // only low 8 bytes (4 words, 64 bits) are valid
    tmp0lo = _mm_madd_epi16(tmp0lo, tmp0lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp0lo);

    tmp1lo = _mm_unpacklo_epi8(tmp1, zero);
    tmp1lo = _mm_madd_epi16(tmp1lo, tmp1lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp1lo);

    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;
  }
  // we have only lo64 in tmpsum
  __m128i sum64lo = _mm_unpacklo_epi32(tmpsum, zero); // move to 64 bit boundary
  //__m128i sum64hi = _mm_unpackhi_epi32(tmpsum, zero);
  tmpsum = sum64lo;

  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  ssd = _mm_cvtsi128_si32(sum);
}

template<int blkSizeY>
void calcSSD_SSE2_8xN(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &ssd)
{
  assert(0 == blkSizeY % 2);

  // even blkSize Y 8x8, 8x16
  __m128i tmpsum = _mm_setzero_si128();
  __m128i zero = _mm_setzero_si128();
  // two lines at a time -> 8 = 4x2
  for (int i = 0; i < blkSizeY / 2; i++) {
    __m128i xmm0, xmm1, xmm2, xmm3;
    __m128i tmp0, tmp1, tmp0lo, tmp1lo;
    // two lines, only lower 8 bytes
    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1 + pitch1));
    xmm2 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2));
    xmm3 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2 + pitch2));

    // abs diff
    tmp0 = _mm_or_si128(_mm_subs_epu8(xmm0, xmm2), _mm_subs_epu8(xmm2, xmm0));
    tmp1 = _mm_or_si128(_mm_subs_epu8(xmm1, xmm3), _mm_subs_epu8(xmm3, xmm1));

    tmp0lo = _mm_unpacklo_epi8(tmp0, zero);
    tmp0lo = _mm_madd_epi16(tmp0lo, tmp0lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp0lo);

    tmp1lo = _mm_unpacklo_epi8(tmp1, zero);
    tmp1lo = _mm_madd_epi16(tmp1lo, tmp1lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp1lo);

    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;
  }
  __m128i sum64lo = _mm_unpacklo_epi32(tmpsum, zero); // move to 64 bit boundary
  __m128i sum64hi = _mm_unpackhi_epi32(tmpsum, zero);
  tmpsum = _mm_add_epi64(sum64lo, sum64hi);

  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  ssd = _mm_cvtsi128_si32(sum);
}

void calcSSD_SSE2_8x8_YUY2_lumaonly(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &ssd)
{
  __m128i tmpsum = _mm_setzero_si128();
  __m128i zero = _mm_setzero_si128();
  const __m128i lumaMask = _mm_set1_epi16(0x00FF);
  // two lines at a time -> 4x2
  for (int i = 0; i < 4; i++) {
    __m128i xmm0, xmm1, xmm2, xmm3;
    __m128i tmp0, tmp1;
    // two lines
    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1 + pitch1));
    xmm2 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2));
    xmm3 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2 + pitch2));

    tmp0 = _mm_or_si128(_mm_subs_epu8(xmm0, xmm2), _mm_subs_epu8(xmm2, xmm0));
    tmp1 = _mm_or_si128(_mm_subs_epu8(xmm1, xmm3), _mm_subs_epu8(xmm3, xmm1));

    // luma:
    tmp0 = _mm_and_si128(tmp0, lumaMask); // no need to unpack, we have 00XX after masking
    tmp1 = _mm_and_si128(tmp1, lumaMask);

    tmp0 = _mm_madd_epi16(tmp0, tmp0);
    tmpsum = _mm_add_epi32(tmpsum, tmp0);

    tmp1 = _mm_madd_epi16(tmp1, tmp1);
    tmpsum = _mm_add_epi32(tmpsum, tmp1);

    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;
  }
  // we have only lo64 in tmpsum
  __m128i sum64lo = _mm_unpacklo_epi32(tmpsum, zero); // move to 64 bit boundary
  //__m128i sum64hi = _mm_unpackhi_epi32(tmpsum, zero); 
  tmpsum = sum64lo;

  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  ssd = _mm_cvtsi128_si32(sum);
}


template<int blkSizeY>
void calcSSD_SSE2_16xN(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &ssd)
{
  assert(0 == blkSizeY % 2);

  __m128i tmpsum = _mm_setzero_si128();
  __m128i zero = _mm_setzero_si128();
  // two lines at a time -> 16 = 8x2
  for (int i = 0; i < blkSizeY / 2; i++) {
    __m128i xmm0, xmm1, xmm2, xmm3;
    __m128i tmp0, tmp1, tmp0lo, tmp0hi, tmp1lo, tmp1hi;
    // two lines
    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1));
    xmm2 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2));
    xmm3 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2));

    tmp0 = _mm_or_si128(_mm_subs_epu8(xmm0, xmm2), _mm_subs_epu8(xmm2, xmm0));
    tmp1 = _mm_or_si128(_mm_subs_epu8(xmm1, xmm3), _mm_subs_epu8(xmm3, xmm1));

    tmp0lo = _mm_unpacklo_epi8(tmp0, zero);
    tmp0hi = _mm_unpackhi_epi8(tmp0, zero);
    tmp0lo = _mm_madd_epi16(tmp0lo, tmp0lo);
    tmp0hi = _mm_madd_epi16(tmp0hi, tmp0hi);
    tmpsum = _mm_add_epi32(tmpsum, tmp0lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp0hi);

    tmp1lo = _mm_unpacklo_epi8(tmp1, zero);
    tmp1hi = _mm_unpackhi_epi8(tmp1, zero);
    tmp1lo = _mm_madd_epi16(tmp1lo, tmp1lo);
    tmp1hi = _mm_madd_epi16(tmp1hi, tmp1hi);
    tmpsum = _mm_add_epi32(tmpsum, tmp1lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp1hi);

    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;
  }
  __m128i sum64lo = _mm_unpacklo_epi32(tmpsum, zero); // move to 64 bit boundary
  __m128i sum64hi = _mm_unpackhi_epi32(tmpsum, zero);
  tmpsum = _mm_add_epi64(sum64lo, sum64hi);

  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  ssd = _mm_cvtsi128_si32(sum);
}

// instantiate
template void calcSSD_SSE2_16xN<16>(const uint8_t *ptr1, const uint8_t *ptr2, int pitch1, int pitch2, int &ssd);
template void calcSSD_SSE2_8xN<16>(const uint8_t* ptr1, const uint8_t* ptr2, int pitch1, int pitch2, int& ssd);
template void calcSSD_SSE2_8xN<8>(const uint8_t* ptr1, const uint8_t* ptr2, int pitch1, int pitch2, int& ssd);
template void calcSSD_SSE2_4xN<4>(const uint8_t* ptr1, const uint8_t* ptr2, int pitch1, int pitch2, int& ssd);
template void calcSSD_SSE2_4xN<8>(const uint8_t* ptr1, const uint8_t* ptr2, int pitch1, int pitch2, int& ssd);
template void calcSSD_SSE2_4xN<16>(const uint8_t* ptr1, const uint8_t* ptr2, int pitch1, int pitch2, int& ssd);

// YUY2 16x16 luma+chroma
void calcSSD_SSE2_32x16(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &ssd)
{
  __m128i tmpsum = _mm_setzero_si128();
  __m128i zero = _mm_setzero_si128();
  // unrolled loop 8x2
  for (int i = 0; i < 8; i++) {
    __m128i xmm0, xmm1, xmm2, xmm3;
    __m128i tmp0, tmp1, tmp0lo, tmp0hi, tmp1lo, tmp1hi;
    // unroll#1
    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + 16));
    xmm2 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2));
    xmm3 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + 16));

    tmp0 = _mm_or_si128(_mm_subs_epu8(xmm0, xmm2), _mm_subs_epu8(xmm2, xmm0));
    tmp1 = _mm_or_si128(_mm_subs_epu8(xmm1, xmm3), _mm_subs_epu8(xmm3, xmm1));

    tmp0lo = _mm_unpacklo_epi8(tmp0, zero);
    tmp0hi = _mm_unpackhi_epi8(tmp0, zero);
    tmp0lo = _mm_madd_epi16(tmp0lo, tmp0lo);
    tmp0hi = _mm_madd_epi16(tmp0hi, tmp0hi);
    tmpsum = _mm_add_epi32(tmpsum, tmp0lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp0hi);

    tmp1lo = _mm_unpacklo_epi8(tmp1, zero);
    tmp1hi = _mm_unpackhi_epi8(tmp1, zero);
    tmp1lo = _mm_madd_epi16(tmp1lo, tmp1lo);
    tmp1hi = _mm_madd_epi16(tmp1hi, tmp1hi);
    tmpsum = _mm_add_epi32(tmpsum, tmp1lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp1hi);
    // unroll#2
    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1 + 16));
    xmm2 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2));
    xmm3 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2 + 16));

    tmp0 = _mm_or_si128(_mm_subs_epu8(xmm0, xmm2), _mm_subs_epu8(xmm2, xmm0));
    tmp1 = _mm_or_si128(_mm_subs_epu8(xmm1, xmm3), _mm_subs_epu8(xmm3, xmm1));

    tmp0lo = _mm_unpacklo_epi8(tmp0, zero);
    tmp0hi = _mm_unpackhi_epi8(tmp0, zero);
    tmp0lo = _mm_madd_epi16(tmp0lo, tmp0lo);
    tmp0hi = _mm_madd_epi16(tmp0hi, tmp0hi);
    tmpsum = _mm_add_epi32(tmpsum, tmp0lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp0hi);

    tmp1lo = _mm_unpacklo_epi8(tmp1, zero);
    tmp1hi = _mm_unpackhi_epi8(tmp1, zero);
    tmp1lo = _mm_madd_epi16(tmp1lo, tmp1lo);
    tmp1hi = _mm_madd_epi16(tmp1hi, tmp1hi);
    tmpsum = _mm_add_epi32(tmpsum, tmp1lo);
    tmpsum = _mm_add_epi32(tmpsum, tmp1hi);

    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;
  }
  __m128i sum64lo = _mm_unpacklo_epi32(tmpsum, zero); // move to 64 bit boundary
  __m128i sum64hi = _mm_unpackhi_epi32(tmpsum, zero);
  tmpsum = _mm_add_epi64(sum64lo, sum64hi);

  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  ssd = _mm_cvtsi128_si32(sum);
}

void calcSSD_SSE2_32x16_YUY2_lumaonly(const uint8_t *ptr1, const uint8_t *ptr2,
  int pitch1, int pitch2, int &ssd)
{
  __m128i tmpsum = _mm_setzero_si128();
  __m128i zero = _mm_setzero_si128();
  const __m128i lumaMask = _mm_set1_epi16(0x00FF);
  // unrolled loop 8x2
  for (int i = 0; i < 16/2; i++) {
    __m128i xmm0, xmm1, xmm2, xmm3;
    __m128i tmp0, tmp1;
    // unroll#1
    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + 16));
    xmm2 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2));
    xmm3 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + 16));

    tmp0 = _mm_or_si128(_mm_subs_epu8(xmm0, xmm2), _mm_subs_epu8(xmm2, xmm0));
    tmp1 = _mm_or_si128(_mm_subs_epu8(xmm1, xmm3), _mm_subs_epu8(xmm3, xmm1));

    // luma:
    tmp0 = _mm_and_si128(tmp0, lumaMask); // no need to unpack, we have 00XX after masking
    tmp1 = _mm_and_si128(tmp1, lumaMask);

    tmp0 = _mm_madd_epi16(tmp0, tmp0);
    tmpsum = _mm_add_epi32(tmpsum, tmp0);

    tmp1 = _mm_madd_epi16(tmp1, tmp1);
    tmpsum = _mm_add_epi32(tmpsum, tmp1);
    // unroll#2
    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1 + 16));
    xmm2 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2));
    xmm3 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2 + 16));

    tmp0 = _mm_or_si128(_mm_subs_epu8(xmm0, xmm2), _mm_subs_epu8(xmm2, xmm0));
    tmp1 = _mm_or_si128(_mm_subs_epu8(xmm1, xmm3), _mm_subs_epu8(xmm3, xmm1));

    // luma:
    tmp0 = _mm_and_si128(tmp0, lumaMask);
    tmp1 = _mm_and_si128(tmp1, lumaMask);

    tmp0 = _mm_madd_epi16(tmp0, tmp0);
    tmpsum = _mm_add_epi32(tmpsum, tmp0);

    tmp1 = _mm_madd_epi16(tmp1, tmp1);
    tmpsum = _mm_add_epi32(tmpsum, tmp1);

    ptr1 += pitch1 * 2;
    ptr2 += pitch2 * 2;
  }
  __m128i sum64lo = _mm_unpacklo_epi32(tmpsum, zero); // move to 64 bit boundary
  __m128i sum64hi = _mm_unpackhi_epi32(tmpsum, zero);
  tmpsum = _mm_add_epi64(sum64lo, sum64hi);

  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  ssd = _mm_cvtsi128_si32(sum);
}

// always mod 8, sse2 unaligned!
void HorizontalBlurSSE2_Planar_R(const uint8_t *srcp, uint8_t *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(0x0002); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 8) {
      // we have -1/+1 here, cannot be called for leftmost/rightmost blocks
      __m128i left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 1));
      __m128i center = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i right = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x + 1));
      __m128i left_lo = _mm_unpacklo_epi8(left, zero);
      __m128i center_lo = _mm_unpacklo_epi8(center, zero);
      __m128i right_lo = _mm_unpacklo_epi8(right, zero);
      __m128i left_hi = _mm_unpackhi_epi8(left, zero);
      __m128i center_hi = _mm_unpackhi_epi8(center, zero);
      __m128i right_hi = _mm_unpackhi_epi8(right, zero);

      // (center*2 + left + right + 2) >> 2
      __m128i centermul2_lo = _mm_slli_epi16(center_lo, 1);
      __m128i centermul2_hi = _mm_slli_epi16(center_hi, 1);
      auto res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      auto res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two), 2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two), 2);
      __m128i res = _mm_packus_epi16(res_lo, res_hi);
      _mm_storel_epi64(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}


void HorizontalBlurSSE2_YUY2_R_luma(const uint8_t *srcp, uint8_t *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(0x0002); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 8) {
      // we have -2/+2 here, cannot be called for leftmost/rightmost blocks
      // same as planar case but +/-2 instead of +/-1
      __m128i left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 2));
      __m128i center = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i right = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x + 2));
      __m128i left_lo = _mm_unpacklo_epi8(left, zero);
      __m128i center_lo = _mm_unpacklo_epi8(center, zero);
      __m128i right_lo = _mm_unpacklo_epi8(right, zero);
      __m128i left_hi = _mm_unpackhi_epi8(left, zero);
      __m128i center_hi = _mm_unpackhi_epi8(center, zero);
      __m128i right_hi = _mm_unpackhi_epi8(right, zero);

      // (center*2 + left + right + 2) >> 2
      __m128i centermul2_lo = _mm_slli_epi16(center_lo, 1);
      __m128i centermul2_hi = _mm_slli_epi16(center_hi, 1);
      auto res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      auto res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two), 2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two), 2);
      __m128i res = _mm_packus_epi16(res_lo, res_hi);

      _mm_storel_epi64(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}


// mod 8 always, unaligned
void HorizontalBlurSSE2_YUY2_R(const uint8_t *srcp, uint8_t *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(2); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 8) {
      // luma part
      // we have -2/+2 here, cannot be called for leftmost/rightmost blocks
      // same as Y12 but +/-2 instead of +/-1
      __m128i left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 2));
      __m128i center = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i right = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x + 2));
      __m128i left_lo = _mm_unpacklo_epi8(left, zero);
      __m128i center_lo = _mm_unpacklo_epi8(center, zero);
      __m128i right_lo = _mm_unpacklo_epi8(right, zero);
      __m128i left_hi = _mm_unpackhi_epi8(left, zero);
      __m128i center_hi = _mm_unpackhi_epi8(center, zero);
      __m128i right_hi = _mm_unpackhi_epi8(right, zero);

      // (center*2 + left + right + 2) >> 2
      __m128i centermul2_lo = _mm_slli_epi16(center_lo, 1);
      __m128i centermul2_hi = _mm_slli_epi16(center_hi, 1);
      auto res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      auto res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two), 2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two), 2);
      __m128i res1 = _mm_packus_epi16(res_lo, res_hi);

      // YUY2 chroma part
      // same as Planar but +/-2 instead of +/-1
      // we have already filled center 
      left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 4));
      right = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x + 4));
      left_lo = _mm_unpacklo_epi8(left, zero);
      center_lo = _mm_unpacklo_epi8(center, zero);
      right_lo = _mm_unpacklo_epi8(right, zero);
      left_hi = _mm_unpackhi_epi8(left, zero);
      center_hi = _mm_unpackhi_epi8(center, zero);
      right_hi = _mm_unpackhi_epi8(right, zero);

      // (center*2 + left + right + 2) >> 2
      centermul2_lo = _mm_slli_epi16(center_lo, 1);
      centermul2_hi = _mm_slli_epi16(center_hi, 1);
      res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two), 2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two), 2);
      __m128i res2 = _mm_packus_epi16(res_lo, res_hi);

      __m128i chroma_mask = _mm_set1_epi16((short)0xFF00);
      __m128i luma_mask = _mm_set1_epi16(0x00FF);

      res1 = _mm_and_si128(res1, luma_mask);
      res2 = _mm_and_si128(res1, chroma_mask);
      __m128i res = _mm_or_si128(res1, res2);

      _mm_storel_epi64(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}


void VerticalBlurSSE2_R(const uint8_t *srcp, uint8_t *dstp,
  int src_pitch, int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(0x0002); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      __m128i left = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x - src_pitch));
      __m128i center = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i right = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x + src_pitch));
      __m128i left_lo = _mm_unpacklo_epi8(left, zero);
      __m128i center_lo = _mm_unpacklo_epi8(center, zero);
      __m128i right_lo = _mm_unpacklo_epi8(right, zero);
      __m128i left_hi = _mm_unpackhi_epi8(left, zero);
      __m128i center_hi = _mm_unpackhi_epi8(center, zero);
      __m128i right_hi = _mm_unpackhi_epi8(right, zero);

      // (center*2 + left + right + 2) >> 2
      __m128i centermul2_lo = _mm_slli_epi16(center_lo, 1);
      __m128i centermul2_hi = _mm_slli_epi16(center_hi, 1);
      auto res_lo = _mm_add_epi16(_mm_add_epi16(centermul2_lo, left_lo), right_lo);
      auto res_hi = _mm_add_epi16(_mm_add_epi16(centermul2_hi, left_hi), right_hi);
      res_lo = _mm_srli_epi16(_mm_add_epi16(res_lo, two), 2); // +2, / 4
      res_hi = _mm_srli_epi16(_mm_add_epi16(res_hi, two), 2);
      __m128i res = _mm_packus_epi16(res_lo, res_hi);

      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}


//-------- helpers

// true SAD false SSD
template<bool SAD>
static void calcDiff_SADorSSD_32x32_SSE2(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, const VideoInfo& vi)
{
  int temp1, temp2, y, x, u, difft, box1, box2;
  int widtha, heighta, heights = height, widths = width;
  const uint8_t* ptr1T, * ptr2T;
  if (!vi.IsYUY2())
  {
    // from YV12 to generic planar
    const int xsubsampling = vi.GetPlaneWidthSubsampling(plane);
    const int ysubsampling = vi.GetPlaneHeightSubsampling(plane);
    // base: luma: 16x16, chroma: divided with subsampling
    const int w_to_shift = 4 - xsubsampling;
    const int h_to_shift = 4 - ysubsampling;
    // whole blocks
    heighta = (height >> h_to_shift) << h_to_shift; // mod16 for luma, mod8 or 16 for chroma
    widtha = (width >> w_to_shift) << w_to_shift; // mod16 for luma, mod8 or 16 for chroma
    height >>= h_to_shift; // whole blocks
    width >>= w_to_shift;

    using SAD_fn_t = decltype(calcSAD_SSE2_16xN<16>);
    SAD_fn_t* SAD_fn = nullptr;
    if constexpr (SAD) {
      if (xsubsampling == 0 && ysubsampling == 0) // YV24 or luma
        SAD_fn = calcSAD_SSE2_16xN<16>;
      else if (xsubsampling == 1 && ysubsampling == 0) // YV16
        SAD_fn = calcSAD_SSE2_8xN<16>;
      else if (xsubsampling == 1 && ysubsampling == 1) // YV12
        SAD_fn = calcSAD_SSE2_8xN<8>;
      else if (xsubsampling == 2 && ysubsampling == 0) // YV411
        SAD_fn = calcSAD_SSE2_4xN<16>;
    }
    else {
      if (xsubsampling == 0 && ysubsampling == 0) // YV24 or luma
        SAD_fn = calcSSD_SSE2_16xN<16>;
      else if (xsubsampling == 1 && ysubsampling == 0) // YV16
        SAD_fn = calcSSD_SSE2_8xN<16>;
      else if (xsubsampling == 1 && ysubsampling == 1) // YV12
        SAD_fn = calcSSD_SSE2_8xN<8>;
      else if (xsubsampling == 2 && ysubsampling == 0) // YV411
        SAD_fn = calcSSD_SSE2_4xN<16>;
    }
    // other formats are forbidden and were pre-checked

    // number of whole blocks
    for (y = 0; y < height; ++y)
    {
      // at other places:
      // for (y = 0; y < heighta; y += yhalf)
      //   const int temp1 = (y >> blocky_shift)*xblocks4;
      //   const int temp2 = ((y + blocky_half) >> blocky_shift) * xblocks4;
      // FIXME: why >>1 and +1>>1 here? 
      // Fact 1: y here goes in block-counter mode
      // Fact 2: Because we do 32x32 but with 16x16 luma (and divided chroma) blocks?
      temp1 = (y >> 1) * xblocks4;
      temp2 = ((y + 1) >> 1) * xblocks4;
      for (x = 0; x < width; ++x) // width is the number of blocks
      {
        SAD_fn(ptr1 + (x << w_to_shift), ptr2 + (x << w_to_shift), pitch1, pitch2, difft);
        box1 = (x >> 1) << 2;
        box2 = ((x + 1) >> 1) << 2;
        diff[temp1 + box1 + 0] += difft;
        diff[temp1 + box2 + 1] += difft;
        diff[temp2 + box1 + 2] += difft;
        diff[temp2 + box2 + 3] += difft;
      }
      // rest non-simd
      for (x = widtha; x < widths; ++x)
      {
        ptr1T = ptr1;
        ptr2T = ptr2;
        for (difft = 0, u = 0; u < (1 << w_to_shift); ++u) // 16 or 8. u<blocksize
        {
          if constexpr (SAD)
            difft += abs(ptr1T[x] - ptr2T[x]);
          else
            difft += (ptr1T[x] - ptr2T[x]) * (ptr1T[x] - ptr2T[x]);
          ptr1T += pitch1;
          ptr2T += pitch2;
        }
        box1 = (x >> (w_to_shift + 1)) << 2;
        box2 = ((x + (1 << w_to_shift)) >> (w_to_shift + 1)) << 2;
        diff[temp1 + box1 + 0] += difft;
        diff[temp1 + box2 + 1] += difft;
        diff[temp2 + box1 + 2] += difft;
        diff[temp2 + box2 + 3] += difft;
      }
      // += pitch1 * vertical blocksize
      ptr1 += pitch1 << h_to_shift;
      ptr2 += pitch2 << h_to_shift;
    }
    for (y = heighta; y < heights; ++y)
    {
      temp1 = (y >> (h_to_shift + 1)) * xblocks4; // y >> 5 or 4
      temp2 = ((y + (1 << h_to_shift)) >> (h_to_shift + 1)) * xblocks4;
      for (x = 0; x < widths; ++x)
      {
        if constexpr (SAD)
          difft = abs(ptr1[x] - ptr2[x]);
        else {
          difft = ptr1[x] - ptr2[x];
          difft *= difft;
        }
        box1 = (x >> (w_to_shift + 1)) << 2;
        box2 = ((x + (1 << w_to_shift)) >> (w_to_shift + 1)) << 2;
        diff[temp1 + box1 + 0] += difft;
        diff[temp1 + box2 + 1] += difft;
        diff[temp2 + box1 + 2] += difft;
        diff[temp2 + box2 + 3] += difft;
      }
      ptr1 += pitch1;
      ptr2 += pitch2;
    }
    // planar (was: YV12)
  }
  else // YUY2
  {
    heighta = (height >> 4) << 4;
    widtha = (width >> 5) << 5;
    height >>= 4;
    width >>= 5;
    if (chroma)
    {
      // YUY2 common luma chroma
      for (y = 0; y < height; ++y)
      {
        temp1 = (y >> 1) * xblocks4;
        temp2 = ((y + 1) >> 1) * xblocks4;
        for (x = 0; x < width; ++x)
        {
          if constexpr (SAD)
            calcSAD_SSE2_32x16(ptr1 + (x << 5), ptr2 + (x << 5), pitch1, pitch2, difft);
          else
            calcSSD_SSE2_32x16(ptr1 + (x << 5), ptr2 + (x << 5), pitch1, pitch2, difft);
          box1 = (x >> 1) << 2;
          box2 = ((x + 1) >> 1) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        for (x = widtha; x < widths; ++x)
        {
          ptr1T = ptr1;
          ptr2T = ptr2;
          for (difft = 0, u = 0; u < 16; ++u)
          {
            if constexpr (SAD)
              difft += abs(ptr1T[x] - ptr2T[x]);
            else
              difft += (ptr1T[x] - ptr2T[x]) * (ptr1T[x] - ptr2T[x]);
            ptr1T += pitch1;
            ptr2T += pitch2;
          }
          box1 = (x >> 6) << 2;
          box2 = ((x + 32) >> 6) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        // 16 vertical lines at a time
        ptr1 += pitch1 << 4;
        ptr2 += pitch2 << 4;
      }
      for (y = heighta; y < heights; ++y)
      {
        temp1 = (y >> 5) * xblocks4;
        temp2 = ((y + 16) >> 5) * xblocks4;
        for (x = 0; x < widths; ++x)
        {
          if constexpr (SAD)
            difft = abs(ptr1[x] - ptr2[x]);
          else {
            difft = ptr1[x] - ptr2[x];
            difft *= difft;
          }
          box1 = (x >> 6) << 2;
          box2 = ((x + 32) >> 6) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        ptr1 += pitch1;
        ptr2 += pitch2;
      }
    }
    else
    {
      for (y = 0; y < height; ++y)
      {
        temp1 = (y >> 1) * xblocks4;
        temp2 = ((y + 1) >> 1) * xblocks4;
        for (x = 0; x < width; ++x)
        {
          if constexpr (SAD)
            calcSAD_SSE2_32x16_YUY2_lumaonly(ptr1 + (x << 5), ptr2 + (x << 5), pitch1, pitch2, difft);
          else
            calcSSD_SSE2_32x16_YUY2_lumaonly(ptr1 + (x << 5), ptr2 + (x << 5), pitch1, pitch2, difft);
          box1 = (x >> 1) << 2;
          box2 = ((x + 1) >> 1) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        for (x = widtha; x < widths; x += 2)
        {
          ptr1T = ptr1;
          ptr2T = ptr2;
          for (difft = 0, u = 0; u < 16; ++u)
          {
            if constexpr (SAD)
              difft += abs(ptr1T[x] - ptr2T[x]);
            else
              difft += (ptr1T[x] - ptr2T[x]) * (ptr1T[x] - ptr2T[x]);
            ptr1T += pitch1;
            ptr2T += pitch2;
          }
          box1 = (x >> 6) << 2;
          box2 = ((x + 32) >> 6) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        // 16 vertical lines at a time
        ptr1 += pitch1 << 4;
        ptr2 += pitch2 << 4;
      }
      for (y = heighta; y < heights; ++y)
      {
        temp1 = (y >> 5) * xblocks4;
        temp2 = ((y + 16) >> 5) * xblocks4;
        for (x = 0; x < widths; x += 2)
        {
          if constexpr (SAD)
            difft = abs(ptr1[x] - ptr2[x]);
          else {
            difft = ptr1[x] - ptr2[x];
            difft *= difft;
          }
          box1 = (x >> 6) << 2;
          box2 = ((x + 32) >> 6) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        ptr1 += pitch1;
        ptr2 += pitch2;
      }
    }
  }
}

void calcDiffSAD_32x32_SSE2(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, const VideoInfo& vi)
{
  calcDiff_SADorSSD_32x32_SSE2<true>(ptr1, ptr2, pitch1, pitch2, width, height, plane, xblocks4, diff, chroma, vi);
}

void calcDiffSSD_32x32_SSE2(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, const VideoInfo& vi)
{
  calcDiff_SADorSSD_32x32_SSE2<false>(ptr1, ptr2, pitch1, pitch2, width, height, plane, xblocks4, diff, chroma, vi);
}


// true: SAD, false: SSD
template<bool SAD>
void calcDiff_SADorSSD_Generic_SSE2(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, const VideoInfo& vi)
{
  int temp1, temp2, y, x, u, difft, box1, box2;
  int yshift, yhalf, xshift, xhalf;
  int heighta, heights = height, widtha, widths = width;
  int yshifta, yhalfa, xshifta, xhalfa;
  const uint8_t* ptr1T, * ptr2T;
  if (!vi.IsYUY2()) // YV12, YV16, YV24
  {
    // from YV12 to generic planar
    const int xsubsampling = vi.GetPlaneWidthSubsampling(plane);
    const int ysubsampling = vi.GetPlaneHeightSubsampling(plane);
    // base: luma: 8x8, chroma: divided with subsampling
    const int w_to_shift = 3 - xsubsampling;
    const int h_to_shift = 3 - ysubsampling;
    // whole blocks
    heighta = (height >> h_to_shift) << h_to_shift; // mod16 for luma, mod8 or 4 for chroma
    widtha = (width >> w_to_shift) << w_to_shift; // mod16 for luma, mod8 or 4 for chroma
    height >>= h_to_shift;
    width >>= w_to_shift;

    using SAD_fn_t = decltype(calcSAD_SSE2_16xN<16>); // similar prototype for all X*Y
    SAD_fn_t* SAD_fn = nullptr;
    if constexpr (SAD) {
      if (xsubsampling == 0 && ysubsampling == 0) // YV24 or luma
        SAD_fn = calcSAD_SSE2_8xN<8>;
      else if (xsubsampling == 1 && ysubsampling == 0) // YV16
        SAD_fn = calcSAD_SSE2_4xN<8>;
      else if (xsubsampling == 1 && ysubsampling == 1) // YV12
        SAD_fn = calcSAD_SSE2_4xN<4>;
      else if (xsubsampling == 2 && ysubsampling == 0) // YV411
        SAD_fn = calcSAD_C_2xN<8>;
    }
    else {
      if (xsubsampling == 0 && ysubsampling == 0) // YV24 or luma
        SAD_fn = calcSSD_SSE2_8xN<8>;
      else if (xsubsampling == 1 && ysubsampling == 0) // YV16
        SAD_fn = calcSSD_SSE2_4xN<8>;
      else if (xsubsampling == 1 && ysubsampling == 1) // YV12
        SAD_fn = calcSSD_SSE2_4xN<4>;
      else if (xsubsampling == 2 && ysubsampling == 0) // YV411
        SAD_fn = calcSSD_C_2xN<8>;
    }
    // other formats are forbidden and were pre-checked

    yshifta = yshiftS - ysubsampling; // yshiftS  or yshiftS - 1
    yhalfa = yhalfS >> ysubsampling; // yhalfS  or yhalfS >> 1;
    xshifta = xshiftS - xsubsampling; //  xshiftS or  xshiftS - 1;
    xhalfa = xhalfS >> xsubsampling; // xhalfS  or xhalfS >> 1;
    // these are the same for luma and chroma as well, 8x8
    // FIXME: check, really? Really.
    yshift = yshiftS - 3;
    yhalf = yhalfS >> 3; // div 8
    xshift = xshiftS - 3;
    xhalf = xhalfS >> 3;
    for (y = 0; y < height; ++y)
    {
      temp1 = (y >> yshift) * xblocks4;
      temp2 = ((y + yhalf) >> yshift) * xblocks4;
      for (x = 0; x < width; ++x)
      {
        SAD_fn(ptr1 + (x << w_to_shift), ptr2 + (x << w_to_shift), pitch1, pitch2, difft);
        box1 = (x >> xshift) << 2;
        box2 = ((x + xhalf) >> xshift) << 2;
        diff[temp1 + box1 + 0] += difft;
        diff[temp1 + box2 + 1] += difft;
        diff[temp2 + box1 + 2] += difft;
        diff[temp2 + box2 + 3] += difft;
      }
      for (x = widtha; x < widths; ++x)
      {
        ptr1T = ptr1;
        ptr2T = ptr2;
        for (difft = 0, u = 0; u < (1 << w_to_shift); ++u) // u < 8 or 4
        {
          if constexpr (SAD)
            difft += abs(ptr1T[x] - ptr2T[x]);
          else
            difft += (ptr1T[x] - ptr2T[x]) * (ptr1T[x] - ptr2T[x]);
          ptr1T += pitch1;
          ptr2T += pitch2;
        }
        box1 = (x >> xshifta) << 2;
        box2 = ((x + xhalfa) >> xshifta) << 2;
        diff[temp1 + box1 + 0] += difft;
        diff[temp1 + box2 + 1] += difft;
        diff[temp2 + box1 + 2] += difft;
        diff[temp2 + box2 + 3] += difft;
      }
      ptr1 += pitch1 << h_to_shift;
      ptr2 += pitch2 << h_to_shift;
    }
    for (y = heighta; y < heights; ++y)
    {
      temp1 = (y >> yshifta) * xblocks4;
      temp2 = ((y + yhalfa) >> yshifta) * xblocks4;
      for (x = 0; x < widths; ++x)
      {
        if constexpr (SAD)
          difft = abs(ptr1[x] - ptr2[x]);
        else {
          difft = ptr1[x] - ptr2[x];
          difft *= difft;
        }
        box1 = (x >> xshifta) << 2;
        box2 = ((x + xhalfa) >> xshifta) << 2;
        diff[temp1 + box1 + 0] += difft;
        diff[temp1 + box2 + 1] += difft;
        diff[temp2 + box1 + 2] += difft;
        diff[temp2 + box2 + 3] += difft;
      }
      ptr1 += pitch1;
      ptr2 += pitch2;
    }
    // end of YV12 / planar
  }
  else // YUY2
  {
    heighta = (height >> 3) << 3;
    widtha = (width >> 3) << 3;
    height >>= 3;
    width >>= 3;
    yshifta = yshiftS;
    yhalfa = yhalfS;
    xshifta = xshiftS + 1;
    xhalfa = xhalfS << 1;
    yshift = yshiftS - 3;
    yhalf = yhalfS >> 3;
    xshift = xshiftS - 2;
    xhalf = xhalfS >> 2;
    if (chroma) // luma-chroma together
    {
      for (y = 0; y < height; ++y)
      {
        temp1 = (y >> yshift) * xblocks4;
        temp2 = ((y + yhalf) >> yshift) * xblocks4;
        for (x = 0; x < width; ++x)
        {
          if constexpr (SAD)
            calcSAD_SSE2_8xN<8>(ptr1 + (x << 3), ptr2 + (x << 3), pitch1, pitch2, difft);
          else
            calcSSD_SSE2_8xN<8>(ptr1 + (x << 3), ptr2 + (x << 3), pitch1, pitch2, difft);

          box1 = (x >> xshift) << 2;
          box2 = ((x + xhalf) >> xshift) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        for (x = widtha; x < widths; ++x)
        {
          ptr1T = ptr1;
          ptr2T = ptr2;
          for (difft = 0, u = 0; u < 8; ++u)
          {
            if constexpr (SAD)
              difft += abs(ptr1T[x] - ptr2T[x]);
            else
              difft += (ptr1T[x] - ptr2T[x]) * (ptr1T[x] - ptr2T[x]);
            ptr1T += pitch1;
            ptr2T += pitch2;
          }
          box1 = (x >> xshifta) << 2;
          box2 = ((x + xhalfa) >> xshifta) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        // 8 lines
        ptr1 += pitch1 << 3;
        ptr2 += pitch2 << 3;
      }
      for (y = heighta; y < heights; ++y)
      {
        temp1 = (y >> yshifta) * xblocks4;
        temp2 = ((y + yhalfa) >> yshifta) * xblocks4;
        for (x = 0; x < widths; ++x)
        {
          if constexpr (SAD)
            difft = abs(ptr1[x] - ptr2[x]);
          else {
            difft = ptr1[x] - ptr2[x];
            difft *= difft;
          }
          box1 = (x >> xshifta) << 2;
          box2 = ((x + xhalfa) >> xshifta) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        ptr1 += pitch1;
        ptr2 += pitch2;
      }
    }
    else
    {
      // YUY2 luma only: chroma is masked out. Function names ending with _Luma do this.
      for (y = 0; y < height; ++y)
      {
        temp1 = (y >> yshift) * xblocks4;
        temp2 = ((y + yhalf) >> yshift) * xblocks4;
        for (x = 0; x < width; ++x)
        {
          if constexpr (SAD)
            calcSAD_SSE2_8x8_YUY2_lumaonly(ptr1 + (x << 3), ptr2 + (x << 3), pitch1, pitch2, difft);
          else
            calcSSD_SSE2_8x8_YUY2_lumaonly(ptr1 + (x << 3), ptr2 + (x << 3), pitch1, pitch2, difft);


          box1 = (x >> xshift) << 2;
          box2 = ((x + xhalf) >> xshift) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        for (x = widtha; x < widths; x += 2)
        {
          ptr1T = ptr1;
          ptr2T = ptr2;
          for (difft = 0, u = 0; u < 8; ++u)
          {
            if constexpr (SAD)
              difft += abs(ptr1T[x] - ptr2T[x]);
            else
              difft += (ptr1T[x] - ptr2T[x]) * (ptr1T[x] - ptr2T[x]);

            ptr1T += pitch1;
            ptr2T += pitch2;
          }
          box1 = (x >> xshifta) << 2;
          box2 = ((x + xhalfa) >> xshifta) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        // 8 lines
        ptr1 += pitch1 << 3;
        ptr2 += pitch2 << 3;
      }
      for (y = heighta; y < heights; ++y)
      {
        temp1 = (y >> yshifta) * xblocks4;
        temp2 = ((y + yhalfa) >> yshifta) * xblocks4;
        for (x = 0; x < widths; x += 2)
        {
          if constexpr (SAD)
            difft = abs(ptr1[x] - ptr2[x]);
          else {
            difft = ptr1[x] - ptr2[x];
            difft *= difft;
          }

          box1 = (x >> xshifta) << 2;
          box2 = ((x + xhalfa) >> xshifta) << 2;
          diff[temp1 + box1 + 0] += difft;
          diff[temp1 + box2 + 1] += difft;
          diff[temp2 + box1 + 2] += difft;
          diff[temp2 + box2 + 3] += difft;
        }
        ptr1 += pitch1;
        ptr2 += pitch2;
      }
    }
  }
}

void calcDiffSAD_Generic_SSE2(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, const VideoInfo& vi)
{
  calcDiff_SADorSSD_Generic_SSE2<true>(ptr1, ptr2, pitch1, pitch2, width, height, plane, xblocks4, diff, chroma, xshiftS, yshiftS, xhalfS, yhalfS, vi);
}

void calcDiffSSD_Generic_SSE2(const uint8_t* ptr1, const uint8_t* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, const VideoInfo& vi)
{
  calcDiff_SADorSSD_Generic_SSE2<false>(ptr1, ptr2, pitch1, pitch2, width, height, plane, xblocks4, diff, chroma, xshiftS, yshiftS, xhalfS, yhalfS, vi);
}


// true: SAD, false: SSD
// inc: YUY2 increment
template<typename pixel_t, bool SAD, int inc>
void calcDiff_SADorSSD_Generic_c(const pixel_t* prvp, const pixel_t* curp,
  int prv_pitch, int cur_pitch, int width, int height, int plane, int xblocks4, uint64_t* diff,
  bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, int nt,
  const VideoInfo& vi)
{
  int temp1, temp2, u;

  // 16 bits SSD requires int64 intermediate
  typedef typename std::conditional<sizeof(pixel_t) == 1 && !SAD, int, int64_t> ::type safeint_t;

  safeint_t difft; // int or 64 bits
  int diffs; // pixel differences are internally scaled back to 8 bit range to avoid overflow
  int box1, box2;
  int yshift, yhalf, xshift, xhalf;
  int heighta, widtha;
  const pixel_t* prvpT, * curpT;

  const int bits_per_pixel = vi.BitsPerComponent();
  const int shift_count = SAD ? (bits_per_pixel - 8) : 2 * (bits_per_pixel - 8);

  if (!vi.IsYUY2())
  {
    const int ysubsampling = vi.GetPlaneHeightSubsampling(plane);
    const int xsubsampling = vi.GetPlaneWidthSubsampling(plane);
    yshift = yshiftS - ysubsampling;
    yhalf = yhalfS >> ysubsampling;
    xshift = xshiftS - xsubsampling;
    xhalf = xhalfS >> xsubsampling;
  }
  else {
    // YUY2
    yshift = yshiftS;
    yhalf = yhalfS;
    xshift = xshiftS + 1;
    xhalf = xhalfS << 1;
  }

  heighta = (height >> (yshift - 1)) << (yshift - 1);
  widtha = (width >> (xshift - 1)) << (xshift - 1);
  // whole blocks
  for (int y = 0; y < heighta; y += yhalf)
  {
    temp1 = (y >> yshift) * xblocks4;
    temp2 = ((y + yhalf) >> yshift) * xblocks4;
    for (int x = 0; x < widtha; x += xhalf)
    {
      prvpT = prvp;
      curpT = curp;
      for (diffs = 0, u = 0; u < yhalf; ++u)
      {
        for (int v = 0; v < xhalf; v += inc)
        {
          if constexpr (SAD) {
            difft = abs(prvpT[x + v] - curpT[x + v]);
          }
          else {
            difft = prvpT[x + v] - curpT[x + v];
            difft *= difft;
          }
          if constexpr (sizeof(pixel_t) == 2) difft >>= shift_count; // back to 8 bit range

          if (difft > nt) diffs += static_cast<int>(difft);
        }
        prvpT += prv_pitch;
        curpT += cur_pitch;
      }
      if (diffs > nt)
      {
        box1 = (x >> xshift) << 2;
        box2 = ((x + xhalf) >> xshift) << 2;
        diff[temp1 + box1 + 0] += diffs;
        diff[temp1 + box2 + 1] += diffs;
        diff[temp2 + box1 + 2] += diffs;
        diff[temp2 + box2 + 3] += diffs;
      }
    }
    // rest non - whole block on the right
    for (int x = widtha; x < width; x += inc)
    {
      prvpT = prvp;
      curpT = curp;
      for (diffs = 0, u = 0; u < yhalf; ++u)
      {
        if constexpr (SAD) {
          difft = abs(prvpT[x] - curpT[x]);
        }
        else {
          difft = prvpT[x] - curpT[x];
          difft *= difft;
        }
        if constexpr (sizeof(pixel_t) == 2) difft >>= shift_count; // back to 8 bit range
        if (difft > nt) diffs += static_cast<int>(difft);
        prvpT += prv_pitch;
        curpT += cur_pitch;
      }
      if (diffs > nt)
      {
        box1 = (x >> xshift) << 2;
        box2 = ((x + xhalf) >> xshift) << 2;
        diff[temp1 + box1 + 0] += diffs;
        diff[temp1 + box2 + 1] += diffs;
        diff[temp2 + box1 + 2] += diffs;
        diff[temp2 + box2 + 3] += diffs;
      }
    }
    prvp += prv_pitch * yhalf;
    curp += cur_pitch * yhalf;
  }
  // rest non-whole block at the bottom
  for (int y = heighta; y < height; ++y)
  {
    temp1 = (y >> yshift) * xblocks4;
    temp2 = ((y + yhalf) >> yshift) * xblocks4;
    for (int x = 0; x < width; x += inc)
    {
      if constexpr (SAD) {
        difft = abs(prvp[x] - curp[x]);
      }
      else {
        difft = prvp[x] - curp[x];
        difft *= difft;
      }
      if constexpr (sizeof(pixel_t) == 2) difft >>= shift_count; // back to 8 bit range
      if (difft > nt)
      {
        box1 = (x >> xshift) << 2;
        box2 = ((x + xhalf) >> xshift) << 2;
        diff[temp1 + box1 + 0] += difft;
        diff[temp1 + box2 + 1] += difft;
        diff[temp2 + box1 + 2] += difft;
        diff[temp2 + box2 + 3] += difft;
      }
    }
    prvp += prv_pitch;
    curp += cur_pitch;
  }
}

// instantiate
template void calcDiff_SADorSSD_Generic_c<uint8_t, false, 1>(const uint8_t* prvp, const uint8_t* curp,
  int prv_pitch, int cur_pitch, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, int nt, const VideoInfo& vi);
template void calcDiff_SADorSSD_Generic_c<uint8_t, false, 2>(const uint8_t* prvp, const uint8_t* curp,
  int prv_pitch, int cur_pitch, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, int nt, const VideoInfo& vi);
template void calcDiff_SADorSSD_Generic_c<uint8_t, true, 1>(const uint8_t* prvp, const uint8_t* curp,
  int prv_pitch, int cur_pitch, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, int nt, const VideoInfo& vi);
template void calcDiff_SADorSSD_Generic_c<uint8_t, true, 2>(const uint8_t* prvp, const uint8_t* curp,
  int prv_pitch, int cur_pitch, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, int nt, const VideoInfo& vi);

template void calcDiff_SADorSSD_Generic_c<uint16_t, false, 1>(const uint16_t* prvp, const uint16_t* curp,
  int prv_pitch, int cur_pitch, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, int nt, const VideoInfo& vi);
template void calcDiff_SADorSSD_Generic_c<uint16_t, true, 1>(const uint16_t* prvp, const uint16_t* curp,
  int prv_pitch, int cur_pitch, int width, int height, int plane, int xblocks4, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, int nt, const VideoInfo& vi);


