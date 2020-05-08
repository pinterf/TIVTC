/*
**   Helper methods for TIVTC and TDeint
**
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

#include "TCommonASM.h"
#include "emmintrin.h"
#include <algorithm>

void absDiff_SSE2(const unsigned char *srcp1, const unsigned char *srcp2,
  unsigned char *dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width,
  int height, int mthresh1, int mthresh2)
{
  mthresh1 = std::min(std::max(255 - mthresh1, 0), 255);
  mthresh2 = std::min(std::max(255 - mthresh2, 0), 255);

  static const auto onesMask = _mm_set1_epi8(1);
  static const auto sthresh = _mm_set1_epi16((mthresh2 << 8) + mthresh1);
  static const auto all_ff = _mm_set1_epi8(-1);
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; x += 16)
    {
      auto src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp1 + x));
      auto src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp2 + x));
      auto diff12 = _mm_subs_epu8(src1, src2);
      auto diff21 = _mm_subs_epu8(src2, src1);
      auto diff = _mm_or_si128(diff12, diff21);
      auto addedsthresh = _mm_adds_epu8(diff, sthresh);
      auto cmpresult = _mm_cmpeq_epi8(addedsthresh, all_ff);
      auto res = _mm_xor_si128(cmpresult, all_ff);
      auto tmp = _mm_and_si128(res, onesMask);
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), tmp);
      /*
      if (abs(srcp1[x] - srcp2[x]) < mthresh1) dstp[x] = 1;
      else dstp[x] = 0;
      ++x;
      if (abs(srcp1[x] - srcp2[x]) < mthresh2) dstp[x] = 1;
      else dstp[x] = 0;
      */
    }
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
    dstp += dst_pitch;
  }

}

void absDiff_c(const unsigned char* srcp1, const unsigned char* srcp2,
  unsigned char* dstp, int src1_pitch, int src2_pitch, int dst_pitch, int width,
  int height, int mthresh1, int mthresh2)
{
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      if (abs(srcp1[x] - srcp2[x]) < mthresh1) dstp[x] = 1;
      else dstp[x] = 0;
      ++x;
      if (abs(srcp1[x] - srcp2[x]) < mthresh2) dstp[x] = 1;
      else dstp[x] = 0;
    }
    srcp1 += src1_pitch;
    srcp2 += src2_pitch;
    dstp += dst_pitch;
  }
}


// different path if not mod16, but only for remaining 8 bytes
void buildABSDiffMask_SSE2(const unsigned char* prvp, const unsigned char* nxtp,
  unsigned char* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width,
  int height)
{
  if (!(width & 15)) // exact mod16
  {
    while (height--) {
      for (int x = 0; x < width; x += 16)
      {
        __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i*>(prvp + x));
        __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i*>(nxtp + x));
        __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
        __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
        __m128i diff = _mm_or_si128(diffpn, diffnp);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), diff);
      }
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
  else {
    width -= 8; // last chunk is 8 bytes instead of 16
    while (height--) {
      int x;
      for (x = 0; x < width; x += 16)
      {
        __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i*>(prvp + x));
        __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i*>(nxtp + x));
        __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
        __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
        __m128i diff = _mm_or_si128(diffpn, diffnp);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), diff);
      }
      __m128i src_prev = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(prvp + x));
      __m128i src_next = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(nxtp + x));
      __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
      __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
      __m128i diff = _mm_or_si128(diffpn, diffnp);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x), diff);
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
}


template<bool YUY2_LumaOnly>
void buildABSDiffMask_c(const unsigned char* prvp, const unsigned char* nxtp,
  unsigned char* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height)
{
  if (width <= 0)
    return;

  if constexpr (YUY2_LumaOnly) {
    // TFM would use with ignoreYUY2chroma == true
    // where (vi.IsYUY2() && !mChroma)
    // Why: C version is quicker if dealing with every second (luma) pixel
    // (remark in 2020: only if compiler would not vectorize it
    // SSE2: no template because with omitting chroma is slower.
    // YUY2: YUYVYUYV... skip U and V
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; x += 4)
      {
        dstp[x + 0] = abs(prvp[x + 0] - nxtp[x + 0]);
        dstp[x + 2] = abs(prvp[x + 2] - nxtp[x + 2]);
      }
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
  else {
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; x += 4)
      {
        dstp[x + 0] = abs(prvp[x + 0] - nxtp[x + 0]);
        dstp[x + 1] = abs(prvp[x + 1] - nxtp[x + 1]);
        dstp[x + 2] = abs(prvp[x + 2] - nxtp[x + 2]);
        dstp[x + 3] = abs(prvp[x + 3] - nxtp[x + 3]);
      }
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
}

void do_buildABSDiffMask(const unsigned char* prvp, const unsigned char* nxtp, unsigned char* tbuffer,
  int prv_pitch, int nxt_pitch, int tpitch, int width, int height, bool YUY2_LumaOnly, int cpuFlags)
{
  if (cpuFlags & CPUF_SSE2 && width >= 8)
  {
    const int mod8Width = width / 8 * 8;
    // SSE2 is not chroma-ignore template, it's quicker if not skipping each YUY2 chroma
    buildABSDiffMask_SSE2(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, mod8Width, height);
    if(YUY2_LumaOnly)
      buildABSDiffMask_c<true>(prvp + mod8Width, nxtp + mod8Width, tbuffer + mod8Width, prv_pitch, nxt_pitch, tpitch, width - mod8Width, height);
    else
      buildABSDiffMask_c<false>(prvp + mod8Width, nxtp + mod8Width, tbuffer + mod8Width, prv_pitch, nxt_pitch, tpitch, width - mod8Width, height);
  }
  else {
    if (YUY2_LumaOnly)
      buildABSDiffMask_c<true>(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, width, height);
    else
      buildABSDiffMask_c<false>(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch, width, height);
  }
}


void do_buildABSDiffMask2(const unsigned char* prvp, const unsigned char* nxtp, unsigned char* dstp,
  int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height, bool YUY2_LumaOnly, int cpuFlags)
{
  if ((cpuFlags & CPUF_SSE2) && width >= 8)
  {
    int mod8Width = width / 8 * 8;
    buildABSDiffMask2_SSE2(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch, mod8Width, height);
    if (YUY2_LumaOnly)
      buildABSDiffMask2_c<true>(prvp + mod8Width, nxtp + mod8Width, dstp + mod8Width, prv_pitch, nxt_pitch, dst_pitch, width - mod8Width, height);
    else
      buildABSDiffMask2_c<false>(prvp + mod8Width, nxtp + mod8Width, dstp + mod8Width, prv_pitch, nxt_pitch, dst_pitch, width - mod8Width, height);
  }
  else {
    if (YUY2_LumaOnly)
      buildABSDiffMask2_c<true>(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch, width, height);
    else
      buildABSDiffMask2_c<false>(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch, width, height);
  }
}


