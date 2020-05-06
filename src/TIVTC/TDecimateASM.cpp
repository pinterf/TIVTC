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

#include "../TIVTC/TDecimate.h"
#include "TDecimateASM.h"

// Leak's sse2 blend routine
void blend_SSE2_16(unsigned char* dstp, const unsigned char* srcp,
  const unsigned char* nxtp, int width, int height, int dst_pitch,
  int src_pitch, int nxt_pitch, double w1, double w2)
{
  __m128i iw1 = _mm_set1_epi16((int)(w1*65536.0));
  __m128i iw2 = _mm_set1_epi16((int)(w2*65536.0));
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      __m128i src2 = _mm_load_si128(reinterpret_cast<const __m128i *>(nxtp + x));
      __m128i src1_lo = _mm_unpacklo_epi8(src1, src1);
      __m128i src2_lo = _mm_unpacklo_epi8(src2, src2);
      __m128i src1_hi = _mm_unpackhi_epi8(src1, src1);
      __m128i src2_hi = _mm_unpackhi_epi8(src2, src2);
      // pmulhuw -> _mm_mulhi_epu16
      // paddusw -> _mm_adds_epu16
      __m128i mulres_lo = _mm_adds_epu16(_mm_mulhi_epu16(src1_lo, iw1), _mm_mulhi_epu16(src2_lo, iw2));
      __m128i mulres_hi = _mm_adds_epu16(_mm_mulhi_epu16(src1_hi, iw1), _mm_mulhi_epu16(src2_hi, iw2));

      mulres_lo = _mm_srli_epi16(mulres_lo, 8); // psrlw xmm0, 8
      mulres_hi = _mm_srli_epi16(mulres_hi, 8);

      __m128i res = _mm_packus_epi16(mulres_lo, mulres_hi); // packuswb xmm0, xmm2
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    dstp += dst_pitch;
    srcp += src_pitch;
    nxtp += nxt_pitch;
  }
}

// fast blend routine for 50:50 case
void blend_SSE2_5050(unsigned char* dstp, const unsigned char* srcp,
  const unsigned char* nxtp, int width, int height, int dst_pitch,
  int src_pitch, int nxt_pitch)
{
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      __m128i src2 = _mm_load_si128(reinterpret_cast<const __m128i *>(nxtp + x));
      __m128i res = _mm_avg_epu8(src1, src2); 
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    dstp += dst_pitch;
    srcp += src_pitch;
    nxtp += nxt_pitch;
  }
}

void calcLumaDiffYUY2SAD_SSE2_16(const unsigned char *prvp, const unsigned char *nxtp,
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


void calcLumaDiffYUY2SSD_SSE2_16(const unsigned char *prvp, const unsigned char *nxtp,
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
void calcSAD_SSE2_16xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &sad)
{
    __m128i tmpsum = _mm_setzero_si128();
    // unrolled loop
    for (int i = 0; i < blkSizeY / 8; i++) {
      __m128i xmm0, xmm1;
      xmm0 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1));
      xmm1 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1));
      xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2)));
      xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2)));
      ptr1 += pitch1 * 2;
      __m128i tmp1 = _mm_add_epi32(xmm0, xmm1);
      ptr2 += pitch2 * 2;

      xmm0 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1));
      xmm1 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1));
      xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2)));
      xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2)));
      ptr1 += pitch1 * 2;
      __m128i tmp2 = _mm_add_epi32(xmm0, xmm1);
      ptr2 += pitch2 * 2;

      xmm0 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1));
      xmm1 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1));
      xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2)));
      xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2)));
      ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2]
      __m128i tmp3 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
      ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2]

      xmm0 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1));
      xmm1 = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr1 + pitch1));
      xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2)));
      xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i *>(ptr2 + pitch2)));
      ptr1 += pitch1 * 2;
      __m128i tmp4 = _mm_add_epi32(xmm0, xmm1);
      ptr2 += pitch2 * 2;

      xmm0 = _mm_add_epi32(tmp1, tmp2);
      xmm1 = _mm_add_epi32(tmp3, tmp4);
      tmpsum = _mm_add_epi32(tmpsum, xmm0);
      tmpsum = _mm_add_epi32(tmpsum, xmm1);
    }
    __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
    sad = _mm_cvtsi128_si32(sum);
}


