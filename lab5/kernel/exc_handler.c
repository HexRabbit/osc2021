#include "printf.h"
#include "sched.h"
#include "mm.h"
#include "sysreg.h"
#include "syscall.h"
#include "uart.h"
#include "reg.h"
#include "timer.h"
#include <current.h>
#include <interrupt.h>

void svc_handler(struct pt_regs *regs);
void segv_handler();

void irq_handler(struct pt_regs *regs) {
    if (*CORE0_TIMER_IRQ_SRC & 2) {
        core_timer_handler();

    } else if (*AUXIRQ & 1) {
        /* uart handler */
    }
}

void sync_handler(struct pt_regs *regs) {
    unsigned long esr = read_sysreg(esr_el1);
    unsigned ec = ESR_ELx_EC(esr);
    unsigned iss = ESR_ELx_ISS(esr);

    switch (ec) {
    case ESR_ELx_EC_SVC64:
        /* iss[24-16] = res0  */
        /* iss[15-0]  = imm16 */
        if ((iss & 0xffff) == 0) {
            svc_handler(regs);
        }
        break;

    case ESR_ELx_EC_DABT_LOW:
        /* Userland data abort exception */
        segv_handler();

    case ESR_ELx_EC_IABT_LOW:
        /* Userland instruction abort exception */
        segv_handler();

    case ESR_ELx_EC_BRK_LOW:
        panic("[Kernel] panic: Breakpoint exception");

    default:
        panic("[Kernel] panic: Unknown exception: EC=0x%x, ISS=0x%x\n\r", ec, iss);
    }
}

void svc_handler(struct pt_regs *regs) {
    //enable_interrupt();

    unsigned nr = regs->regs[8];
    if (nr >= NR_syscalls) {
        panic("[Kernel] svc_handler: Unknown syscall number %d\n\r", regs->regs[8]);
    }

    /* exit syscall wont return */
    syscall_table[nr](regs);

    //disable_interrupt();
}

void segv_handler() {
    //disable_interrupt();

    struct task_struct *ts = current;
    kfree(ts->stack);
    kfree(ts->kstack);
    kfree(ts->user_prog);
    kfree(ts);

    del_task(current);

    /* call scheduler */
    schedule();
}