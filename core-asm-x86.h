/*
 * Copyright (C) 2023      Colin Ian King.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#ifndef CORE_ASM_X86_H
#define CORE_ASM_X86_H

#include "stress-ng.h"
#include "core-arch.h"

#if defined(STRESS_ARCH_X86)
#define stress_asm_x86_lock_add(ptr, inc) 		\
	do {						\
		__asm__ __volatile__(			\
			"lock addl %1,%0"		\
			: "+m" (*ptr)			\
			: "ir" (inc));			\
	} while (0)

#if defined(STRESS_ARCH_X86_32) && !NEED_GNUC(5, 0, 0) && defined(__PIC__)
#define stress_asm_x86_cpuid(a, b, c, d)		\
	do {						\
		__asm__ __volatile__ (			\
			"pushl %%ebx\n"			\
			"cpuid\n"			\
			"mov %%ebx,%1\n"		\
			"popl %%ebx\n"			\
			: "=a"(a),			\
			  "=r"(b),			\
			  "=r"(c),			\
			  "=d"(d)			\
			: "0"(a),"2"(c));		\
	} while (0)
#else
#define stress_asm_x86_cpuid(a, b, c, d)		\
	do {						\
		__asm__ __volatile__ (			\
			"cpuid\n"			\
			: "=a"(a),			\
			  "=b"(b),			\
			  "=c"(c),			\
			  "=d"(d)			\
			: "0"(a),"2"(c));		\
	} while (0)
#endif

#if defined(HAVE_ASM_X86_PAUSE)
static inline void stress_asm_x86_pause(void)
{
	__asm__ __volatile__("pause;\n" ::: "memory");
}
#endif

#if defined(HAVE_ASM_X86_SERIALIZE)
static inline void stress_asm_x86_serialize(void)
{
	__asm__ __volatile__("serialize");
}
#endif

/*
 *  x86 read TSC
 */
static inline uint64_t stress_asm_x86_rdtsc(void)
{
#if defined(STRESS_ARCH_X86_64)
	uint32_t lo, hi;

	__asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)hi << 32) | lo;
#elif defined(STRESS_ARCH_X86_32)
	uint64_t tsc;

	__asm__ __volatile__("rdtsc" : "=A" (tsc));
	return tsc;
#else
	return 0;
#endif
}

#if defined(STRESS_ARCH_X86_64) &&	\
    defined(HAVE_ASM_X86_RDRAND)
/*
 *  stress_asm_x86_rdrand()
 *	read 64 bit random value
 */
static inline uint64_t stress_asm_x86_rdrand(void)
{
	uint64_t        ret;

	__asm__ __volatile__(
	"1:;\n\
		rdrand %0;\n\
		jnc 1b;\n"
	: "=r"(ret));

	return ret;
}
#elif defined(STRESS_ARCH_X86_32) &&	\
      defined(HAVE_ASM_X86_RDRAND)
/*
 *  stress_asm_x86_rdrand()
 *	read 2 x 32 bit random value
 */
static inline uint64_t stress_asm_x86_rdrand(void)
{
	uint32_t ret;
	uint64_t ret64;

	__asm__ __volatile__("1:;\n\
	rdrand %0;\n\
	jnc 1b;\n":"=r"(ret));

	ret64 = (uint64_t)ret << 32;

	__asm__ __volatile__("1:;\n\
	rdrand %0;\n\
	jnc 1b;\n":"=r"(ret));

	return ret64 | ret;
}
#endif

#if defined(STRESS_ARCH_X86_64) &&	\
    defined(HAVE_ASM_X86_RDSEED)
/*
 *  stress_asm_x86_rdseed()
 *	read 64 bit random value
 */
static inline uint64_t stress_asm_x86_rdseed(void)
{
	uint64_t        ret;

	__asm__ __volatile__(
	"1:;\n\
		rdseed %0;\n\
		jnc 1b;\n"
	: "=r"(ret));

	return ret;
}
#elif defined(STRESS_ARCH_X86_32) &&	\
      defined(HAVE_ASM_X86_RDSEED)
