/*
 * ipcBench.c
 *
 *  Created on: 7 Oct 2023
 *      Author: Jasper Di Francesco
 */

#include <ipcBench.h>

// Indexed CACHE -> SEND/REPLY -> START/END -> RUN
uint64_t ipc_benchmark_results[2][2][2][RUNS];

void client_ipc_thread_fn(void *arg)
{
    int conn_id;
    if ((conn_id = ConnectAttach(0, 0, (int)arg, _NTO_SIDE_CHANNEL, 0)) == -1)
    {
        printf("Failed to attach from within client\n");
        return;
    }

    int flags = GET_FLAGS_FROM_64PTR(arg);
    int cache = IS_FLAG_SET(flags, CACHE_DIRTIED) ? CACHE_DIRTIED : HOT_CACHE;

    char msg_out[] = "a";
    char msg_in[100];
    for (int i = 0; i < RUNS; i++)
    {
        if (IS_FLAG_SET(flags, POLLUTE_CACHE))
        {
            pollute_dcache();
            pollute_icache();
        }
        int start = ClockCycles();
        int err = MsgSend(conn_id, msg_out, 1, msg_in, 100);
        int end = ClockCycles();
        if (err)
        {
            printf("Failed MsgSend on iter %d\n", i);
        }
        ipc_benchmark_results[cache][SEND][START][i] = start;
        ipc_benchmark_results[cache][REPLY][END][i] = end;
    }
}

void server_ipc_thread_fn(void *arg)
{
    int chid = (int)arg;

    int conn_id;
    char msg_in[100];
    int flags = GET_FLAGS_FROM_64PTR(arg);
    int cache = IS_FLAG_SET(flags, CACHE_DIRTIED) ? CACHE_DIRTIED : HOT_CACHE;
    char msg_out[] = "b";
    for (int i = 0; i < RUNS; i++)
    {
        struct _msg_info info;
        int rcvid = MsgReceive(chid, msg_in, 100, &info);
        int end = ClockCycles();
        ipc_benchmark_results[cache][SEND][END][i] = end;
        if (IS_FLAG_SET(flags, POLLUTE_CACHE))
        {
            pollute_dcache();
            pollute_icache();
        }
        int start = ClockCycles();
        MsgReply(rcvid, 0, msg_out, 1);
        ipc_benchmark_results[cache][REPLY][START][i] = start;
    }
}

int ipcBenchmark()
{
    pthread_t server_thread;

    printf("Starting IPC...\n");

    uintptr_t chid = ChannelCreate(0);

    // First with a hot cache
    pthread_t server = pthread_create(&server_thread, NULL, server_ipc_thread_fn, (void *)chid);
    client_ipc_thread_fn((void *)chid);
    pthread_join(server, NULL);

    // Then with a dirty cache
    server = pthread_create(&server_thread, NULL, server_ipc_thread_fn, ADD_FLAGS_TO_64PTR(chid, SET_FLAG((uint64_t)0, CACHE_DIRTIED)));
    client_ipc_thread_fn(ADD_FLAGS_TO_64PTR(chid, SET_FLAG((uint64_t)0, CACHE_DIRTIED)));
    pthread_join(server, NULL);

    printf("Finished IPC.\n");
    return 0;
}

void printIpcResults()
{
    printf("IPC Send results\n");
    printf("run,"
           "hotCacheSendCycles,"
           "dirtyCacheSendCycles,"
           "hotCacheReplyCycles,"
           "dirtyCacheReplyCycles\n");
    for (int i = 0; i < RUNS; i++)
    {
        printf("%d,%lu,%lu,%lu,%lu\n", i,
               ipc_benchmark_results[HOT_CACHE][SEND][END][i] - ipc_benchmark_results[HOT_CACHE][SEND][START][i],
               ipc_benchmark_results[CACHE_DIRTIED][SEND][END][i] - ipc_benchmark_results[CACHE_DIRTIED][SEND][START][i],
               ipc_benchmark_results[HOT_CACHE][REPLY][END][i] - ipc_benchmark_results[HOT_CACHE][REPLY][START][i],
               ipc_benchmark_results[CACHE_DIRTIED][REPLY][END][i] - ipc_benchmark_results[CACHE_DIRTIED][REPLY][START][i]);
    }
}
