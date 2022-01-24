/** @file
 *
 *  Contains various convenience macros for detecting the compiler (xxx_COMPILER),
 *  the operating system (xxx_OS), the processor (xxx_CPU), and the language (C<digits>).
 *
 *  The language macros are defined if the feature exists, and undefined otherwise.
 *  All other macros are always defined, just to 1 if the feature exists and 0 if not.
 *
 *  Compilers detected:
 *     - GCC
 *     - Clang
 *     - Intel C++ compiler
 *     - MSVC
 *
 *  Operating systems detected:
 *     - Android
 *     - iOS
 *     - MacOS
 *     - Windows
 *     - Linux
 *     - BSD (FreeBSD, OpenBSD, DragonflyBSD, NetBSD)
 *     - DOS
 *
 *  Convenience groupings for operating systems include:
 *     - POSIX_OS - Defined to 1 if the operating system provides POSIX-compliant implementations of system calls.
 *     - IS_MOBILE_OS - Defined to 1 if the operating system is designed for mobile devices (i.e. iOS or Android, especially).
 *
 *  Processors detected:
 *     - Alpha
 *     - AMD64 (x86-64)
 *     - ARM
 *     - ARM64
 *     - Blackfin
 *     - Convex
 *     - Epiphany
 *     - HPPA RISC
 *     - x86 (x86-32)
 *     - Itanium
 *     - Motorola 68k
 *     - MIPS
 *     - PowerPC
 *     - Pyramid
 *     - RS6000
 *     - SPARC
 *     - SuperH
 *     - SystemZ
 *     - TMS470
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_ENVIRONMENT_H
#define SKATE_ENVIRONMENT_H

/* Compiler */
#ifdef __clang__
# define CLANG_COMPILER 1
#else
# define CLANG_COMPILER 0
#endif

#if defined(__GNUC__) && !CLANG_COMPILER
# define GCC_COMPILER 1
#else
# define GCC_COMPILER 0
#endif

#ifdef __INTEL_COMPILER
# define INTEL_COMPILER 1
#else
# define INTEL_COMPILER 0
#endif

#ifdef _MSC_VER
# define MSVC_COMPILER 1
#else
# define MSVC_COMPILER 0
#endif

#if __cplusplus >= 202002L
# define CONSTEXPR20 constexpr
#else
# define CONSTEXPR20
#endif

#if __cplusplus >= 201703L
# define CONSTEXPR17 constexpr
#else
# define CONSTEXPR17
#endif

#if __cplusplus >= 201402L
# define CONSTEXPR14 constexpr
#else
# define CONSTEXPR14
#endif

#if __cplusplus < 201103L
# error Skate requires a compiler that supports C++11
#endif

/* Operating system */
#if defined(macintosh) | defined(Macintosh)
# define IOS_SIMULATOR_OS 0
# define IOS_OS 0
# define MAC_OS 1
#elif defined(__APPLE__)
# include "TargetConditionals.h"
# if TARGET_IPHONE_SIMULATOR
#  define IOS_SIMULATOR_OS 1
#  define IOS_OS 1
#  define MAC_OS 0
# elif TARGET_OS_IPHONE
#  define IOS_SIMULATOR_OS 0
#  define IOS_OS 1
#  define MAC_OS 0
# elif TARGET_OS_MAC
#  define IOS_SIMULATOR_OS 0
#  define IOS_OS 0
#  define MAC_OS 1
# endif
#else
# define IOS_SIMULATOR_OS 0
# define IOS_OS 0
# define MAC_OS 0
#endif

#if defined(__FreeBSD__) | defined(__NetBSD__) | defined(__OpenBSD__) | defined(__Dragonfly__) | defined(__bsdi__)
# define BSD_OS 1
#else
# define BSD_OS 0
#endif

#if defined(__ANDROID__)
# define ANDROID_OS 1
#else
# define ANDROID_OS 0
#endif

#if !MAC_OS && !IOS_OS && !ANDROID_OS && !BSD_OS && (defined(__linux) | defined(__linux__) | defined(linux))
# define LINUX_OS 1
#else
# define LINUX_OS 0
#endif

#if defined(MSDOS) | defined(__MSDOS__) | defined(_MSDOS) | defined(__DOS__)
# define DOS_OS 1
#else
# define DOS_OS 0
#endif

#if defined(__WIN32) | defined(__WIN64) | defined(WIN32) | defined(WIN64) | defined(__WIN32__) | defined(_WIN16) | defined(_WIN32) | defined(_WIN64)
# define WINDOWS_OS 1
#else
# define WINDOWS_OS 0
#endif

#if MAC_OS | LINUX_OS | BSD_OS
# define POSIX_OS 1
#else
# define POSIX_OS 0
#endif

#if ANDROID_OS | IOS_OS
# define IS_MOBILE_OS 1
#else
# define IS_MOBILE_OS 0
#endif

/* CPU type */
#if (defined(__alpha__) | defined(__alpha) | defined(_M_ALPHA))
# define ALPHA_CPU 1
#else
# define ALPHA_CPU 0
#endif