// new
template<int blkSizeY>
void calcSAD_SSE2_4xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &sad)
{
  __m128i tmpsum = _mm_setzero_si128();
  // unrolled loop
  for (int i = 0; i < blkSizeY / 4; i++) {
    __m128i xmm0, xmm1;
    xmm0 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr1)));
    xmm1 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr1 + pitch1)));
    xmm0 = _mm_sad_epu8(xmm0, _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr2))));
    xmm1 = _mm_sad_epu8(xmm1, _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr2 + pitch2))));
    ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2]
    __m128i tmp1 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
    ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2]

    xmm0 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr1)));
    xmm1 = _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr1 + pitch1)));
    xmm0 = _mm_sad_epu8(xmm0, _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr2))));
    xmm1 = _mm_sad_epu8(xmm1, _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(ptr2 + pitch2))));
    ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2]
    __m128i tmp2 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
    ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2]

    xmm0 = _mm_add_epi32(tmp1, tmp2);
    tmpsum = _mm_add_epi32(tmpsum, xmm0);
  }

  sad = _mm_cvtsi128_si32(tmpsum); // we have only lo
}

template<int blkSizeY>
void calcSAD_SSE2_8xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &sad)
{
  __m128i tmpsum = _mm_setzero_si128();
  // blkSizeY should be multiple of 8
  // unrolled loop
  for (int i = 0; i < blkSizeY / 8; i++) {
    __m128i xmm0, xmm1;
    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2]
    __m128i tmp1 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
    ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2]

    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2]
    __m128i tmp2 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
    ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2]

    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2]
    __m128i tmp3 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
    ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2]

    xmm0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm0 = _mm_sad_epu8(xmm0, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    // ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2] // no need more 
    __m128i tmp4 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
    ptr2 += pitch2 * 2;

    xmm0 = _mm_add_epi32(tmp1, tmp2);
    xmm1 = _mm_add_epi32(tmp3, tmp4);
    tmpsum = _mm_add_epi32(tmpsum, xmm0);
    tmpsum = _mm_add_epi32(tmpsum, xmm1);
  }

  sad = _mm_cvtsi128_si32(tmpsum); // we have only lo
}

// new
void calcSAD_SSE2_8x8_YUY2_lumaonly(const unsigned char *ptr1, const unsigned char *ptr2,
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
  ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2]
  __m128i tmp1 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
  ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2]

  xmm0 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1)), lumaMask);
  xmm1 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1 + pitch1)), lumaMask);
  xmm0 = _mm_sad_epu8(xmm0, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2)), lumaMask));
  xmm1 = _mm_sad_epu8(xmm1, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2 + pitch2)), lumaMask));
  ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2]
  __m128i tmp2 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
  ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2]

  xmm0 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1)), lumaMask);
  xmm1 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1 + pitch1)), lumaMask);
  xmm0 = _mm_sad_epu8(xmm0, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2)), lumaMask));
  xmm1 = _mm_sad_epu8(xmm1, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2 + pitch2)), lumaMask));
  ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2]
  __m128i tmp3 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
  ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2]

  xmm0 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1)), lumaMask);
  xmm1 = _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr1 + pitch1)), lumaMask);
  xmm0 = _mm_sad_epu8(xmm0, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2)), lumaMask));
  xmm1 = _mm_sad_epu8(xmm1, _mm_and_si128(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(ptr2 + pitch2)), lumaMask));
  // ptr1 += pitch1 * 2; // lea edi, [edi + edx * 2] // no need more 
  __m128i tmp4 = _mm_add_epi32(xmm0, xmm1); // paddd xmm0, xmm1
  // ptr2 += pitch2 * 2; //lea esi, [esi + ecx * 2] // no need more 

  xmm0 = _mm_add_epi32(tmp1, tmp2);
  xmm1 = _mm_add_epi32(tmp3, tmp4);
  tmpsum = _mm_add_epi32(tmpsum, xmm0);
  tmpsum = _mm_add_epi32(tmpsum, xmm1);

  sad = _mm_cvtsi128_si32(tmpsum); // we have only lo
}

