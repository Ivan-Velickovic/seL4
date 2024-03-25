#include <benchmark/benchmark.h>
#include <arch/benchmark.h>
#include <armv/benchmark.h>
#include <api/faults.h>
#include <arch/arm/arch/64/mode/kernel/vspace.h>
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

    // Get the PC 
    uint64_t pc = getRegister(NODE_STATE(ksCurThread), FaultIP);
    // Save the interrupt flags
    uint32_t irq_f = 0;
    MRS(PMOVSR, irq_f);
    uint32_t val = BIT(CCNT_INDEX);
    MSR(PMOVSR, val);

    // Unwinding the call stack, currently only supporting 4 prev calls (arbitrary size)

    // First, get the threadRoot capability based on the current tcb
    cap_t threadRoot = TCB_PTR_CTE_PTR(NODE_STATE(ksCurThread), tcbVTable)->cap;

    /* lookup the vspace root */
    if (cap_get_capType(threadRoot) != cap_vspace_cap) {
        printf("Invalid vspace\n");
        return;
    }

    vspace_root_t *vspaceRoot = VSPACE_PTR(cap_vspace_cap_get_capVSBasePtr(threadRoot));

    // Read the x29 register for the address of the current frame pointer
    word_t fp = getRegister(NODE_STATE(ksCurThread), X29);

    word_t cc[4] = {0,0,0,0};

    // Loop and read the start of the frame pointer, save the lr value and load the next fp
    for (int i = 0; i < 4; i++) {
        // The LR should be one word above the FP
        word_t lr_addr = fp + sizeof(word_t);

        // We need to traverse the frame stack chain. We want to save the value of the LR in the frame
        // entry as part of our perf callchain, and then look at the next frame record. 
        readWordFromVSpace_ret_t read_lr = readWordFromVSpace(vspaceRoot, lr_addr);
        readWordFromVSpace_ret_t read_fp = readWordFromVSpace(vspaceRoot, fp);
        if (read_fp.status == EXCEPTION_NONE && read_lr.status == EXCEPTION_NONE) {
            // Set the fp value to the next frame entry
            fp = read_fp.value;
            cc[i] = read_lr.value;
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
    current_fault = seL4_Fault_PMUEvent_new(pc, irq_f);
    
    // Add the callstack to the message

    // Receiver here is the fault handler of the current thread
    cap_t receiver_cap = TCB_PTR_CTE_PTR(NODE_STATE(ksCurThread), tcbFaultHandler)->cap;
    endpoint_t *ep_ptr = EP_PTR(cap_endpoint_cap_get_capEPPtr(receiver_cap));
    tcb_t *receiver = TCB_PTR(endpoint_ptr_get_epQueue_head(ep_ptr));
    word_t *receiveIPCBuffer = lookupIPCBuffer(true, receiver);

    setMR(receiver, receiveIPCBuffer, seL4_PMUEvent_CC0, cc[0]);
    setMR(receiver, receiveIPCBuffer, seL4_PMUEvent_CC1, cc[1]);
    setMR(receiver, receiveIPCBuffer, seL4_PMUEvent_CC2, cc[2]);
    setMR(receiver, receiveIPCBuffer, seL4_PMUEvent_CC3, cc[3]);

    if (isRunnable(NODE_STATE(ksCurThread))) {
        handleFault(NODE_STATE(ksCurThread));
        schedule();
        activateThread();
    }
}
#endif 
#endif