#if (defined(__amd64__) | defined(__amd64) | defined(__x86_64__) | defined(__x86_64) | defined(_M_AMD64) | defined(_M_X64))
# define AMD64_CPU 1
#else
# define AMD64_CPU 0
#endif

#if (defined(__arm__) | defined(__thumb__) | defined(__TARGET_ARCH_ARM) | defined(__TARGET_ARCH_THUMB) | defined(_ARM) | defined(_M_ARM) | defined(_M_ARMT) | defined(__arm))
# define ARM_CPU 1
#else
# define ARM_CPU 0
#endif

#if (defined(__aarch64__))
# define ARM64_CPU 1
#else
# define ARM64_CPU 0
#endif

#if (defined(__bfin) | defined(__BFIN__))
# define BLACKFIN_CPU 1
#else
# define BLACKFIN_CPU 0
#endif

#if (defined(__convex__))
# define CONVEX_CPU 1
#else
# define CONVEX_CPU 0
#endif

#if (defined(__epiphany__))
# define EPIPHANY_CPU 1
#else
# define EPIPHANY_CPU 0
#endif

#if (defined(__hppa__) | defined(__HPPA__) | defined(__hppa))
# define HPPA_RISC_CPU 1
#else
# define HPPA_RISC_CPU 0
#endif

#if (defined(i386) | defined(__i386) | defined(__i386__) | defined(__i486__) | defined(__i586__) | defined(__i686__) | defined(__IA32__) | \
     defined(_M_I86) | defined(_M_IX86) | defined(__X86__) | defined(_X86_) | defined(__THW_INTEL__) | defined(__I86__) | defined(__INTEL__) | defined(__386))
# define X86_CPU 1
#else
# define X86_CPU 0
#endif

#if (defined(__ia64__) | defined(_IA64) | defined(__IA64__) | defined(__ia64) | defined(_M_IA64) | defined(__itanium__))
# define ITANIUM_CPU 1
#else
# define ITANIUM_CPU 0
#endif

#if (defined(__m68k__) | defined(M68000) | defined(__MC68K__))
# define M68K_CPU 1
#else
# define M68K_CPU 0
#endif

#if (defined(__mips__) | defined(mips) | defined(__mips) | defined(__MIPS__))
# define MIPS_CPU 1
#else
# define MIPS_CPU 0
#endif

#if (defined(__powerpc) | defined(__powerpc__) | defined(__powerpc64__) | defined(__POWERPC__) | defined(__ppc__) | defined(__ppc64__) | \
     defined(__PPC__) | defined(__PPC64__) | defined(_ARCH_PPC) | defined(_ARCH_PPC64) | defined(_M_PPC) | defined(_ARCH_PPC) | \
     defined(_ARCH_PPC64) | defined(__PPCGECKO__) | defined(__PPCBROADWAY__) | defined(_XENON) | defined(__ppc))
# define POWERPC_CPU 1
#else
# define POWERPC_CPU 0
#endif

#if (defined(pyr))
# define PYRAMID_CPU 1
#else
# define PYRAMID_CPU 0
#endif

#if (defined(__THW_RS6000) | defined(_IBMR2) | defined(_POWER) | defined(_ARCH_PWR) | defined(_ARCH_PWR2) | defined(_ARCH_PWR3) | defined(_ARCH_PWR4))
# define RS6000_CPU 1
#else
# define RS6000_CPU 0
#endif

#if (defined(__sparc__) | defined(__sparc) | defined(__sparc_v8__) | defined(__sparc_v9__) | defined(__sparc_v8) | defined(__sparc_v9))
# define SPARC_CPU 1
#else
# define SPARC_CPU 0
#endif

#if (defined(__sh__) | defined(__sh1__) | defined(__sh2__) | defined(__sh3__) | defined(__sh4__) | defined(__sh5__))
# define SUPERH_CPU 1
#else
# define SUPERH_CPU 0
#endif

#if (defined(__370__) | defined(__THW_370__) | defined(__s390__) | defined(__s390x__) | defined(__zarch__) | defined(__SYSC_ZARCH__))
# define SYSTEMZ_CPU 1
#else
# define SYSTEMZ_CPU 0
#endif

#if (defined(__TMS470__))
# define TMS470_CPU 1
#else
# define TMS470_CPU 0
#endif

/* Include processor intrinsics */
#if X86_CPU | AMD64_CPU
# if MSVC_COMPILER
#  include <intrin.h>
#  if AMD64_CPU
#   define __SSE2__ 1
#   define __SSE__ 1
#  elif _M_IX86_FP == 2
#   define __SSE2__ 1
#   define __SSE__ 1
#  elif _M_IX86_FP == 1
#   define __SSE__ 1
#  endif
#  ifdef __SSE2__
#   define __SSE3__ 1
#   define __SSSE3__ 1
#   define __SSE4_1__ 1
#   define __SSE4_2__ 1
#   define __AES__ 1
#   define __SHA__ 1
#  endif
# elif GCC_COMPILER | CLANG_COMPILER
#  include <cpuid.h>
#  include <x86intrin.h>
# endif
#elif ARM_CPU | ARM64_CPU
# include <sys/auxv.h>
# include <asm/hwcap.h>
#endif

#endif // SKATE_ENVIRONMENT_H
