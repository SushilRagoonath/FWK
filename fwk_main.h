// -----------------------------------------------------------------------------
// config directives
// debug /O0 /D3 > debugopt /O1 /D2 > release (+ndebug) /O2 /D1 > final (+ndebug+final) /O3 /D0

#ifdef FINAL
#define WITH_COOKER          0
#define WITH_FASTCALL_LUA    1
#define WITH_LEAK_DETECTOR   0
#define WITH_PROFILE         0
#define WITH_XREALLOC_POISON 0
#define WITH_VIDEO_YCBCR     1
#else
#define WITH_COOKER          1
#define WITH_FASTCALL_LUA    0
#define WITH_LEAK_DETECTOR   0
#define WITH_PROFILE         1
#define WITH_XREALLOC_POISON 1
#define WITH_VIDEO_YCBCR     1
#endif

//#define WITH_ASSIMP        0      // Only art/tools/ass2iqe.c would define this.
#define WITH_COMPRESSOR      ULZ|0  // Use COMPRESSOR|LEVEL[0..15], where compressor is one of:
                                    // LZMA,DEFL,LZ4X,ULZ,BCM,CRSH,BALZ,LZW3,LZSS,LZP1,PPP,RAW

// -----------------------------------------------------------------------------
// system headers

#ifndef _GNU_SOURCE
#define _GNU_SOURCE   // for linux
#endif

#ifdef _MSC_VER
#include <omp.h>      // compile with /openmp to speed up some computations
#endif
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// stl, forward includes

#define array(t) t*

#ifdef _MSC_VER
#define __thread         __declspec(thread)
#elif defined __TINYC__ && defined _WIN32
#define __thread         __declspec(thread) // compiles fine, but does not work apparently
#elif defined __TINYC__
#define __thread
#endif

// util for sanity checks (static_assert)

//#define STATIC_ASSERT_2(COND, LINE) typedef int static_assert_on_line_##LINE[ !!(COND) ]
#define STATIC_ASSERT_2(COND, LINE) typedef struct { unsigned static_assert_on_line_##LINE : !!(COND); } static_assert_on_line_##LINE
#define STATIC_ASSERT_1(COND, LINE) STATIC_ASSERT_2(COND, LINE)
#define STATIC_ASSERT(COND) STATIC_ASSERT_1(COND, __LINE__)

//#define STATIC_ASSERT(exp) typedef char UNIQUE_NAME(_static_assert_on_line)[(exp)?1:-1]

// pragma libs

#if defined _WIN32 && (defined _MSC_VER || defined __TINYC__)
#pragma comment(lib, "advapi32")
#pragma comment(lib, "dbghelp")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "user32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")
#endif

#if defined __linux__ && defined __TINYC__
#pragma comment(lib, "dl")
#pragma comment(lib, "m")
#pragma comment(lib, "pthread")
#endif
