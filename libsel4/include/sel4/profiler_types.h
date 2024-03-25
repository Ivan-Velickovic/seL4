#pragma once

#include <sel4/config.h>
#include <stdint.h>

#ifdef CONFIG_PROFILER_ENABLE

#define MAX_CALL_DEPTH 4

typedef struct pmu_sample {
    uint64_t valid;          /* Flag set by kernel to tell profiler that we have got a valid new sample */
    uint64_t ip;            /* Instruction pointer */
    uint32_t pid;           /* Process ID */
    uint64_t time;          /* Timestamp */
    uint32_t cpu;           /* CPU affinity */
    uint64_t period;        /* Number of events per sample */
    uint32_t irqFlag;
    uint64_t ips[MAX_CALL_DEPTH]; /* Call stack - MAX_CALL_DEPTH = 4 */
} pmu_sample_t;
#endif