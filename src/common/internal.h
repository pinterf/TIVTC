#ifndef __Internal_H__
#define __Internal_H__

#include "avisynth.h"
#include "avs/config.h"
#include <cstring>

#include <stdexcept>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _WIN32
#define OutputDebugString(x)
#endif

#if (defined(GCC) || defined(CLANG)) && !defined(_WIN32)
#include <stdlib.h>
//#define _aligned_malloc(size, alignment) aligned_alloc(alignment, size)
//#define _aligned_free(ptr) free(ptr)
// when defined as above, we cannot pass the address of _aligned_free to unique_ptr's custom deleter
#define _aligned_malloc aligned_alloc
#define _aligned_free free
#endif
#ifndef _WIN32
#include <stdio.h>
#ifdef AVS_POSIX
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 1
#endif
#include <limits.h>
#endif
#endif

// these settings control whether the included code comes from old asm or newer simd/C rewrites
#define USE_C_NO_ASM
// USE_C_NO_ASM: inline non-simd asm

class TIVTCError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

constexpr int ISP = 0x00000000; // p
constexpr int ISC = 0x00000001; // c
constexpr int ISN = 0x00000002; // n
constexpr int ISB = 0x00000003; // b
constexpr int ISU = 0x00000004; // u
constexpr int ISDB = 0x00000005; // l = (deinterlaced c bottom field)
constexpr int ISDT = 0x00000006; // h = (deinterlaced c top field)

#define MTC(n) n == 0 ? 'p' : n == 1 ? 'c' : n == 2 ? 'n' : n == 3 ? 'b' : n == 4 ? 'u' : \
               n == 5 ? 'l' : n == 6 ? 'h' : 'x'

constexpr int TOP_FIELD = 0x00000008;
constexpr int COMBED = 0x00000010;
constexpr int D2VFILM = 0x00000020;

constexpr int FILE_COMBED = 0x00000030;
constexpr int FILE_NOTCOMBED = 0x00000020;
constexpr int FILE_ENTRY = 0x00000080;
constexpr int FILE_D2V = 0x00000008;
constexpr int D2VARRAY_DUP_MASK = 0x03;
constexpr int D2VARRAY_MATCH_MASK = 0x3C;

constexpr int DROP_FRAME = 0x00000001; // ovr array - bit 1
constexpr int KEEP_FRAME = 0x00000002; // ovr array - 2
constexpr int FILM = 0x00000004; // ovr array - bit 3
constexpr int VIDEO = 0x00000008; // ovr array - bit 4
constexpr int ISMATCH = 0x00000070; // ovr array - bits 5-7
constexpr int ISD2VFILM = 0x00000080; // ovr array - bit 8

#define cfps(n) n == 1 ? "119.880120" : n == 2 ? "59.940060" : n == 3 ? "39.960040" : \
				n == 4 ? "29.970030" : n == 5 ? "23.976024" : "unknown"

constexpr uint32_t MAGIC_NUMBER = 0xdeadfeed;
constexpr uint32_t MAGIC_NUMBER_2 = 0xdeadbeef;
constexpr uint32_t MAGIC_NUMBER_DEADFEED = 0xdeadfeed; // TIVTC
constexpr uint32_t MAGIC_NUMBER_2_DEADBEEF = 0xdeadbeef; // Decomb or DGDecode
constexpr uint32_t MAGIC_NUMBER_3_DEADDEED = 0xdeaddeed;
constexpr uint32_t MAGIC_NUMBER_4_DEADBEAD = 0xdeadbead;

FILE *tivtc_fopen(const char *name, const char *mode);
void BitBlt(uint8_t* dstp, int dst_pitch, const uint8_t* srcp, int src_pitch, int row_size, int height);

// Frame properties set by TFM:
// #define PROP_TFMDisplay "TFMDisplay"
#define PROP_TFMMATCH "TFMMatch"
#define PROP_TFMMics "TFMMics"
#define PROP_Combed "_Combed"
#define PROP_TFMD2VFilm "TFMD2VFilm"
#define PROP_TFMField "TFMField"
#define PROP_TFMPP "TFMPP"

// Frame properties set by TDecimate:
// #define PROP_TDecimateDisplay "TDecimateDisplay"
#define PROP_TDecimateCycleStart "TDecimateCycleStart"
#define PROP_TDecimateCycleMaxBlockDiff "TDecimateCycleMaxBlockDiff" // uint64_t[]
#define PROP_TDecimateOriginalFrame "TDecimateOriginalFrame"
#define PROP_DurationNum "_DurationNum"
#define PROP_DurationDen "_DurationDen"

/* converts an int64 to int with saturation, useful to silence warnings when reading int properties among other things */
static inline int int64ToIntS(int64_t i) {
  if (i > INT_MAX)
    return INT_MAX;
  else if (i < INT_MIN)
    return INT_MIN;
  else return (int)i;
}

#endif  // __Internal_H__