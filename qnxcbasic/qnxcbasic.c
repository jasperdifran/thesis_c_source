//============================================================================
// Name        : qnxcbasic.c
// Author      : Jasper Di Francesco
// Version     : 0.0.1
// Copyright   :
// Description : QNX benchmarking program.
//============================================================================

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
#include <threads.h>
#include <sched.h>
#include <sys/cache.h>

#include <pollution.h>

#define IPC_BENCH 1

uint64_t ticker = 0;

#define TIMER_PERIOD_MS 100

const struct sigevent *custom_isr(void *arg, int id)
{
    /**
     * @brief Ideally wanted to update a shared mem region, seemed to not be called.
     * See line 151.
     */
    int *i = (int *)arg;
    *i = 5;

    return NULL;
}

void *irqSpinnerFn(void *arg)
{
    sched_yield();

    int policy;
    struct sched_param selfSchedParam;
    pthread_t self = pthread_self();
    if (pthread_getschedparam(self, &policy, &selfSchedParam) != 0)
    {
        perror("pthread_getschedparam");
        exit(1);
    }

    while (1)
    {
        ticker = ClockCycles();
    }
}
#define RUNS 100

uint64_t irq_benchmark_results[2][2][RUNS];

void printIrqResults()
{
    for (int i = 0; i < RUNS; i++)
    {
        printf("%d,%lu,%lu\n", i, irq_benchmark_results[0][1][i] - irq_benchmark_results[0][0][i], irq_benchmark_results[1][1][i] - irq_benchmark_results[1][0][i]);
    }
}

