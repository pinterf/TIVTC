#ifndef __Internal_H__
#define __Internal_H__

#include "avisynth.h"

// these settings control whether the included code comes from old asm or newer simd/C rewrites

#ifdef _M_X64
#define USE_INTR
#undef ALLOW_MMX
#define USE_C_NO_ASM
#else
#define USE_INTR
//#define ALLOW_MMX
#define USE_C_NO_ASM
// USE_C_NO_ASM: inline non-simd asm in tfm.cpp, tfmyuy2.cpp and tfmyv12.cpp
// C seems to be faster
#endif

#endif  // __Internal_H__
