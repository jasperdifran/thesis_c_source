/*
 * ipcBench.h
 *
 *  Created on: 7 Oct 2023
 *      Author: Jasper Di Francesco
 */

#ifndef IPCBENCH_H_
#define IPCBENCH_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/siginfo.h>
#include <signal.h>
#include <sched.h>

#define RUNS 100



#define START 0
#define END 1

#define SEND 0
#define REPLY 1

#define CACHE_DIRTIED 1
#define HOT_CACHE 0



// Top 8 bits as flags
#define POLLUTE_CACHE 0

#define SET_FLAG(flags, flag) (flags | (1 << flag))
#define IS_FLAG_SET(flags, flag) (flags & (1 << flag))

#define ADD_FLAGS_TO_64PTR(ptr, flags) ((uint64_t)ptr | (flags << 56))
#define GET_FLAGS_FROM_64PTR(ptr) ((uint64_t)ptr >> 56)

int ipcBenchmark();
void printIpcResults();


#endif /* IPCBENCH_H_ */