// really YUY2 16x16 with chroma
void calcSAD_SSE2_32x16(const unsigned char* ptr1, const unsigned char* ptr2,
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

    ptr1 += pitch1 * 2;
    __m128i tmp1 = _mm_add_epi32(xmm0, xmm1);
    __m128i tmp2 = _mm_add_epi32(xmm2, xmm3);
    ptr2 += pitch2 * 2;

    xmm0 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1));
    xmm1 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + 16));
    xmm2 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1));
    xmm3 = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr1 + pitch1 + 16));
    xmm0 = _mm_sad_epu8(xmm0, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2)));
    xmm1 = _mm_sad_epu8(xmm1, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + 16)));
    xmm2 = _mm_sad_epu8(xmm2, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2)));
    xmm3 = _mm_sad_epu8(xmm3, _mm_load_si128(reinterpret_cast<const __m128i*>(ptr2 + pitch2 + 16)));

    ptr1 += pitch1 * 2;
    __m128i tmp3 = _mm_add_epi32(xmm0, xmm1);
    __m128i tmp4 = _mm_add_epi32(xmm2, xmm3);
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
void calcSAD_SSE2_32x16_YUY2_lumaonly(const unsigned char *ptr1, const unsigned char *ptr2,
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

    ptr1 += pitch1 * 2;
    __m128i tmp1 = _mm_add_epi32(xmm0, xmm1);
    __m128i tmp2 = _mm_add_epi32(xmm2, xmm3);
    ptr2 += pitch2 * 2;

    xmm0 = _mm_add_epi32(tmp1, tmp2);
    tmpsum = _mm_add_epi32(tmpsum, xmm0);
  }
  __m128i sum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // add lo, hi
  sad = _mm_cvtsi128_si32(sum);
}

