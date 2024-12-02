/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2015, 2016 Hesham Almatary <heshamelmatary@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <types.h>
#include <object.h>
#include <kernel/vspace.h>
#include <api/faults.h>
#include <api/syscall.h>

#include <types.h>
#include <machine/io.h>
#include <api/faults.h>
#include <api/syscall.h>
#include <util.h>

bool_t Arch_handleFaultReply(tcb_t *receiver, tcb_t *sender, word_t faultType)
{
    switch (faultType) {
    case seL4_Fault_VMFault:
        return true;
#ifdef CONFIG_RISCV_HYPERVISOR_SUPPORT
    case seL4_Fault_VCPUFault:
        return true;
#endif
    default:
        fail("Invalid fault");
    }
}

word_t Arch_setMRs_fault(tcb_t *sender, tcb_t *receiver, word_t *receiveIPCBuffer, word_t faultType)
{
    switch (faultType) {
    case seL4_Fault_VMFault: {
        printf("seL4_VMFault_IP(%d): 0x%lx\n", seL4_VMFault_IP, getRestartPC(sender));
        printf("seL4_VMFault_Instruction(%d): 0x%llx\n", seL4_VMFault_Instruction, seL4_Fault_VMFault_get_instruction(sender->tcbFault));
        printf("seL4_VMFault_Addr(%d): 0x%llx\n", seL4_VMFault_Addr, seL4_Fault_VMFault_get_address(sender->tcbFault));
        printf("seL4_VMFault_PrefetchFault(%d): 0x%llx\n", seL4_VMFault_PrefetchFault, seL4_Fault_VMFault_get_instructionFault(sender->tcbFault));
        printf("seL4_VMFault_FSR(%d): 0x%llx\n", seL4_VMFault_FSR, seL4_Fault_VMFault_get_FSR(sender->tcbFault));

        setMR(receiver, receiveIPCBuffer, seL4_VMFault_IP, getRestartPC(sender));
#ifdef CONFIG_RISCV_HYPERVISOR_SUPPORT
        setMR(receiver, receiveIPCBuffer, seL4_VMFault_Instruction,
                seL4_Fault_VMFault_get_instruction(sender->tcbFault));
#endif
        setMR(receiver, receiveIPCBuffer, seL4_VMFault_Addr,
              seL4_Fault_VMFault_get_FSR(sender->tcbFault));
        setMR(receiver, receiveIPCBuffer, seL4_VMFault_PrefetchFault,
              seL4_Fault_VMFault_get_instructionFault(sender->tcbFault));
        unsigned int ret = setMR(receiver, receiveIPCBuffer, seL4_VMFault_FSR,
                     0x100000000);
        asm volatile(".word 0xfffff00b");

        return ret;
    }
#ifdef CONFIG_RISCV_HYPERVISOR_SUPPORT
    case seL4_Fault_VCPUFault:
        setMR(receiver, receiveIPCBuffer, seL4_VCPUFault_Cause, seL4_Fault_VCPUFault_get_cause(sender->tcbFault));
        return setMR(receiver, receiveIPCBuffer, seL4_VCPUFault_Data, seL4_Fault_VCPUFault_get_data(sender->tcbFault));
#endif
    default:
        fail("Invalid fault");
    }
}
