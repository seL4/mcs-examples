/*
 * Copyright 2020, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <autoconf.h>
#include <stdio.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <sel4runtime.h>

#include <simple/simple.h>
#include <simple-default/simple-default.h>

#include <vka/object.h>
#include <vka/object_capops.h>

#include <allocman/allocman.h>
#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <vspace/vspace.h>

#include <sel4utils/vspace.h>
#include <sel4utils/mapping.h>
#include <sel4utils/mcs_api.h>
#include <sel4utils/helpers.h>

#include <utils/arith.h>
#include <utils/zf_log.h>
#include <sel4utils/sel4_zf_logif.h>

#include <sel4platsupport/bootinfo.h>
#include <sel4bench/sel4bench.h>

/* constants */
#define IPCBUF_FRAME_SIZE_BITS 12 // use a 4K frame for the IPC buffer
#define IPCBUF_VADDR 0x7000000    // arbitrary (but free) address for IPC buffer

#define TEST_SIZE 30
#define MAGIC_CYCLES 700
#define N_TASK 3

/* global environment variables */
seL4_BootInfo *info;
simple_t simple;
vka_t vka;
allocman_t *allocman;

/* Task priority, budget and periods */
uint64_t priority[N_TASK] = {42, 41, 40};
uint64_t budget[N_TASK]   = {90000, 60000, 10000};
uint64_t period[N_TASK]   = {200000, 250000, 300000};

/* thread variables */
int tnums[3] = {0, 1, 2};
vka_object_t sc_objects[N_TASK]        = {0};
vka_object_t tcb_objects[N_TASK]       = {0};
UNUSED seL4_UserContext regs[N_TASK]   = {0};
vka_object_t ipc_frame_objects[N_TASK] = {0};
vka_object_t ep_object                 = {0};
cspacepath_t ep_cap_paths[N_TASK]      = {0};

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE (BIT(seL4_PageBits) * 30)
UNUSED static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* stack for the new thread */
#define THREAD_STACK_SIZE 4096
static uint64_t thread_stacks[N_TASK][THREAD_STACK_SIZE];

/* Suspend the root task after exit */
void exit_cb(int code)
{
    seL4_TCB_Suspend(seL4_CapInitThreadTCB);
}

/* This example demonstrates mixed-criticality scheduling:
 * Consider 3 threads with budget and period setting as T_1(90 / 200),
 * T_2(60 / 250), and T_3(10 / 300), sorted in descending order of priority.
 * The sum of CPU utilization is 72.3% thus meeting the schedulability
 * requirement of Rate-monotonic scheduling, which means the lower priority
 * thread T_3 will always have its chance to run. The `task` function which each
 * thread will run demonstrates this working.
 */

/* The variable ts stands for timestamp and tss an array of timestamps. */
void task(void *arg0, void *arg1, void *ipc_buf)
{
    int task_num = *(int *)arg0;
    __sel4_ipc_buffer = ipc_buf;

    uint64_t cycle_counts[TEST_SIZE];
    uint64_t tss[TEST_SIZE];

    uint64_t ts, prev = (uint64_t)sel4bench_get_cycle_count();
    uint64_t cycle_count = 0;
    int preemption_count = 0;
    tss[0] = prev;

    while (preemption_count < TEST_SIZE) {
        ts = (uint64_t)sel4bench_get_cycle_count();
        uint64_t diff = ts - prev;

        if (diff < MAGIC_CYCLES) {
            COMPILER_MEMORY_FENCE();
            cycle_count += diff;
            COMPILER_MEMORY_FENCE();

        } else {
            cycle_counts[preemption_count] = cycle_count;
            preemption_count++;
            (preemption_count < TEST_SIZE) ? (tss[preemption_count] = ts) : 0;
            cycle_count = 0;
        }
        prev = ts;
    }

    uint64_t util = 0;
    for (int i = 0; i < TEST_SIZE; i++) {
        printf("[thread %d] #%d: started %llu, consumed %llu\n", task_num, i + 1, tss[i], cycle_counts[i]);
        util += cycle_counts[i];
    }
    printf("[thread %d] CPU utilization: %d%%\n", task_num, (util * 100) / (ts - tss[0]));

    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, task_num);
    seL4_Send(ep_object.cptr, info);
}