void *irqBenchmarkRunner(void *arg)
{
    sched_yield();
    int policy;
    struct sched_param selfSchedParam;
    pthread_t self = pthread_self();
    if (pthread_getschedparam(self, &policy, &selfSchedParam) != 0)
    {
        perror("pthread_getschedparam");
        exit(1);
    }

    timer_t timer_id;
    struct sigevent event;
    struct itimerspec itime;

    // Create a timer
    event.sigev_notify = SIGEV_INTR; // Notify through an interrupt
    event.sigev_code = 1;            // Arbitrary code for the interrupt
    event.sigev_priority = 100;      // Use the same priority as the current thread

    if (timer_create(CLOCK_REALTIME, &event, &timer_id) == -1)
    {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    int didIsr = 0;
    for (int i = 0; i < RUNS; i++)
    {
        itime.it_value.tv_sec = 0;
        itime.it_value.tv_nsec = 1 * 1000 * 1000 * 100;
        itime.it_interval.tv_sec = 0;
        itime.it_interval.tv_nsec = 0;

        if (timer_settime(timer_id, 0, &itime, NULL) == -1)
        {
            perror("timer_settime");
            exit(EXIT_FAILURE);
        }

        // Attach the custom ISR to the timer interrupt
        int isr_id = InterruptAttach(1, custom_isr, &didIsr, 4, 0);
        if (isr_id == -1)
        {
            perror("InterruptAttach");
            exit(EXIT_FAILURE);
        }

        InterruptWait(0, NULL);
        uint64_t currTime = ClockCycles();
        irq_benchmark_results[0][1][i] = currTime;
        irq_benchmark_results[0][0][i] = ticker;
        InterruptDetach(isr_id);
    }

    /**
     * @brief Test with dirty/polluted cache
     *
     */
    for (int i = 0; i < RUNS; i++)
    {

        // Set the timer to expire after 100ms and then disable it (one-shot)
        itime.it_value.tv_sec = 0;
        itime.it_value.tv_nsec = 1 * 1000 * 1000 * 100;
        itime.it_interval.tv_sec = 0;
        itime.it_interval.tv_nsec = 0;

        if (timer_settime(timer_id, 0, &itime, NULL) == -1)
        {
            perror("timer_settime");
            exit(EXIT_FAILURE);
        }

        /**
         * @brief Supposed to be the way that we get ISR function to be called from the kernel.
         * Had issues with it, either not getting called or the board timing out and rebooting,
         * only seemd to be `InterruptWait` which did anything.
         *
         */
        // int isr_id = InterruptAttach(1, custom_isr, &didIsr, 4, 0);
        // if (isr_id == -1)
        // {
        //     perror("InterruptAttach");
        //     exit(EXIT_FAILURE);
        // }

        real_cache_func();

        InterruptWait(0, NULL);
        irq_benchmark_results[1][1][i] = ClockCycles();
        irq_benchmark_results[1][0][i] = ticker;
        InterruptDetach(isr_id);
    }

    // Clean up (not reached in this example)
    timer_delete(timer_id);

    if (didIsr)
    {
        printf("DID THE ISR\n");
    }

    return NULL;
}

/**
 * @brief Initially made this function for testing 'contention' on timer expiry. After chatting to
 * Kevin, this wasn't really testing the right thing. Leaving a few of these spinners running increased
 * WCIL, but did so because the kernel was spending time notifying each thread, not due to contention.
 *
 * The better test is having all IRQ sources on the board going off at once, and seeing how long the longest
 * one takes.
 *
 * @param arg
 * @return void*
 */
void *spinnerThreadFn(void *arg)
{
    timer_t timer_id;
    struct sigevent event;
    struct itimerspec itime;

    // Create a timer
    event.sigev_notify = SIGEV_INTR; // Notify through an interrupt
    event.sigev_code = 1;            // Arbitrary code for the interrupt
    event.sigev_priority = 100;      // Use the same priority as the current thread

    if (timer_create(CLOCK_REALTIME, &event, &timer_id) == -1)
    {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    // Set the timer to expire after 100ms and then disable it (one-shot)
    itime.it_value.tv_sec = 0;
    itime.it_value.tv_nsec = 1;
    itime.it_interval.tv_sec = 0;
    itime.it_interval.tv_nsec = 100;
    // uint64_t before = ClockCycles();

    if (timer_settime(timer_id, 0, &itime, NULL) == -1)
    {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }

    int count = 0;
    while (1)
    {
        if (count < 10)
        {
            printf("Got timer intr in spinner\n");
            count++;
        }
        InterruptWait(0, NULL);
    }
}

int irqBenchmark()
{
    pthread_t tickerThread, runnerThread;

    int policy;
    struct sched_param selfSchedParam;
    pthread_t self = pthread_self();
    if (pthread_getschedparam(self, &policy, &selfSchedParam) != 0)
    {
        perror("pthread_getschedparam");
        exit(1);
    }

    if (pthread_setschedparam(self, SCHED_FIFO, &selfSchedParam))
    {
        perror("pthread_setschedparam");
        exit(1);
    }

    int irqTicker = pthread_create(&tickerThread, NULL, irqSpinnerFn, NULL);
    if (irqTicker != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_setschedprio(tickerThread, selfSchedParam.sched_priority - 2);

    int irqRunner = pthread_create(&runnerThread, NULL, irqBenchmarkRunner, NULL);
    if (irqRunner != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_setschedprio(runnerThread, selfSchedParam.sched_priority - 1);

    pthread_join(runnerThread, NULL);

    return 0;
}

int main(void)
{
    printf("Cycles per second: %lu\n", SYSPAGE_ENTRY(qtime)->cycles_per_sec);
    printf("CPU res %d\n", SYSPAGE_ENTRY(cpuinfo)->speed);

    int cyclesPerTick = (SYSPAGE_ENTRY(cpuinfo)->speed * 1000000) / SYSPAGE_ENTRY(qtime)->cycles_per_sec;
    printf("cpuCyclesPerClockCycle: %d\n", cyclesPerTick);

    printf("\nQNX Benchmarks\n==============\n");

    // IPC
#ifdef IPC_BENCH
    ipcBenchmark();
#endif

    irqBenchmark();

    printf("Benchmarks done. Printing results...\n");
#ifdef IPC_BENCH
    printIpcResults();
#endif

    printIrqResults();

    printf("All is well in the universe.\n");
}
