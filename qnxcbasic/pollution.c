/*
 * Copyright 2014, General Dynamics C4 Systems
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Modified by Jasper Di Francesco - 22 November 2023
 */

#include <pollution.h>
#include <stdlib.h>
#include <threads.h>
#include <sched.h>
#include <stdint.h>
#include <sys/neutrino.h>

#define mylog()

typedef uint64_t word_t;
typedef word_t bool_t;

static inline void dsb(void)
{
    mylog();
    asm volatile("dsb sy" ::: "memory");
}

static inline void dmb(void)
{
    mylog();
    asm volatile("dmb sy" ::: "memory");
}

static inline void isb(void)
{
    mylog();
    asm volatile("isb" ::: "memory");
}

#define MRS(reg, v) asm volatile("mrs %x0," reg : "=r"(v))
#define MSR(reg, v)                                \
    do                                             \
    {                                              \
        word_t _v = v;                             \
        asm volatile("msr " reg ",%x0" ::"r"(_v)); \
    } while (0)

static inline void cleanByWSL(word_t wsl)
{
    mylog();
    asm volatile("dc csw, %0" : : "r"(wsl));
}

static inline void cleanInvalidateByWSL(word_t wsl)
{
    mylog();
    asm volatile("dc cisw, %0" : : "r"(wsl));
}

#define CLZL(x) __builtin_clzl(x)

static inline long
clzl(unsigned long x)
{
    mylog();
    return CLZL(x);
}

static inline void invalidate_I_PoU(void)
{
    mylog();
    asm volatile("ic iallu");
    isb();
}

static inline word_t readCacheSize(int level, bool_t instruction)
{
    mylog();
    word_t size, csselr_old;
    /* Save CSSELR */
    MRS("csselr_el1", csselr_old);
    /* Select cache level */
    MSR("csselr_el1", ((level << 1) | instruction));
    /* Read 'size' */
    MRS("ccsidr_el1", size);
    /* Restore CSSELR */
    MSR("csselr_el1", csselr_old);
    return size;
}

static inline word_t readCLID(void)
{
    mylog();
    word_t CLID;
    MRS("clidr_el1", CLID);
    return CLID;
}

#define BIT(n) (UL_CONST(1) << (n))
#define MASK(n) (BIT(n) - UL_CONST(1))
#define LOC(x) (((x) >> 24) & MASK(3))
#define LINEBITS(s) (((s) & MASK(3)) + 4)
#define ASSOC(s) ((((s) >> 3) & MASK(10)) + 1)
#define NSETS(s) ((((s) >> 13) & MASK(15)) + 1)
#define wordRadix 6
#define wordBits (1 << wordRadix)

#define PASTE(a, b) a##b
#define UL_CONST(x) PASTE(x, ul)
#define LOUU(x) (((x) >> 27) & MASK(3))

enum arm_cache_type
{
    ARMCacheI = 1,
    ARMCacheD = 2,
    ARMCacheID = 3,
};

#define CTYPE(x, n) (((x) >> (n * 3)) & MASK(3))

static inline void cleanInvalidate_D_by_level(int l)
{
    mylog();
    word_t lsize = readCacheSize(l, 0);
    int lbits = LINEBITS(lsize);
    int assoc = ASSOC(lsize);
    int assoc_bits = wordBits - clzl(assoc - 1);
    int nsets = NSETS(lsize);

    for (int w = 0; w < assoc; w++)
    {
        for (int s = 0; s < nsets; s++)
        {
            cleanInvalidateByWSL((w << (32 - assoc_bits)) |
                                 (s << lbits) | (l << 1));
        }
    }
}

void clean_D_PoU(void)
{
    mylog();
    int clid = readCLID();
    int lou = LOUU(clid);

    for (int l = 0; l < lou; l++)
    {
        if (CTYPE(clid, l) > ARMCacheI)
        {
            word_t lsize = readCacheSize(l, 0);
            int lbits = LINEBITS(lsize);
            int assoc = ASSOC(lsize);
            int assoc_bits = wordBits - clzl(assoc - 1);
            int nsets = NSETS(lsize);
            for (int w = 0; w < assoc; w++)
            {
                for (int s = 0; s < nsets; s++)
                {
                    cleanByWSL((w << (32 - assoc_bits)) |
                               (s << lbits) | (l << 1));
                }
            }
        }
    }
}

void cleanCaches_PoU(void)
{
    mylog();
    dsb();
    clean_D_PoU();
    dsb();
    invalidate_I_PoU();
    dsb();
}

void cleanInvalidate_D_PoC(void)
{
    mylog();
    int clid = readCLID();
    int loc = LOC(clid);

    for (int l = 0; l < loc; l++)
    {
        if (CTYPE(clid, l) > ARMCacheI)
        {
            cleanInvalidate_D_by_level(l);
        }
    }
}

void cleanInvalidateL1Caches(void)
{
    mylog();
    dsb();
    cleanInvalidate_D_PoC();
    dsb();
    invalidate_I_PoU();
    dsb();
}

void arch_clean_invalidate_caches(void)
{
    mylog();
    ThreadCtl(_NTO_TCTL_IO_LEVEL, (void *)_NTO_IO_LEVEL_2);
    cleanCaches_PoU();
    cleanInvalidateL1Caches();
    isb();
    ThreadCtl(_NTO_TCTL_IO_LEVEL, (void *)_NTO_IO_LEVEL_NONE);
}

#define CONFIG_L1_DCACHE_SIZE 0x8000
#define CONFIG_L1_CACHE_LINE_SIZE_BITS 6

#define L1_CACHE_LINE_SIZE BIT(CONFIG_L1_CACHE_LINE_SIZE_BITS)
#define POLLUTE_ARRAY_SIZE CONFIG_L1_DCACHE_SIZE / L1_CACHE_LINE_SIZE / sizeof(int)
#define POLLUTE_RUNS 5
#define ALIGN(n) __attribute__((__aligned__(n)))

#define COMPILER_MEMORY_FENCE() \
    dsb();                      \
    isb();

void pollute_dcache(void)
{
    ALIGN(L1_CACHE_LINE_SIZE)
    volatile int pollute_array[POLLUTE_ARRAY_SIZE][L1_CACHE_LINE_SIZE] = {0};
    for (int i = 0; i < POLLUTE_RUNS; i++)
    {
        for (int j = 0; j < L1_CACHE_LINE_SIZE; j++)
        {
            for (int k = 0; k < POLLUTE_ARRAY_SIZE; k++)
            {
                pollute_array[k][j]++;
            }
        }
    }
}
void pollute_icache(void)
{
    __asm__(".rept 64000\n\t"
            "nop\n\t"
            ".endr");
}

void use_fpu_func(void)
{
    float nums[5] = {15.4, 293.2235, 9.1094128, 10.94128, 1.1094128};
    for (int i = 0; i < 5; i++)
    {
        nums[i] = nums[i] * 2.0;
    }
}

void invalidate_tlb(void)
{
    ThreadCtl(_NTO_TCTL_IO_LEVEL, (void *)_NTO_IO_LEVEL_2);
    __asm__ volatile("tlbi vmalle1");
    ThreadCtl(_NTO_TCTL_IO_LEVEL, (void *)_NTO_IO_LEVEL_NONE);
}

void real_cache_func(void)
{
    use_fpu_func();
    COMPILER_MEMORY_FENCE();
    invalidate_tlb();
    COMPILER_MEMORY_FENCE();
    arch_clean_invalidate_caches();
    COMPILER_MEMORY_FENCE();
    pollute_dcache();
    pollute_icache();
    COMPILER_MEMORY_FENCE();
}