template<int blkSizeY>
void calcSSD_SSE2_4xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &ssd)
{
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
void calcSSD_SSE2_8xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &ssd)
{
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

void calcSSD_SSE2_8x8_YUY2_lumaonly(const unsigned char *ptr1, const unsigned char *ptr2,
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
void calcSSD_SSE2_16xN(const unsigned char *ptr1, const unsigned char *ptr2,
  int pitch1, int pitch2, int &ssd)
{
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
template void calcSSD_SSE2_16xN<16>(const unsigned char *ptr1, const unsigned char *ptr2, int pitch1, int pitch2, int &ssd);
template void calcSSD_SSE2_8xN<16>(const unsigned char* ptr1, const unsigned char* ptr2, int pitch1, int pitch2, int& ssd);
template void calcSSD_SSE2_8xN<8>(const unsigned char* ptr1, const unsigned char* ptr2, int pitch1, int pitch2, int& ssd);
template void calcSSD_SSE2_4xN<4>(const unsigned char* ptr1, const unsigned char* ptr2, int pitch1, int pitch2, int& ssd);
template void calcSSD_SSE2_4xN<8>(const unsigned char* ptr1, const unsigned char* ptr2, int pitch1, int pitch2, int& ssd);

void calcSSD_SSE2_32x16(const unsigned char *ptr1, const unsigned char *ptr2,
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

void calcSSD_SSE2_32x16_YUY2_lumaonly(const unsigned char *ptr1, const unsigned char *ptr2,
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
void HorizontalBlurSSE2_Planar_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(0x0002); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 8) {
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


void HorizontalBlurSSE2_YUY2_R_luma(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(0x0002); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 8) {
      __m128i left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 2)); // same as Y12 but +/-2 instead of +/-1
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
void HorizontalBlurSSE2_YUY2_R(const unsigned char *srcp, unsigned char *dstp, int src_pitch,
  int dst_pitch, int width, int height)
{
  __m128i two = _mm_set1_epi16(2); // rounder
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 8) {
      // luma part
      __m128i left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 2)); // same as Y12 but +/-2 instead of +/-1
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

      // chroma part
      left = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp + x - 4)); // same as Y12 but +/-2 instead of +/-1
                                                                              // we have already filler center 
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


void VerticalBlurSSE2_R(const unsigned char *srcp, unsigned char *dstp,
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
void calcDiff_SADorSSD_32x32_SSE2(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t* diff, bool chroma, const VideoInfo& vi)
{
  int temp1, temp2, y, x, u, difft, box1, box2;
  int widtha, heighta, heights = height, widths = width;
  const unsigned char* ptr1T, * ptr2T;
  const bool IsPlanar = (np == 3);
  if (IsPlanar) // YV12, YV16, YV24: number of planes = 3 (planar)
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
    height >>= h_to_shift;
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
    }
    else {
      if (xsubsampling == 0 && ysubsampling == 0) // YV24 or luma
        SAD_fn = calcSSD_SSE2_16xN<16>;
      else if (xsubsampling == 1 && ysubsampling == 0) // YV16
        SAD_fn = calcSSD_SSE2_8xN<16>;
      else if (xsubsampling == 1 && ysubsampling == 1) // YV12
        SAD_fn = calcSSD_SSE2_8xN<8>;
    }
    // other formats are forbidden and were pre-checked

    for (y = 0; y < height; ++y)
    {
      temp1 = (y >> 1) * xblocks4;
      temp2 = ((y + 1) >> 1) * xblocks4;
      for (x = 0; x < width; ++x)
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
      ptr1 += pitch1 << w_to_shift; // += pitch1 * blocksize
      ptr2 += pitch2 << w_to_shift;
    }
    for (y = heighta; y < heights; ++y)
    {
      temp1 = (y >> (w_to_shift + 1)) * xblocks4; // y >> 5 or 4
      temp2 = ((y + (1 << w_to_shift)) >> (w_to_shift + 1)) * xblocks4;
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

void calcDiffSAD_32x32_SSE2(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t* diff, bool chroma, const VideoInfo& vi)
{
  calcDiff_SADorSSD_32x32_SSE2<true>(ptr1, ptr2, pitch1, pitch2, width, height, plane, xblocks4, np, diff, chroma, vi);
}

void calcDiffSSD_32x32_SSE2(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t* diff, bool chroma, const VideoInfo& vi)
{
  calcDiff_SADorSSD_32x32_SSE2<false>(ptr1, ptr2, pitch1, pitch2, width, height, plane, xblocks4, np, diff, chroma, vi);
}


// true: SAD, false: SSD
template<bool SAD>
void calcDiff_SADorSSD_Generic_SSE2(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, const VideoInfo& vi)
{
  int temp1, temp2, y, x, u, difft, box1, box2;
  int yshift, yhalf, xshift, xhalf;
  int heighta, heights = height, widtha, widths = width;
  int yshifta, yhalfa, xshifta, xhalfa;
  const unsigned char* ptr1T, * ptr2T;
  const bool IsPlanar = (np == 3);
  if (IsPlanar) // YV12, YV16, YV24
  {
    // from YV12 to generic planar
    const int xsubsampling = vi.GetPlaneWidthSubsampling(plane);
    const int ysubsampling = vi.GetPlaneHeightSubsampling(plane);
    // base: luma: 8x8, chroma: divided with subsampling
    const int w_to_shift = 3 - xsubsampling;
    const int h_to_shift = 3 - ysubsampling;
    // whole blocks
    heighta = (height >> h_to_shift) << h_to_shift; // mod16 for luma, mod8 or 16 for chroma
    widtha = (width >> w_to_shift) << w_to_shift; // mod16 for luma, mod8 or 16 for chroma
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
    }
    else {
      if (xsubsampling == 0 && ysubsampling == 0) // YV24 or luma
        SAD_fn = calcSSD_SSE2_8xN<8>;
      else if (xsubsampling == 1 && ysubsampling == 0) // YV16
        SAD_fn = calcSSD_SSE2_4xN<8>;
      else if (xsubsampling == 1 && ysubsampling == 1) // YV12
        SAD_fn = calcSSD_SSE2_4xN<4>;
    }
    // other formats are forbidden and were pre-checked

    yshifta = yshiftS - ysubsampling; // yshiftS  or yshiftS - 1
    yhalfa = yhalfS >> ysubsampling; // yhalfS  or yhalfS >> 1;
    xshifta = xshiftS - xsubsampling; //  xshiftS or  xshiftS - 1;
    xhalfa = xhalfS >> xsubsampling; // xhalfS  or xhalfS >> 1;
    // these are the same for luma and chroma as well, 8x8
    // FIXME: check, really?
    yshift = yshiftS - 3;
    yhalf = yhalfS >> 3;
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
      ptr1 += pitch1 << w_to_shift;
      ptr2 += pitch2 << w_to_shift;
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

void calcDiffSAD_Generic_SSE2(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, const VideoInfo& vi)
{
  calcDiff_SADorSSD_Generic_SSE2<true>(ptr1, ptr2, pitch1, pitch2, width, height, plane, xblocks4, np, diff, chroma, xshiftS, yshiftS, xhalfS, yhalfS, vi);
}

void calcDiffSSD_Generic_SSE2(const unsigned char* ptr1, const unsigned char* ptr2,
  int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, uint64_t* diff, bool chroma, int xshiftS, int yshiftS, int xhalfS, int yhalfS, const VideoInfo& vi)
{
  calcDiff_SADorSSD_Generic_SSE2<false>(ptr1, ptr2, pitch1, pitch2, width, height, plane, xblocks4, np, diff, chroma, xshiftS, yshiftS, xhalfS, yhalfS, vi);
}
