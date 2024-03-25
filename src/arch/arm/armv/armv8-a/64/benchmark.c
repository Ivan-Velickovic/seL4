#include <benchmark/benchmark.h>
#include <arch/benchmark.h>
#include <armv/benchmark.h>
#include <api/faults.h>
#include <arch/arm/arch/64/mode/kernel/vspace.h>
#include <sel4/config.h>
#include <sel4/profiler_types.h>

#ifdef CONFIG_ENABLE_BENCHMARKS
#ifdef CONFIG_PROFILER_ENABLE
void armv_handleOverflowIRQ(void) {
    // Halt the PMU
    uint32_t mask = 0;

    // Disable counters
    mask = 0;
    mask |= (1 << 0); 
    mask |= (1 << 1);
    mask |= (1 << 2);
    MSR("PMCR_EL0", (~mask));

    // Disable cycle counter
    mask = 0;
    mask |= (1 << 31);
    MSR("PMCNTENSET_EL0", (~mask));

    if (NODE_STATE(ksCurThread) == NULL) {
        printf("NULL current thread\n");
        return;
    }
    
    #ifdef CONFIG_KERNEL_LOG_BUFFER

      // Checking the log buffer exists, and is valid
    if (ksUserLogBuffer == 0) {
        userError("A user-level buffer has to be set before starting profiling.\
                Use seL4_BenchmarkSetLogBuffer\n");
        setRegister(NODE_STATE(ksCurThread), capRegister, seL4_IllegalOperation);
        return;
    } 

    // Get the pmu sample structure in the log
    pmu_sample_t *profLogs = (pmu_sample_t *) KS_LOG_PPTR;

    // Save the interrupt flags
    uint32_t irq_f = 0;
    MRS(PMOVSR, irq_f);
    // uint32_t val = BIT(CCNT_INDEX);
    // MSR(PMOVSR, val);
    profLogs[0].irqFlag = irq_f;

    // Check that this TCB has been marked to track
    if (NODE_STATE(ksCurThread)->tcbProfileId == 0) {
        profLogs[0].valid = 0;
        return;
    }

    // Get the PC 
    uint64_t pc = getRegister(NODE_STATE(ksCurThread), FaultIP);


    // Unwinding the call stack, currently only supporting 4 prev calls (arbitrary size)

    // First, get the threadRoot capability based on the current tcb
    cap_t threadRoot = TCB_PTR_CTE_PTR(NODE_STATE(ksCurThread), tcbVTable)->cap;

    vspace_root_t *vspaceRoot = VSPACE_PTR(cap_vspace_cap_get_capVSBasePtr(threadRoot));

    // Read the x29 register for the address of the current frame pointer
    word_t fp = getRegister(NODE_STATE(ksCurThread), X29);

    // Loop and read the start of the frame pointer, save the lr value and load the next fp
    for (int i = 0; i < SEL4_PROF_MAX_CALL_DEPTH; i++) {
        // The LR should be one word above the FP
        word_t lr_addr = fp + sizeof(word_t);

        // We need to traverse the frame stack chain. We want to save the value of the LR in the frame
        // entry as part of our perf callchain, and then look at the next frame record. 
        readWordFromVSpace_ret_t read_lr = readWordFromVSpace(vspaceRoot, lr_addr);
        readWordFromVSpace_ret_t read_fp = readWordFromVSpace(vspaceRoot, fp);
        if (read_fp.status == EXCEPTION_NONE && read_lr.status == EXCEPTION_NONE) {
            // Set the fp value to the next frame entry
            fp = read_fp.value;
            profLogs[0].ips[i] = read_lr.value;
            profLogs[0].nr = i;
            // If the fp is 0, then we have reached the end of the frame stack chain
            if (fp == 0) {
                break;
            }
        } else {
            // If we are unable to read, then we have reached the end of our stack unwinding
            printf("0x%"SEL4_PRIx_word": INVALID\n",
                   lr_addr);
            break;
        }        
    }
    // Add the data to the profiler log buffer
    profLogs[0].valid = 1;
    profLogs[0].ip = pc;
    // Populate PID with whatever we registered inside the TCB
    profLogs[0].pid = NODE_STATE(ksCurThread)->tcbProfileId;
    profLogs[0].time = getCurrentTime();
    #ifdef ENABLE_SMP_SUPPORT
    profLogs[0].cpu = NODE_STATE(ksCurThread)->tcbAffinity;
    #else
    profLogs[0].cpu = 0;
    #endif
    // The period is only known by the profiler.
    profLogs[0].period = 0;
     #endif
 
}
#endif
#endif