// TDeint and TFM version
// fixme: AnalyzeDiffMask_Planar compare with AnalyzeDiffMask_YUY2
void AnalyzeDiffMask_Planar(unsigned char* dstp, int dst_pitch, unsigned char* tbuffer, int tpitch, int Width, int Height)
{
  const unsigned char* dppp = tbuffer - tpitch;
  const unsigned char* dpp = tbuffer;
  const unsigned char* dp = tbuffer + tpitch;
  const unsigned char* dpn = tbuffer + tpitch * 2;
  const unsigned char* dpnn = tbuffer + tpitch * 3;
  int count;
  bool upper, lower, upper2, lower2;
#ifdef USE_C_NO_ASM
  for (int y = 2; y < Height - 2; y += 2) {
    for (int x = 1; x < Width - 1; x++) {
      int eax, esi, edi, edx;

      if (dp[x] <= 3) continue;
      if (dp[x - 1] <= 3 && dp[x + 1] <= 3 &&
        dpp[x - 1] <= 3 && dpp[x] <= 3 && dpp[x + 1] <= 3 &&
        dpn[x - 1] <= 3 && dpn[x] <= 3 && dpn[x + 1] <= 3) continue;
      dstp[x]++; // inc BYTE PTR[esi + ebx]
      if (dp[x] <= 19) continue; //  cmp BYTE PTR[ecx + ebx], 19, ja b2

      edi = 0; // xor edi, edi
      lower = 0;
      upper = 0;

      if (dpp[x - 1] > 19) edi++;
      if (dpp[x] > 19) edi++;
      if (dpp[x + 1] > 19) edi++;

      if (edi != 0) upper = 1;

      if (dp[x - 1] > 19) edi++;
      if (dp[x + 1] > 19) edi++;

      esi = edi;

      if (dpn[x - 1] > 19) edi++;
      if (dpn[x] > 19) edi++;
      if (dpn[x + 1] > 19) edi++;

      if (edi <= 2) continue;

      count = edi; // mov count, edi
      if (count != esi) {  // cmp edi, esi, je b11
        lower = 1; // mov lower, 1
        if (upper != 0) { // cmp upper, 0, je b11
          dstp[x] += 2; // mov esi, dstp, add BYTE PTR[esi + ebx], 2
          continue; //  jmp c2
        }
      }
      // b11 :
      eax = x - 4; // mov eax, ebx, add eax, -4
      if (eax < 0) eax = 0; // jge p3, xor eax, eax
      //  p3 :

      edx = x + 5;
      lower2 = 0;
      upper2 = 0;
      if (edx > Width) edx = Width;
      if (y != 2) { // cmp y, 2,  je p5
        int esi = eax;
        do {
          if (dppp[esi] > 19) {
            upper2 = 1;  // ??? check others copied from here! todo change upper to upper2 if not upper 2 like in YUY2 part
            break;
          }
          esi++;
        } while (esi < edx);
      }
      // p5 :
      { // blocked for local vars
        int esi = eax;
        do {
          if (dpp[esi] > 19)
            upper = 1;
          if (dpn[esi] > 19)
            lower = 1;
          if (upper != 0 && lower != 0)
            break;
          esi++;
        } while (esi < edx);
      }

      //    p12:
      if (y != Height - 4) {

        int esi = eax;
        do {
          if (dpnn[esi] > 19) {
            lower2 = 1;
            break;
          }
          esi++;
        } while (esi < edx);

        //            cmp BYTE PTR[ecx + esi], 19
        //            ja p15
        //            inc esi
        //            cmp esi, edx
        //            jl p14
        //            jmp p13
        //            p15 :
        //          mov lower2, 1
      }
      //p13 :
      if (upper == 0) { // cmp upper, 0, jne p16
       //  cmp lower, 0
          //je p17
          //cmp lower2, 0
          //je p17
          //jmp p18
        if (lower == 0 || lower2 == 0) {
          // p17:
          if (count > 4)
            dstp[x] += 4;
        }
        else {
          dstp[x] += 2;
          // p18
        }
      }
      else {
        if (lower != 0 || upper2 != 0) {
          dstp[x] += 2;
        }
        else {
          if (count > 4)
            dstp[x] += 4;
        }
      }
    }
    dppp += tpitch;
    dpp += tpitch;
    dp += tpitch;
    dpn += tpitch;
    dpnn += tpitch;
    dstp += dst_pitch;
  }

#else
  // TFMYV12 565
  int y;
  __asm
  {
    mov y, 2
    yloop:
    mov edi, Width
      mov eax, dpp
      mov ecx, dp
      mov edx, dpn
      mov ebx, 1
      mov esi, dstp
      dec edi
      xloop :
    cmp BYTE PTR[ecx + ebx], 3
      ja b1
      inc ebx
      cmp ebx, edi
      jl xloop
      jmp end_yloop
      b1 :
    cmp BYTE PTR[ecx + ebx - 1], 3
      ja p1
      cmp BYTE PTR[ecx + ebx + 1], 3
      ja p1
      cmp BYTE PTR[eax + ebx - 1], 3
      ja p1
      cmp BYTE PTR[eax + ebx], 3
      ja p1
      cmp BYTE PTR[eax + ebx + 1], 3
      ja p1
      cmp BYTE PTR[edx + ebx - 1], 3
      ja p1
      cmp BYTE PTR[edx + ebx], 3
      ja p1
      cmp BYTE PTR[edx + ebx + 1], 3
      ja p1
      inc ebx
      cmp ebx, edi
      jl xloop
      jmp end_yloop
      p1 :
    inc BYTE PTR[esi + ebx]
      cmp BYTE PTR[ecx + ebx], 19
      ja b2
      inc ebx
      cmp ebx, edi
      jl xloop
      jmp end_yloop
      b2 :
    xor edi, edi
      cmp BYTE PTR[eax + ebx - 1], 19
      mov lower, 0
      mov upper, 0
      jbe b3
      inc edi
      b3 :
    cmp BYTE PTR[eax + ebx], 19
      jbe b4
      inc edi
      b4 :
    cmp BYTE PTR[eax + ebx + 1], 19
      jbe b5
      inc edi
      b5 :
    or edi, edi
      jz p2
      mov upper, 1
      p2 :
      cmp BYTE PTR[ecx + ebx - 1], 19
      jbe b6
      inc edi
      b6 :
    cmp BYTE PTR[ecx + ebx + 1], 19
      jbe b7
      inc edi
      b7 :
    mov esi, edi
      cmp BYTE PTR[edx + ebx - 1], 19
      jbe b8
      inc edi
      b8 :
    cmp BYTE PTR[edx + ebx], 19
      jbe b9
      inc edi
      b9 :
    cmp BYTE PTR[edx + ebx + 1], 19
      jbe b10
      inc edi
      b10 :
    cmp edi, 2
      jg c1
      c2 :
    mov edi, Width
      inc ebx
      dec edi
      cmp ebx, edi
      jge end_yloop
      mov esi, dstp
      jmp xloop
      c1 :
    cmp edi, esi
      mov count, edi
      je b11
      mov lower, 1
      cmp upper, 0
      je b11
      mov esi, dstp
      add BYTE PTR[esi + ebx], 2
      jmp c2
      b11 :
    mov eax, ebx
      add eax, -4
      jge p3
      xor eax, eax
      p3 :
    mov edx, ebx
      mov ecx, Width
      mov lower2, 0
      add edx, 5
      mov upper2, 0
      cmp edx, ecx
      jle p4
      mov edx, ecx
      p4 :
    cmp y, 2
      je p5
      mov esi, eax
      mov ecx, dppp
      p6 :
    cmp BYTE PTR[ecx + esi], 19
      ja p7
      inc esi
      cmp esi, edx
      jl p6
      jmp p5
      p7 :
    mov upper2, 1
      p5 :
      mov esi, eax
      mov ecx, dpp
      mov edi, dpn
      p11 :
    cmp BYTE PTR[ecx + esi], 19
      jbe p8
      mov upper, 1
      p8 :
      cmp BYTE PTR[edi + esi], 19
      jbe p9
      mov lower, 1
      p9 :
      cmp upper, 0
      je p10
      cmp lower, 0
      jne p12
      p10 :
    inc esi
      cmp esi, edx
      jl p11
      p12 :
    mov esi, Height
      add esi, -4
      cmp y, esi
      je p13
      mov esi, eax
      mov ecx, dpnn
      p14 :
    cmp BYTE PTR[ecx + esi], 19
      ja p15
      inc esi
      cmp esi, edx
      jl p14
      jmp p13
      p15 :
    mov lower2, 1
      p13 :
      cmp upper, 0
      jne p16
      cmp lower, 0
      je p17
      cmp lower2, 0
      je p17
      jmp p18
      p16 :
    cmp lower, 0
      jne p18
      cmp upper2, 0
      jne p18
      jmp p17
      p18 :
    mov esi, dstp
      add BYTE PTR[esi + ebx], 2
      jmp end_xloop
      p17 :
    cmp count, 4
      jle end_xloop
      mov esi, dstp
      add BYTE PTR[esi + ebx], 4
      end_xloop :
      mov edi, Width
      inc ebx
      dec edi
      cmp ebx, edi
      jge end_yloop
      mov eax, dpp
      mov ecx, dp
      mov edx, dpn
      mov esi, dstp
      jmp xloop
      end_yloop :
    mov edi, tpitch
      mov eax, dst_pitch
      mov ecx, Height
      add y, 2
      sub ecx, 2
      add dppp, edi
      add dpp, edi
      add dp, edi
      add dpn, edi
      add dpnn, edi
      add dstp, eax
      cmp y, ecx
      jl yloop
  }
#endif // todo
}