/*
 *  stress_asm_x86_rdseed()
 *	read 2 x 32 bit random value
 */
static inline uint64_t stress_asm_x86_rdseed(void)
{
	uint32_t ret;
	uint64_t ret64;

	__asm__ __volatile__(
	"1:;\n\
		rdseed %0;\n\
		jnc 1b;\n"
	: "=r"(ret));

	ret64 = (uint64_t)ret << 32;

	__asm__ __volatile__(
	"1:;\n\
		rdseed %0;\n\
		jnc 1b;\n"
	: "=r"(ret));

	return ret64 | ret;
}
#endif

/* #if defined(STRESS_ARCH_X86) */
#endif

#if defined(HAVE_ASM_X86_TPAUSE) && 	\
    !defined(__PCC__)
static inline int stress_asm_x86_tpause__(int state, uint32_t hi, uint32_t lo)
{
	uint8_t cflags;

	asm volatile(
		"mov %1, %%edx;\n"
		"mov %2, %%eax;\n"
		"mov %3, %%edi;\n"
		".byte 0x66,0x0f,0xae,0xf7;\n"	/* tpause %%edi; */
		"setb %0;\n"
		: "=r" (cflags)
		: "r" (hi), "r" (lo), "r"(state)
		: "cc", "eax", "edx", "edi");
	return cflags;
}

static inline int stress_asm_x86_tpause(const int state, const uint64_t delay)
{
	register uint32_t lo = delay & 0xffffffff;
	register uint32_t hi = (uint32_t)(delay >> 32);

	return stress_asm_x86_tpause__(state, hi, lo);
}
#endif

#if defined(HAVE_ASM_X86_CLFLUSH)
static inline void stress_asm_x86_clflush(volatile void *p)
{
        __asm__ __volatile__("clflush (%0)\n" : : "r"(p) : "memory");
}
#endif

#if defined(HAVE_ASM_X86_CLFLUSHOPT)
static inline void stress_asm_x86_clflushopt(void *p)
{
        __asm__ __volatile__("clflushopt (%0)\n" : : "r"(p) : "memory");
}
#endif

#if defined(HAVE_ASM_X86_CLDEMOTE)
static inline void stress_asm_x86_cldemote(void *p)
{
        __asm__ __volatile__("cldemote (%0)\n" : : "r"(p) : "memory");
}
#endif

#if defined(HAVE_ASM_X86_CLWB)
static inline void stress_asm_x86_clwb(void *p)
{
        __asm__ __volatile__("clwb (%0)\n" : : "r"(p) : "memory");
}
#endif

#if defined(HAVE_ASM_X86_MFENCE)
static inline void stress_asm_x86_mfence(void)
{
	__asm__ __volatile__("mfence" : : : "memory");
}
#endif

#if !defined(__PCC__)
static inline int stress_asm_x86_umwait__(int state, uint32_t hi, uint32_t lo)
{
	uint8_t cflags;

	asm volatile(
		"mov %1, %%edx;\n"
		"mov %2, %%eax;\n"
		"mov %3, %%edi;\n"
		".byte 0xf2, 0x0f, 0xae, 0xf7;\n"	/* umwait %edi */
		"setb %0;\n"
		: "=r" (cflags)
		: "r" (hi), "r" (lo), "r"(state)
		: "cc", "eax", "edx", "edi");
	return cflags;
}

static inline int stress_asm_x86_umwait(const int state, const uint64_t delay)
{
	register uint32_t lo = delay & 0xffffffff;
	register uint32_t hi = (uint32_t)(delay >> 32);

	return stress_asm_x86_umwait__(state, hi, lo);
}

static inline void stress_asm_x86_umonitor(void *addr)
{
	asm volatile("mov %0, %%rdi\t\n"
		     ".byte 0xf3, 0x0f, 0xae, 0xf7\t\n"
		     : : "r" (addr));
}
#endif

/* #ifndef CORE_ASM_X86_H */
#endif
