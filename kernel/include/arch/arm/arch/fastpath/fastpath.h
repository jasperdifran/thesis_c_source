/*
 * Copyright 2014, General Dynamics C4 Systems
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */“

#pragma once

#include <linker.h>
#include <mode/fastpath/fastpath.h>
#include <benchmark/benchmark_track.h>
#include <arch/machine/debug.h>

void slowpath(syscall_t syscall)
NORETURN;

// #ifdef CONFIG_SIGNAL_FASTPATH
static inline
void fastpath_signal(word_t cptr, word_t msgInfo)
NORETURN;
// #endif

static inline
void fastpath_call(word_t cptr, word_t r_msgInfo)
NORETURN;

#ifdef CONFIG_EXCEPTION_FASTPATH
static inline
void fastpath_vm_fault(vm_fault_type_t type)
NORETURN;

void vm_fault_slowpath(vm_fault_type_t type)
NORETURN;
#endif

static inline
void fastpath_simple_call(word_t tcbDest, word_t r_msgInfo)
NORETURN;

static inline
void fastpath_simple_recv(word_t tcbDest, word_t r_msgInfo);

static inline
void fastpath_simple_send(word_t tcbDest, word_t r_msgInfo);

static inline
void fastpath_simple_reply_recv(word_t tcbDest, word_t r_msgInfo)
NORETURN;


static inline
#ifdef CONFIG_KERNEL_MCS
void fastpath_reply_recv(word_t cptr, word_t r_msgInfo, word_t reply)
#else
void fastpath_reply_recv(word_t cptr, word_t r_msgInfo)
#endif
NORETURN;