// TDeint and TFM version
// fixme: compare with AnalyzeDiffMask_Planar + Compare chroma and non-chroma parts
void AnalyzeDiffMask_YUY2(unsigned char* dstp, int dst_pitch, unsigned char* tbuffer, int tpitch, int Width, int Height, bool mChroma)
{
  const unsigned char* dppp = tbuffer - tpitch;
  const unsigned char* dpp = tbuffer;
  const unsigned char* dp = tbuffer + tpitch;
  const unsigned char* dpn = tbuffer + tpitch * 2;
  const unsigned char* dpnn = tbuffer + tpitch * 3;
  int count;
  bool upper, lower, upper2, lower2;

  // fixit: make if common with TFM
#ifdef USE_C_NO_ASM
  // reconstructed from inline asm by pf

  if (mChroma) // TFM YUY2's mChroma bool parameter
  {
    for (int y = 2; y < Height - 2; y += 2) {
      for (int x = 4; x < Width - 4; x += 1) // the first ebx++ is in the code. later in TFMYUY2 1051: ebx += 2
      {
        int eax, esi, edi;

        if (dp[x] < 3) {
          goto chroma_sec;
        } // continue in TFMYUY2 1051;
        if (dp[x - 2] < 3 && dp[x + 2] < 3 &&
          dpp[x - 2] < 3 && dpp[x] < 3 && dpp[x + 2] < 3 &&
          dpn[x - 2] < 3 && dpn[x] < 3 && dpn[x + 2] < 3)
        {
          goto chroma_sec;
        }// continue in TFMYUY2 1051;
        dstp[x]++;
        if (dp[x] <= 19) {
          goto chroma_sec;
        }// continue in TFMYUY2 1051;
        //mov eax, dpp
        //  mov ecx, dp
        //  mov edx, dpn

        edi = 0;
        lower = 0;
        upper = 0;

        if (dpp[x - 2] > 19) edi++;
        if (dpp[x] > 19) edi++;
        if (dpp[x + 2] > 19) edi++;

        if (edi != 0) upper = 1;

        if (dp[x - 2] > 19) edi++;
        if (dp[x + 2] > 19) edi++;

        esi = edi;

        if (dpn[x - 2] > 19) edi++;
        if (dpn[x] > 19) edi++;
        if (dpn[x + 2] > 19) edi++;

        if (edi <= 2) {
          goto chroma_sec; // continue in TFMYUY2 1051;
        }// continue in TFMYUY2 1051;

        count = edi;
        if (count != esi) {
          lower = 1;
          if (upper != 0) {
            dstp[x] += 2;
            goto chroma_sec; // continue in TFMYUY2 1051;
          }
        }
        // b111:
        eax = x - 2 * 4;
        if (eax < 0) eax = 0;
        // p31:
        lower2 = 0;
        upper2 = 0;
        { // blocked for local vars
          int edx = x + 2 * 5;
          if (edx > Width) edx = Width;
          // p41:
    //-----------
          if (y != 2) { // cmp y, 2,  je p51
            int esi = eax;
            do {
              if (dppp[esi] > 19) {
                upper2 = 1;
                break;
              }
              esi += 2; // YUY2 inc
            } while (esi < edx);
          }
          // p51 :
          int esi = eax;
          do {
            if (dpp[esi] > 19)
              upper = 1;
            if (dpn[esi] > 19)
              lower = 1;
            if (upper != 0 && lower != 0)
              break;
            esi += 2; // YUY2 inc
          } while (esi < edx);
          //---------
          // p121
          //    p12:
          if (y != Height - 4) {
            int esi = eax;
            do {
              if (dpnn[esi] > 19) {
                lower2 = 1;
                break;
              }
              esi += 2; // YUY2 inc
            } while (esi < edx);
          }
        }
        //p13 :
        if (upper == 0) { // cmp upper, 0, jne p16
                          //  cmp lower, 0
                          //je p17
                          //cmp lower2, 0
                          //je p17
                          //jmp p18
          if (lower == 0 || lower2 == 0) {
            // p17:
            if (count > 4)
              dstp[x] += 4;
          }
          else {
            dstp[x] += 2;
            // p18
          }
        }
        else {
          if (lower != 0 || upper2 != 0) {
            dstp[x] += 2;
          }
          else {
            if (count > 4)
              dstp[x] += 4;
          }
        }
        //-------------- itt vége van egy ebx+=2-vel a másiknak   

      chroma_sec:
        // skip to chroma
        x++;

        if (dp[x] < 3) {
          continue;
        } // continue in TFMYUY2 1051;
        if (dp[x - 4] < 3 && dp[x + 4] < 3 &&
          dpp[x - 4] < 3 && dpp[x] < 3 && dpp[x + 4] < 3 &&
          dpn[x - 4] < 3 && dpn[x] < 3 && dpn[x + 4] < 3)
        {
          continue;
        }// continue in TFMYUY2 1051;
        dstp[x]++;
        if (dp[x] <= 19) {
          continue;
        }// continue in TFMYUY2 1051;
         //mov eax, dpp
         //  mov ecx, dp
         //  mov edx, dpn
        edi = 0;
        lower = 0;
        upper = 0;
        if (dpp[x - 4] > 19) edi++;
        if (dpp[x] > 19) edi++;
        if (dpp[x + 4] > 19) edi++;
        if (edi != 0) upper = 1;
        if (dp[x - 4] > 19) edi++;
        if (dp[x + 4] > 19) edi++;
        esi = edi;
        if (dpn[x - 4] > 19) edi++;
        if (dpn[x] > 19) edi++;
        if (dpn[x + 4] > 19) edi++;
        if (edi <= 2) {
          continue; // continue in TFMYUY2 1051;
        }// continue in TFMYUY2 1051;
        count = edi;
        if (esi != edi) {
          lower = 1;
          if (upper != 0) {
            dstp[x] += 2;
            continue;
          }
        }
        // b111:
        eax = x - 8 - 8; // was: -8 in the first chroma part
        int edx_tmp = (x & 2) + 1; // diff from 1st part, comparison is with 0 there
        if (eax < edx_tmp) eax = edx_tmp; // diff from 1st part
        // p31:/p3c
        lower2 = 0;
        upper2 = 0;
        int edx = x + 10 + 8; // was: +10 in the first chroma part
        if (edx > Width) edx = Width;
        // p41:
        //-----------
        if (y != 2) { // cmp y, 2,  je p51
          int esi = eax;
          do {
            if (dppp[esi] > 19) {
              upper2 = 1;
              break;
            }
            esi += 4; // was: += 2 in the first chroma part
          } while (esi < edx);
        }
        // p51 :
        { // blocked for local vars
          int esi = eax;
          do {
            if (dpp[esi] > 19)
              upper = 1;
            if (dpn[esi] > 19)
              lower = 1;
            if (upper != 0 && lower != 0)
              break;
            esi += 4;  // was: += 2 in the first chroma part
          } while (esi < edx);
        }
        //---------
        // p121
        //    p12:
        if (y != Height - 4) {

          int esi = eax;
          do {
            if (dpnn[esi] > 19) {
              lower2 = 1;
              break;
            }
            esi += 4;  // was: += 2 in the first chroma part
          } while (esi < edx);
        }
        //p13 :
        if (upper == 0) { // cmp upper, 0, jne p16
                          //  cmp lower, 0
                          //je p17
                          //cmp lower2, 0
                          //je p17
                          //jmp p18
          if (lower == 0 || lower2 == 0) {
            // p17:
            if (count > 4)
              dstp[x] += 4;
          }
          else {
            dstp[x] += 2;
            // p18
          }
        }
        else {
          if (lower != 0 || upper2 != 0) {
            dstp[x] += 2;
          }
          else {
            if (count > 4)
              dstp[x] += 4;
          }
        }

      }
      dppp += tpitch;
      dpp += tpitch;
      dp += tpitch;
      dpn += tpitch;
      dpnn += tpitch;
      dstp += dst_pitch;

    }
  }
  else {
    // no YUY2 chroma, LumaOnly
    // TFMYUV2 1051
    //env->ThrowError("not implemented yet"); // to be checked. What options do we need
    for (int y = 2; y < Height - 2; y += 2) {
      for (int x = 4; x < Width - 4; x += 2) // the first ebx++ is in the code. later in TFMYUY2 1051: ebx += 2
      {
        if (dp[x] < 3) {
          continue; // goto chroma_sec;
        } // continue in TFMYUY2 1051;
        if (dp[x - 2] < 3 && dp[x + 2] < 3 &&
          dpp[x - 2] < 3 && dpp[x] < 3 && dpp[x + 2] < 3 &&
          dpn[x - 2] < 3 && dpn[x] < 3 && dpn[x + 2] < 3)
        {
          continue; // goto chroma_sec;
        }// continue in TFMYUY2 1051;
        dstp[x]++;
        if (dp[x] <= 19) {
          continue; //  goto chroma_sec;
        }// continue in TFMYUY2 1051;
         //mov eax, dpp
         //  mov ecx, dp
         //  mov edx, dpn
        int edi = 0;
        lower = 0;
        upper = 0;
        if (dpp[x - 2] > 19) edi++;
        if (dpp[x] > 19) edi++;
        if (dpp[x + 2] > 19) edi++;
        if (edi != 0) upper = 1;
        if (dp[x - 2] > 19) edi++;
        if (dp[x + 2] > 19) edi++;
        int esi = edi;
        if (dpn[x - 2] > 19) edi++;
        if (dpn[x] > 19) edi++;
        if (dpn[x + 2] > 19) edi++;
        if (edi <= 2) {
          continue; //  goto chroma_sec; // continue in TFMYUY2 1051;
        }// continue in TFMYUY2 1051;
        count = edi;
        if (esi != edi) {
          lower = 1;
          if (upper != 0) {
            dstp[x] += 2;
            continue; // goto chroma_sec; // continue in TFMYUY2 1051;
          }
        }
        // b111:
        int eax = x - 8;
        if (eax < 0) eax = 0;
        // p31:
        lower2 = 0;
        upper2 = 0;
        int edx = x + 10;
        if (edx > Width) edx = Width;
        // p41:
        //-----------
        if (y != 2) { // cmp y, 2,  je p51
          int esi = eax;
          do {
            if (dppp[esi] > 19) {
              upper2 = 1;
              break;
            }
            esi += 2;
          } while (esi < edx);
        }
        // p51 :
        { // blocked for local vars
          int esi = eax;
          do {
            if (dpp[esi] > 19)
              upper = 1;
            if (dpn[esi] > 19)
              lower = 1;
            if (upper != 0 && lower != 0)
              break;
            esi += 2;
          } while (esi < edx);
        }
        //---------
        // p121
        //    p12:
        if (y != Height - 4) {

          int esi = eax;
          do {
            if (dpnn[esi] > 19) {
              lower2 = 1;
              break;
            }
            esi += 2;
          } while (esi < edx);
        }
        //p13 :
        if (upper == 0) { // cmp upper, 0, jne p16
                          //  cmp lower, 0
                          //je p17
                          //cmp lower2, 0
                          //je p17
                          //jmp p18
          if (lower == 0 || lower2 == 0) {
            // p17:
            if (count > 4)
              dstp[x] += 4;
          }
          else {
            dstp[x] += 2;
            // p18
          }
        }
        else {
          if (lower != 0 || upper2 != 0) {
            dstp[x] += 2;
          }
          else {
            if (count > 4)
              dstp[x] += 4;
          }
        }
      }
      dppp += tpitch;
      dpp += tpitch;
      dp += tpitch;
      dpn += tpitch;
      dpnn += tpitch;
      dstp += dst_pitch;

    }

  }
#else
  int y;
  if (mChroma)
  {
    // TFMYUV2 636
    __asm
    {
      push ebx // pf170421

      mov y, 2
      yloopl:
      mov edi, Width
        mov eax, dpp
        mov ecx, dp
        mov edx, dpn
        mov ebx, 4
        mov esi, dstp
        sub edi, 4
        xloopl:
      cmp BYTE PTR[ecx + ebx], 3
        ja b1l
        inc ebx
        jmp chroma_sec
        b1l :
      cmp BYTE PTR[ecx + ebx - 2], 3
        ja p1l
        cmp BYTE PTR[ecx + ebx + 2], 3
        ja p1l
        cmp BYTE PTR[eax + ebx - 2], 3
        ja p1l
        cmp BYTE PTR[eax + ebx], 3
        ja p1l
        cmp BYTE PTR[eax + ebx + 2], 3
        ja p1l
        cmp BYTE PTR[edx + ebx - 2], 3
        ja p1l
        cmp BYTE PTR[edx + ebx], 3
        ja p1l
        cmp BYTE PTR[edx + ebx + 2], 3
        ja p1l
        inc ebx
        jmp chroma_sec
        p1l :
      inc BYTE PTR[esi + ebx]
        cmp BYTE PTR[ecx + ebx], 19
        ja b2l
        inc ebx
        jmp chroma_sec
        b2l :
      xor edi, edi
        cmp BYTE PTR[eax + ebx - 2], 19
        mov lower, 0
        mov upper, 0
        jbe b3l
        inc edi
        b3l :
      cmp BYTE PTR[eax + ebx], 19
        jbe b4l
        inc edi
        b4l :
      cmp BYTE PTR[eax + ebx + 2], 19
        jbe b5l
        inc edi
        b5l :
      or edi, edi
        jz p2l
        mov upper, 1
        p2l :
        cmp BYTE PTR[ecx + ebx - 2], 19
        jbe b6l
        inc edi
        b6l :
      cmp BYTE PTR[ecx + ebx + 2], 19
        jbe b7l
        inc edi
        b7l :
      mov esi, edi
        cmp BYTE PTR[edx + ebx - 2], 19
        jbe b8l
        inc edi
        b8l :
      cmp BYTE PTR[edx + ebx], 19
        jbe b9l
        inc edi
        b9l :
      cmp BYTE PTR[edx + ebx + 2], 19
        jbe b10l
        inc edi
        b10l :
      cmp edi, 2
        jg c1l
        c2l :
      mov edi, Width
        inc ebx
        mov esi, dstp
        sub edi, 4
        jmp chroma_sec
        c1l :
      cmp edi, esi
        mov count, edi
        je b11l
        mov lower, 1
        cmp upper, 0
        je b11l
        mov esi, dstp
        add BYTE PTR[esi + ebx], 2
        jmp c2l
        b11l :
      mov eax, ebx
        add eax, -8
        jge p3l
        xor eax, eax
        p3l :
      mov edx, ebx
        mov ecx, Width
        mov lower2, 0
        add edx, 10
        mov upper2, 0
        cmp edx, ecx
        jle p4l
        mov edx, ecx
        p4l :
      cmp y, 2
        je p5l
        mov esi, eax
        mov ecx, dppp
        p6l :
      cmp BYTE PTR[ecx + esi], 19
        ja p7l
        add esi, 2
        cmp esi, edx
        jl p6l
        jmp p5l
        p7l :
      mov upper2, 1
        p5l :
        mov esi, eax
        mov ecx, dpp
        mov edi, dpn
        p11l :
      cmp BYTE PTR[ecx + esi], 19
        jbe p8l
        mov upper, 1
        p8l :
        cmp BYTE PTR[edi + esi], 19
        jbe p9l
        mov lower, 1
        p9l :
        cmp upper, 0
        je p10l
        cmp lower, 0
        jne p12l
        p10l :
      add esi, 2
        cmp esi, edx
        jl p11l
        p12l :
      mov esi, Height
        add esi, -4
        cmp y, esi
        je p13l
        mov esi, eax
        mov ecx, dpnn
        p14l :
      cmp BYTE PTR[ecx + esi], 19
        ja p15l
        add esi, 2
        cmp esi, edx
        jl p14l
        jmp p13l
        p15l :
      mov lower2, 1
        p13l :
        cmp upper, 0
        jne p16l
        cmp lower, 0
        je p17l
        cmp lower2, 0
        je p17l
        jmp p18l
        p16l :
      cmp lower, 0
        jne p18l
        cmp upper2, 0
        jne p18l
        jmp p17l
        p18l :
      mov esi, dstp
        add BYTE PTR[esi + ebx], 2
        jmp end_xloopl
        p17l :
      cmp count, 4
        jle end_xloopl
        mov esi, dstp
        add BYTE PTR[esi + ebx], 4
        end_xloopl :
        mov edi, Width
        inc ebx
        sub edi, 4
        mov eax, dpp
        mov ecx, dp
        mov edx, dpn
        mov esi, dstp
        chroma_sec :
      cmp BYTE PTR[ecx + ebx], 3
        ja b1c
        inc ebx
        cmp ebx, edi
        jl xloopl
        jmp end_yloopl
        b1c :
      cmp BYTE PTR[ecx + ebx - 4], 3
        ja p1c
        cmp BYTE PTR[ecx + ebx + 4], 3
        ja p1c
        cmp BYTE PTR[eax + ebx - 4], 3
        ja p1c
        cmp BYTE PTR[eax + ebx], 3
        ja p1c
        cmp BYTE PTR[eax + ebx + 4], 3
        ja p1c
        cmp BYTE PTR[edx + ebx - 4], 3
        ja p1c
        cmp BYTE PTR[edx + ebx], 3
        ja p1c
        cmp BYTE PTR[edx + ebx + 4], 3
        ja p1c
        inc ebx
        cmp ebx, edi
        jl xloopl
        jmp end_yloopl
        p1c :
      inc BYTE PTR[esi + ebx]
        cmp BYTE PTR[ecx + ebx], 19
        ja b2c
        inc ebx
        cmp ebx, edi
        jl xloopl
        jmp end_yloopl
        b2c :
      xor edi, edi
        cmp BYTE PTR[eax + ebx - 4], 19
        mov lower, 0
        mov upper, 0
        jbe b3c
        inc edi
        b3c :
      cmp BYTE PTR[eax + ebx], 19
        jbe b4c
        inc edi
        b4c :
      cmp BYTE PTR[eax + ebx + 4], 19
        jbe b5c
        inc edi
        b5c :
      or edi, edi
        jz p2c
        mov upper, 1
        p2c :
        cmp BYTE PTR[ecx + ebx - 4], 19
        jbe b6c
        inc edi
        b6c :
      cmp BYTE PTR[ecx + ebx + 4], 19
        jbe b7c
        inc edi
        b7c :
      mov esi, edi
        cmp BYTE PTR[edx + ebx - 4], 19
        jbe b8c
        inc edi
        b8c :
      cmp BYTE PTR[edx + ebx], 19
        jbe b9c
        inc edi
        b9c :
      cmp BYTE PTR[edx + ebx + 4], 19
        jbe b10c
        inc edi
        b10c :
      cmp edi, 2
        jg c1c
        c2c :
      mov edi, Width
        inc ebx
        sub edi, 4
        cmp ebx, edi
        jge end_yloopl
        mov esi, dstp
        jmp xloopl
        c1c :
      cmp edi, esi
        mov count, edi
        je b11c
        mov lower, 1
        cmp upper, 0
        je b11c
        mov esi, dstp
        add BYTE PTR[esi + ebx], 2
        jmp c2c
        b11c :
      mov eax, ebx
        add eax, -16
        mov edx, ebx
        and edx, 2
        inc edx
        cmp eax, edx
        jge p3c
        mov eax, edx
        p3c :
      mov edx, ebx
        mov ecx, Width
        mov lower2, 0
        add edx, 18
        mov upper2, 0
        cmp edx, ecx
        jle p4c
        mov edx, ecx
        p4c :
      cmp y, 2
        je p5c
        mov esi, eax
        mov ecx, dppp
        p6c :
      cmp BYTE PTR[ecx + esi], 19
        ja p7c
        add esi, 4
        cmp esi, edx
        jl p6c
        jmp p5c
        p7c :
      mov upper2, 1
        p5c :
        mov esi, eax
        mov ecx, dpp
        mov edi, dpn
        p11c :
      cmp BYTE PTR[ecx + esi], 19
        jbe p8c
        mov upper, 1
        p8c :
        cmp BYTE PTR[edi + esi], 19
        jbe p9c
        mov lower, 1
        p9c :
        cmp upper, 0
        je p10c
        cmp lower, 0
        jne p12c
        p10c :
      add esi, 4
        cmp esi, edx
        jl p11c
        p12c :
      mov esi, Height
        add esi, -4
        cmp y, esi
        je p13c
        mov esi, eax
        mov ecx, dpnn
        p14c :
      cmp BYTE PTR[ecx + esi], 19
        ja p15c
        add esi, 4
        cmp esi, edx
        jl p14c
        jmp p13c
        p15c :
      mov lower2, 1
        p13c :
        cmp upper, 0
        jne p16c
        cmp lower, 0
        je p17c
        cmp lower2, 0
        je p17c
        jmp p18c
        p16c :
      cmp lower, 0
        jne p18c
        cmp upper2, 0
        jne p18c
        jmp p17c
        p18c :
      mov esi, dstp
        add BYTE PTR[esi + ebx], 2
        jmp end_xloopc
        p17c :
      cmp count, 4
        jle end_xloopc
        mov esi, dstp
        add BYTE PTR[esi + ebx], 4
        end_xloopc :
        mov edi, Width
        inc ebx
        sub edi, 4
        cmp ebx, edi
        jge end_yloopl
        mov eax, dpp
        mov ecx, dp
        mov edx, dpn
        mov esi, dstp
        jmp xloopl
        end_yloopl :
      mov edi, tpitch
        mov eax, dst_pitch
        mov ecx, Height
        add y, 2
        sub ecx, 2
        add dppp, edi
        add dpp, edi
        add dp, edi
        add dpn, edi
        add dpnn, edi
        add dstp, eax
        cmp y, ecx
        jl yloopl

        pop ebx // pf170421
    }
  }
  else
  {
    // TFMYUV2 1051
    __asm
    {
      push ebx // pf170421

      mov y, 2
      yloop:
      mov edi, Width
        mov eax, dpp
        mov ecx, dp
        mov edx, dpn
        mov ebx, 4
        mov esi, dstp
        sub edi, 4
        xloop:
      cmp BYTE PTR[ecx + ebx], 3
        ja b1
        add ebx, 2
        cmp ebx, edi
        jl xloop
        jmp end_yloop
        b1 :
      cmp BYTE PTR[ecx + ebx - 2], 3
        ja p1
        cmp BYTE PTR[ecx + ebx + 2], 3
        ja p1
        cmp BYTE PTR[eax + ebx - 2], 3
        ja p1
        cmp BYTE PTR[eax + ebx], 3
        ja p1
        cmp BYTE PTR[eax + ebx + 2], 3
        ja p1
        cmp BYTE PTR[edx + ebx - 2], 3
        ja p1
        cmp BYTE PTR[edx + ebx], 3
        ja p1
        cmp BYTE PTR[edx + ebx + 2], 3
        ja p1
        add ebx, 2
        cmp ebx, edi
        jl xloop
        jmp end_yloop
        p1 :
      inc BYTE PTR[esi + ebx]
        cmp BYTE PTR[ecx + ebx], 19
        ja b2
        add ebx, 2
        cmp ebx, edi
        jl xloop
        jmp end_yloop
        b2 :
      xor edi, edi
        cmp BYTE PTR[eax + ebx - 2], 19
        mov lower, 0
        mov upper, 0
        jbe b3
        inc edi
        b3 :
      cmp BYTE PTR[eax + ebx], 19
        jbe b4
        inc edi
        b4 :
      cmp BYTE PTR[eax + ebx + 2], 19
        jbe b5
        inc edi
        b5 :
      or edi, edi
        jz p2
        mov upper, 1
        p2 :
        cmp BYTE PTR[ecx + ebx - 2], 19
        jbe b6
        inc edi
        b6 :
      cmp BYTE PTR[ecx + ebx + 2], 19
        jbe b7
        inc edi
        b7 :
      mov esi, edi
        cmp BYTE PTR[edx + ebx - 2], 19
        jbe b8
        inc edi
        b8 :
      cmp BYTE PTR[edx + ebx], 19
        jbe b9
        inc edi
        b9 :
      cmp BYTE PTR[edx + ebx + 2], 19
        jbe b10
        inc edi
        b10 :
      cmp edi, 2
        jg c1
        c2 :
      mov edi, Width
        add ebx, 2
        sub edi, 4
        cmp ebx, edi
        jge end_yloop
        mov esi, dstp
        jmp xloop
        c1 :
      cmp edi, esi
        mov count, edi
        je b11
        mov lower, 1
        cmp upper, 0
        je b11
        mov esi, dstp
        add BYTE PTR[esi + ebx], 2
        jmp c2
        b11 :
      mov eax, ebx
        add eax, -8
        jge p3
        xor eax, eax
        p3 :
      mov edx, ebx
        mov ecx, Width
        mov lower2, 0
        add edx, 10
        mov upper2, 0
        cmp edx, ecx
        jle p4
        mov edx, ecx
        p4 :
      cmp y, 2
        je p5
        mov esi, eax
        mov ecx, dppp
        p6 :
      cmp BYTE PTR[ecx + esi], 19
        ja p7
        add esi, 2
        cmp esi, edx
        jl p6
        jmp p5
        p7 :
      mov upper2, 1
        p5 :
        mov esi, eax
        mov ecx, dpp
        mov edi, dpn
        p11 :
      cmp BYTE PTR[ecx + esi], 19
        jbe p8
        mov upper, 1
        p8 :
        cmp BYTE PTR[edi + esi], 19
        jbe p9
        mov lower, 1
        p9 :
        cmp upper, 0
        je p10
        cmp lower, 0
        jne p12
        p10 :
      add esi, 2
        cmp esi, edx
        jl p11
        p12 :
      mov esi, Height
        add esi, -4
        cmp y, esi
        je p13
        mov esi, eax
        mov ecx, dpnn
        p14 :
      cmp BYTE PTR[ecx + esi], 19
        ja p15
        add esi, 2
        cmp esi, edx
        jl p14
        jmp p13
        p15 :
      mov lower2, 1
        p13 :
        cmp upper, 0
        jne p16
        cmp lower, 0
        je p17
        cmp lower2, 0
        je p17
        jmp p18
        p16 :
      cmp lower, 0
        jne p18
        cmp upper2, 0
        jne p18
        jmp p17
        p18 :
      mov esi, dstp
        add BYTE PTR[esi + ebx], 2
        jmp end_xloop
        p17 :
      cmp count, 4
        jle end_xloop
        mov esi, dstp
        add BYTE PTR[esi + ebx], 4
        end_xloop :
        mov edi, Width
        add ebx, 2
        sub edi, 4
        cmp ebx, edi
        jge end_yloop
        mov eax, dpp
        mov ecx, dp
        mov edx, dpn
        mov esi, dstp
        jmp xloop
        end_yloop :
      mov edi, tpitch
        mov eax, dst_pitch
        mov ecx, Height
        add y, 2
        sub ecx, 2
        add dppp, edi
        add dpp, edi
        add dp, edi
        add dpn, edi
        add dpnn, edi
        add dstp, eax
        cmp y, ecx
        jl yloop

        pop ebx // pf170421
    }
  }
#endif
}