int main(void)
{
    UNUSED int error = 0;
    char buf[7];
    vka_object_t pt_object = {0};;

    info = platsupport_get_bootinfo();
    ZF_LOGF_IF(info == NULL, "Failed to get bootinfo.");

    simple_default_init_bootinfo(&simple, info);

    simple_print(&simple);

    allocman = bootstrap_use_current_simple(&simple, ALLOCATOR_STATIC_POOL_SIZE, allocator_mem_pool);
    ZF_LOGF_IF(allocman == NULL, "Failed to initialize alloc manager.\n");

    allocman_make_vka(&vka, allocman);

    seL4_CPtr cspace_cap;
    cspace_cap = simple_get_cnode(&simple);

    seL4_CPtr pd_cap;
    pd_cap = simple_get_pd(&simple);

    error =  vka_alloc_page_table(&vka, &pt_object);
    ZF_LOGF_IFERR(error, "Failed to allocate new page table.\n");

    error = vka_alloc_endpoint(&vka, &ep_object);
    ZF_LOGF_IFERR(error, "Failed to allocate new endpoint object.\n");

    for (int i = 0; i < N_TASK; i++) {

        error = vka_alloc_frame(&vka, IPCBUF_FRAME_SIZE_BITS, &ipc_frame_objects[i]);
        ZF_LOGF_IFERR(error, "Failed to alloc a frame for the IPC buffer.\n");

        seL4_Word ipc_buffer_vaddr = IPCBUF_VADDR + (i * (1 << IPCBUF_FRAME_SIZE_BITS));
        error = seL4_ARCH_Page_Map(ipc_frame_objects[i].cptr, pd_cap, ipc_buffer_vaddr,
                                   seL4_AllRights, seL4_ARCH_Default_VMAttributes);
        if (error != 0) {
            error = seL4_ARCH_PageTable_Map(pt_object.cptr, pd_cap,
                                            ipc_buffer_vaddr, seL4_ARCH_Default_VMAttributes);
            ZF_LOGF_IFERR(error, "Failed to map page table into VSpace.\n");

            error = seL4_ARCH_Page_Map(ipc_frame_objects[i].cptr, pd_cap,
                                       ipc_buffer_vaddr, seL4_AllRights, seL4_ARCH_Default_VMAttributes);
            ZF_LOGF_IFERR(error, "Failed again to map the IPC buffer frame into the VSpace.\n");
        }

        error = vka_alloc_sched_context(&vka, &sc_objects[i]);
        ZF_LOGF_IFERR(error, "Failed to allocate sched context.\n");

        error = api_sched_ctrl_configure(simple_get_sched_ctrl(&simple, 0), sc_objects[i].cptr, budget[i], period[i], 0, 0);
        ZF_LOGF_IFERR(error, "Failed to configure scheduling context.\n");

        error = vka_alloc_tcb(&vka, &tcb_objects[i]);
        ZF_LOGF_IFERR(error, "Failed to allocate TCB.\n");

        error = api_tcb_configure(tcb_objects[i].cptr, seL4_CapNull, seL4_CapNull, sc_objects[i].cptr, cspace_cap, seL4_NilData,
                                  pd_cap, seL4_NilData, ipc_buffer_vaddr, ipc_frame_objects[i].cptr);
        ZF_LOGF_IFERR(error, "Failed to configure the TCB object.\n");

        error = seL4_TCB_SetPriority(tcb_objects[i].cptr, simple_get_tcb(&simple), priority[i]);
        ZF_LOGF_IFERR(error, "Failed to set the priority for the new TCB object.\n");

        size_t regs_size = sizeof(seL4_UserContext) / sizeof(seL4_Word);

        /* check that stack is aligned correctly */
        uintptr_t thread_n_stack_top = (uintptr_t)thread_stacks[i] + sizeof(thread_stacks[i]);
        uintptr_t tls_base = thread_n_stack_top - sel4runtime_get_tls_size();
        uintptr_t tp = (uintptr_t)sel4runtime_write_tls_image((void *)tls_base);
        uintptr_t aligned_stack_pointer = ALIGN_DOWN(tls_base, STACK_CALL_ALIGNMENT);

        int error = sel4utils_arch_init_local_context(task, (void *)&tnums[i], NULL,
                                                      (void *) ipc_buffer_vaddr,
                                                      (void *) aligned_stack_pointer,
                                                      &regs[i]);
        ZF_LOGF_IFERR(error, "Failed to init local context.\n");

        error = seL4_TCB_WriteRegisters(tcb_objects[i].cptr, 0, 0, regs_size, &regs[i]);
        ZF_LOGF_IFERR(error, "Failed to write the new thread's register set.\n");

        error = seL4_TCB_SetTLSBase(tcb_objects[i].cptr, tp);
        ZF_LOGF_IF(error, "Failed to set TLS base.\n");
    }

    sel4bench_init();

    printf("Starting mcs tasks.\n");

    for (int i = 0; i < N_TASK; i++) {
        error = seL4_TCB_Resume(tcb_objects[i].cptr);
        ZF_LOGF_IFERR(error, "Failed to start new thread.\n");
    }

    /* wait on endpoint and suspend when tasks finish */
    seL4_Word badge;
    for (int i = 0; i < N_TASK; i++) {
        seL4_Wait(ep_object.cptr, &badge);
        int ind = (int)seL4_GetMR(0);
        error = seL4_TCB_Suspend(tcb_objects[ind].cptr);
        ZF_LOGF_IFERR(error, "Failed to suspend task tcb.\n");
    }

    printf("Tasks finished returning from root thread.\n");
    sel4runtime_set_exit(exit_cb);

    return 0;
}
