#include <printf.h>
#include <timer.h>
#include <sched.h>
#include <dtb.h>
#include <fs/cpio.h>
#include <interrupt.h>
#include <exec.h>
#include <asm/constant.h>
#include <sysreg.h>
#include <timer.h>
#include <uart.h>
#include <string.h>
#include <mm.h>
#include <fs/vfs.h>
#include <sdcard.h>
#include <fs/fat32.h>
#include <mmu.h>

void foo() {
    enable_interrupt();
    for(int i = 0; i < 10; ++i) {
        printf("Thread id: %d %d\n", current->pid, i);
        task_sleep(500);
    }

    kill_task(current, 0);
    schedule();

    /* since we set lr in cpu_context,
     * after return the function will re execute itself again */
}

void run_init() {
    const char *argv[] = {"init", "-n", "0", NULL};
    kernel_exec_file("init", argv);

    panic("spawn init thread failed");
}

int split(char *buf, char *outbuf[], int n) {
    char *ps, *pe;
    int idx = 0;
    ps = pe = buf;

    while(idx < n) {
        while (*pe && *pe != ' ') pe++;

        int size = pe - ps;
        if (size) {
            outbuf[idx++] = ps;
        }

        if (*pe) {
            *pe = '\0';
            pe++;
            while (*pe == ' ') pe++;
            ps = pe;
        } else {
            break;
        }
    }

    return idx;
}

int atoi(const char *s) {
    int n = 0;
    int len = strlen(s);
    for (int i = 0; i < len; i++) {
        n *= 10;
        n += s[i] - 0x30;
    }

    return n;
}

void shell() {
    enable_interrupt();

    char *buf = kmalloc(0x100);
    while (1) {
        printf("> ");
        interact_readline_uart(buf);
        if (!strcmp("init", buf)) {
            run_init();
            while(1);
        }
        else if (!strcmp("kthread", buf)) {
            for (int i = 0; i < 10; i++) {
                schedule_kthread(&foo);
            }
            while(1);
        }
        else if (!strncmp("timeout", buf, 7)) {
            char *args[3];
            if (split(buf, args, 3) != 3) {
                puts("timeout: failed to split args");
                continue;
            }

            int msec = atoi(args[2]);
            timeout((timeout_cb)puts, (size_t)args[1], msec*1000);
        }
    }
}

void enable_fpu() {
    asm(
        "mrs x0, cpacr_el1\n\t"
        "orr x0, x0, #0x300000\n\t"
        "msr cpacr_el1, x0\n\t"
    :::"x0");
}

void main(void *_dtb_ptr) {
    BUILD_BUG_ON(sizeof(struct pt_regs) != PT_REGS_SIZE);
    init_buddy();
    init_cache();

    setup_kernel_space_mapping();

    dtb_node *dtb = build_device_tree(_dtb_ptr);
    init_cpio_storage(dtb);
    set_init_thread();
    mini_uart_init();
    /* enable for EL0 */
    enable_fpu();
    init_rootfs();
    sd_init();
    init_fat32();

    schedule_kthread(&run_init);

    enable_core_timer();
    enable_interrupt();

    idle();
}