template<bool YUY2_LumaOnly>
void buildABSDiffMask2_c(const unsigned char* prvp, const unsigned char* nxtp,
  unsigned char* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width, int height)
{
  if (width <= 0)
    return;

  constexpr int inc = YUY2_LumaOnly ? 2 : 1;
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; x += inc)
    {
      const int diff = abs(prvp[x] - nxtp[x]);
      if (diff > 19) dstp[x] = 3;
      else if (diff > 3) dstp[x] = 1;
      else dstp[x] = 0;
    }
    prvp += prv_pitch;
    nxtp += nxt_pitch;
    dstp += dst_pitch;
  }
}



void buildABSDiffMask2_SSE2(const unsigned char* prvp, const unsigned char* nxtp,
  unsigned char* dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int width,
  int height)
{
  __m128i onesMask = _mm_set1_epi8(0x01);
  __m128i twosMask = _mm_set1_epi8(0x02);
  __m128i all_ff = _mm_set1_epi8(-1);
  __m128i mask251 = _mm_set1_epi8((char)0xFB); // 1111 1011
  __m128i mask235 = _mm_set1_epi8((char)0xEB); // 1110 1011

  if (!(width & 15)) // exact mod16
  {
    while (height--) {
      for (int x = 0; x < width; x += 16)
      {
        __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i*>(prvp + x));
        __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i*>(nxtp + x));
        __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
        __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
        __m128i diff = _mm_or_si128(diffpn, diffnp);
        /*
        const int diff = abs(prvp[x] - nxtp[x]);
        if (diff > 19) dstp[x] = 3;
        else if (diff > 3) dstp[x] = 1;
        else dstp[x] = 0;
        */
        __m128i added251 = _mm_adds_epu8(diff, mask251);
        __m128i added235 = _mm_adds_epu8(diff, mask235);
        __m128i cmp251 = _mm_cmpeq_epi8(added251, all_ff);
        __m128i cmp235 = _mm_cmpeq_epi8(added235, all_ff);
        __m128i tmp1 = _mm_and_si128(cmp251, onesMask);
        __m128i tmp2 = _mm_and_si128(cmp235, twosMask);
        __m128i tmp = _mm_or_si128(tmp1, tmp2);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), tmp);
      }
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
  else {
    width -= 8; // last chunk is 8 bytes instead of 16
    while (height--) {
      int x;
      for (x = 0; x < width; x += 16)
      {
        __m128i src_prev = _mm_load_si128(reinterpret_cast<const __m128i*>(prvp + x));
        __m128i src_next = _mm_load_si128(reinterpret_cast<const __m128i*>(nxtp + x));
        __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
        __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
        __m128i diff = _mm_or_si128(diffpn, diffnp);
        __m128i added251 = _mm_adds_epu8(diff, mask251);
        __m128i added235 = _mm_adds_epu8(diff, mask235);
        __m128i cmp251 = _mm_cmpeq_epi8(added251, all_ff);
        __m128i cmp235 = _mm_cmpeq_epi8(added235, all_ff);
        __m128i tmp1 = _mm_and_si128(cmp251, onesMask);
        __m128i tmp2 = _mm_and_si128(cmp235, twosMask);
        __m128i tmp = _mm_or_si128(tmp1, tmp2);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), tmp);
      }
      __m128i src_prev = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(prvp + x));
      __m128i src_next = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(nxtp + x));
      __m128i diffpn = _mm_subs_epu8(src_prev, src_next);
      __m128i diffnp = _mm_subs_epu8(src_next, src_prev);
      __m128i diff = _mm_or_si128(diffpn, diffnp);
      __m128i added251 = _mm_adds_epu8(diff, mask251);
      __m128i added235 = _mm_adds_epu8(diff, mask235);
      __m128i cmp251 = _mm_cmpeq_epi8(added251, all_ff);
      __m128i cmp235 = _mm_cmpeq_epi8(added235, all_ff);
      __m128i tmp1 = _mm_and_si128(cmp251, onesMask);
      __m128i tmp2 = _mm_and_si128(cmp235, twosMask);
      __m128i tmp = _mm_or_si128(tmp1, tmp2);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x), tmp);
      prvp += prv_pitch;
      nxtp += nxt_pitch;
      dstp += dst_pitch;
    }
  }
}

