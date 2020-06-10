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

#include "TFMasm.h"
#include "emmintrin.h"

void checkSceneChangePlanar_1_SSE2(const uint8_t *prvp, const uint8_t *srcp,
  int height, int width, int prv_pitch, int src_pitch, uint64_t &diffp)
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


void checkSceneChangePlanar_2_SSE2(const uint8_t *prvp, const uint8_t *srcp,
  const uint8_t *nxtp, int height, int width, int prv_pitch, int src_pitch,
  int nxt_pitch, uint64_t &diffp, uint64_t &diffn)
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


void checkSceneChangeYUY2_1_SSE2(const uint8_t *prvp, const uint8_t *srcp,
  int height, int width, int prv_pitch, int src_pitch, uint64_t &diffp)
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


void checkSceneChangeYUY2_2_SSE2(const uint8_t *prvp, const uint8_t *srcp,
  const uint8_t *nxtp, int height, int width, int prv_pitch, int src_pitch,
  int nxt_pitch, uint64_t &diffp, uint64_t &diffn)
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

