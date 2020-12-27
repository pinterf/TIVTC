#ifndef __Internal_H__
#define __Internal_H__

#include "avisynth.h"
#include "avs/config.h"

#ifndef _WIN32
#define OutputDebugString(x)
#endif
#if defined(GCC)
#include <stdlib.h>
#define _aligned_malloc(size, alignment) aligned_alloc(alignment, size)
#define _aligned_free(ptr) free(ptr)
#endif
#ifndef _WIN32
#include <stdio.h>
#ifdef AVS_POSIX
#include <linux/limits.h>
#endif
#endif

// these settings control whether the included code comes from old asm or newer simd/C rewrites
#define USE_C_NO_ASM
// USE_C_NO_ASM: inline non-simd asm

#endif  // __Internal_H__