// fixme: also in tfmasm
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
      if (with_luma_mask) { // YUY2 luma mask
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


// src_pitch2: src_pitch*2 for inline asm speed reasons
void check_combing_SSE2(const unsigned char *srcp, unsigned char *dstp, int width,
  int height, int src_pitch, int src_pitch2, int dst_pitch, __m128i threshb, __m128i thresh6w)
{
  // no luma masking
  check_combing_SSE2_generic_simd<false>(srcp, dstp, width, height, src_pitch, src_pitch2, dst_pitch, threshb, thresh6w);
}

void check_combing_SSE2_Luma(const unsigned char *srcp, unsigned char *dstp, int width,
  int height, int src_pitch, int src_pitch2, int dst_pitch, __m128i threshb, __m128i thresh6w)
{
  // with luma masking
  check_combing_SSE2_generic_simd<true>(srcp, dstp, width, height, src_pitch, src_pitch2, dst_pitch, threshb, thresh6w);
}

void check_combing_SSE2_M1(const unsigned char *srcp, unsigned char *dstp,
  int width, int height, int src_pitch, int dst_pitch, __m128i thresh)
{
  __m128i zero = _mm_setzero_si128();
  __m128i lumaMask = _mm_set1_epi16(0x00FF);

  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto next = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch + x));
      auto curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      auto prev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch + x));

      auto prev_lo = _mm_unpacklo_epi8(prev, zero);
      auto prev_hi = _mm_unpackhi_epi8(prev, zero);
      auto curr_lo = _mm_unpacklo_epi8(curr, zero);
      auto curr_hi = _mm_unpackhi_epi8(curr, zero);
      auto next_lo = _mm_unpacklo_epi8(next, zero);
      auto next_hi = _mm_unpackhi_epi8(next, zero);

      auto diff_prev_curr_lo = _mm_subs_epi16(prev_lo, curr_lo);
      auto diff_next_curr_lo = _mm_subs_epi16(next_lo, curr_lo);
      auto diff_prev_curr_hi = _mm_subs_epi16(prev_hi, curr_hi);
      auto diff_next_curr_hi = _mm_subs_epi16(next_hi, curr_hi);

      // -- lo
      auto diff_prev_curr_lo_lo = _mm_unpacklo_epi16(diff_prev_curr_lo, zero);
      auto diff_prev_curr_lo_hi = _mm_unpackhi_epi16(diff_prev_curr_lo, zero);
      auto diff_next_curr_lo_lo = _mm_unpacklo_epi16(diff_next_curr_lo, zero);
      auto diff_next_curr_lo_hi = _mm_unpackhi_epi16(diff_next_curr_lo, zero);

      auto res_lo_lo = _mm_madd_epi16(diff_prev_curr_lo_lo, diff_next_curr_lo_lo);
      auto res_lo_hi = _mm_madd_epi16(diff_prev_curr_lo_hi, diff_next_curr_lo_hi);

      // -- hi
      auto diff_prev_curr_hi_lo = _mm_unpacklo_epi16(diff_prev_curr_hi, zero);
      auto diff_prev_curr_hi_hi = _mm_unpackhi_epi16(diff_prev_curr_hi, zero);
      auto diff_next_curr_hi_lo = _mm_unpacklo_epi16(diff_next_curr_hi, zero);
      auto diff_next_curr_hi_hi = _mm_unpackhi_epi16(diff_next_curr_hi, zero);

      auto res_hi_lo = _mm_madd_epi16(diff_prev_curr_hi_lo, diff_next_curr_hi_lo);
      auto res_hi_hi = _mm_madd_epi16(diff_prev_curr_hi_hi, diff_next_curr_hi_hi);

      auto cmp_lo_lo = _mm_cmpgt_epi32(res_lo_lo, thresh);
      auto cmp_lo_hi = _mm_cmpgt_epi32(res_lo_hi, thresh);
      auto cmp_hi_lo = _mm_cmpgt_epi32(res_hi_lo, thresh);
      auto cmp_hi_hi = _mm_cmpgt_epi32(res_hi_hi, thresh);

      auto cmp_lo = _mm_packs_epi32(cmp_lo_lo, cmp_lo_hi);
      auto cmp_hi = _mm_packs_epi32(cmp_hi_lo, cmp_hi_hi);
      auto cmp_lo_masked = _mm_and_si128(cmp_lo, lumaMask);
      auto cmp_hi_masked = _mm_and_si128(cmp_hi, lumaMask);

      auto res = _mm_packus_epi16(cmp_lo_masked, cmp_hi_masked);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), res);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }

}


void check_combing_SSE2_Luma_M1(const unsigned char *srcp, unsigned char *dstp,
  int width, int height, int src_pitch, int dst_pitch, __m128i thresh)
{
  __m128i lumaMask = _mm_set1_epi16(0x00FF);
  __m128i zero = _mm_setzero_si128();
  while (height--) {
    for (int x = 0; x < width; x += 16) {
      auto next = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + src_pitch + x));
      auto curr = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x));
      auto prev = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp - src_pitch + x));
      
      next = _mm_and_si128(next, lumaMask);
      curr = _mm_and_si128(curr, lumaMask);
      prev = _mm_and_si128(prev, lumaMask);

      auto diff_prev_curr = _mm_subs_epi16(prev, curr);
      auto diff_next_curr = _mm_subs_epi16(next, curr);

      auto diff_prev_curr_lo = _mm_unpacklo_epi16(diff_prev_curr, zero);
      auto diff_prev_curr_hi = _mm_unpackhi_epi16(diff_prev_curr, zero);
      auto diff_next_curr_lo = _mm_unpacklo_epi16(diff_next_curr, zero);
      auto diff_next_curr_hi = _mm_unpackhi_epi16(diff_next_curr, zero);

      auto res_lo = _mm_madd_epi16(diff_prev_curr_lo, diff_next_curr_lo);
      auto res_hi = _mm_madd_epi16(diff_prev_curr_hi, diff_next_curr_hi);

      auto cmp_lo = _mm_cmpgt_epi32(res_lo, thresh);
      auto cmp_hi = _mm_cmpgt_epi32(res_hi, thresh);

      auto cmp = _mm_packs_epi32(cmp_lo, cmp_hi);
      auto cmp_masked = _mm_and_si128(cmp, lumaMask);

      _mm_store_si128(reinterpret_cast<__m128i *>(dstp + x), cmp_masked);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

template<int blockSizeY>
void compute_sum_8xN_sse2(const unsigned char *srcp, int pitch, int &sum)
{
  // sums masks
  // if (cmkppT[x + v] == 0xFF && cmkpT[x + v] == 0xFF && cmkpnT[x + v] == 0xFF) sum++;
  // scrp is prev
  auto onesMask = _mm_set1_epi8(1);
  auto all_ff = _mm_set1_epi8(-1);
  auto prev = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp));
  auto curr = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp + pitch));
  auto summa = _mm_setzero_si128();
  srcp += pitch * 2; // points to next
  // unroll 2
  for (int i = 0; i < blockSizeY / 2; i++) { // 4x2=8
    /*
    p  #
    c  # #
    n  # #
    nn   #
    */
    auto next = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp));
    auto nextnext = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(srcp + pitch));

    auto anded_common = _mm_and_si128(curr, next);
    auto with_prev = _mm_and_si128(prev, anded_common);
    auto with_nextnext = _mm_and_si128(anded_common, nextnext);

    // these were missing from the original assembler code (== 0xFF)
    with_prev = _mm_cmpeq_epi8(with_prev, all_ff);
    with_nextnext = _mm_cmpeq_epi8(with_nextnext, all_ff);

    with_prev = _mm_and_si128(with_prev, onesMask);
    with_nextnext = _mm_and_si128(with_nextnext, onesMask);

    prev = next;
    curr = nextnext;

    summa = _mm_adds_epu8(summa, with_prev);
    summa = _mm_adds_epu8(summa, with_nextnext);
    srcp += pitch * 2;
  }
  // now we have to sum up lower 8 bytes
  // in sse2, we use sad
  auto zero = _mm_setzero_si128();
  auto tmpsum = _mm_sad_epu8(summa, zero);  // sum(lo 8 bytes)(needed) / sum(hi 8 bytes)(not needed)
  sum = _mm_cvtsi128_si32(tmpsum);
}

// instantiate for 8x8
template void compute_sum_8xN_sse2<8>(const unsigned char* srcp, int pitch, int& sum);

// YUY2 luma only case
void compute_sum_16x8_sse2_luma(const unsigned char *srcp, int pitch, int &sum)
{
  // sums masks
  // if (cmkppT[x + v] == 0xFF && cmkpT[x + v] == 0xFF && cmkpnT[x + v] == 0xFF) sum++;
  // scrp is prev
  auto onesMask = _mm_set1_epi16(0x0001); // ones where luma
  auto all_ff = _mm_set1_epi8(-1);
  auto prev = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp));
  auto curr = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + pitch));
  auto summa = _mm_setzero_si128();
  srcp += pitch * 2; // points to next
  for (int i = 0; i < 4; i++) { // 4x2=8
    /*
    p  #
    c  # #
    n  # #
    nn   #
    */
    auto next = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp));
    auto nextnext = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + pitch));

    auto anded_common = _mm_and_si128(curr, next);
    auto with_prev = _mm_and_si128(prev, anded_common);
    auto with_nextnext = _mm_and_si128(anded_common, nextnext);

    // these were missing from the original assembler code (== 0xFF)
    with_prev = _mm_cmpeq_epi8(with_prev, all_ff);
    with_nextnext = _mm_cmpeq_epi8(with_nextnext, all_ff);

    with_prev = _mm_and_si128(with_prev, onesMask);
    with_nextnext = _mm_and_si128(with_nextnext, onesMask);

    prev = next;
    curr = nextnext;

    summa = _mm_adds_epu8(summa, with_prev);
    summa = _mm_adds_epu8(summa, with_nextnext);
    srcp += pitch * 2;
  }

  // now we have to sum up lower and upper 8 bytes
  // in sse2, we use sad
  auto zero = _mm_setzero_si128();
  auto tmpsum = _mm_sad_epu8(summa, zero);  // sum(lo 8 bytes) / sum(hi 8 bytes)
  tmpsum = _mm_add_epi32(tmpsum, _mm_srli_si128(tmpsum, 8)); // lo + hi
  sum = _mm_cvtsi128_si32(tmpsum);
}


