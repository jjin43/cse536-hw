#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"
#include <stdbool.h>

struct spinlock cow_lock;

// Max number of pages a CoW group of processes can share
#define SHMEM_MAX 100

struct cow_group {
    int group; // group id
    uint64 shmem[SHMEM_MAX]; // list of pages a CoW group share
    int count; // Number of active processes
};

struct cow_group cow_group[NPROC];

struct cow_group* get_cow_group(int group) {
    if(group == -1)
        return 0;

    for(int i = 0; i < NPROC; i++) {
        if(cow_group[i].group == group)
            return &cow_group[i];
    }
    return 0;
}

void cow_group_init(int groupno) {
    for(int i = 0; i < NPROC; i++) {
        if(cow_group[i].group == -1) {
            cow_group[i].group = groupno;
            return;
        }
    }
} 

int get_cow_group_count(int group) {
    return get_cow_group(group)->count;
}
void incr_cow_group_count(int group) {
    get_cow_group(group)->count = get_cow_group_count(group)+1;
}
void decr_cow_group_count(int group) {
    get_cow_group(group)->count = get_cow_group_count(group)-1;
}

void add_shmem(int group, uint64 pa) {
    if(group == -1)
        return;

    uint64 *shmem = get_cow_group(group)->shmem;
    int index;
    for(index = 0; index < SHMEM_MAX; index++) {
        // duplicate address
        if(shmem[index] == pa)
            return;
        if(shmem[index] == 0)
            break;
    }
    shmem[index] = pa;
}

int is_shmem(int group, uint64 pa) {
    if(group == -1)
        return 0;

    uint64 *shmem = get_cow_group(group)->shmem;
    for(int i = 0; i < SHMEM_MAX; i++) {
        if(shmem[i] == 0)
            return 0;
        if(shmem[i] == pa)
            return 1;
    }
    return 0;
}

void cow_init() {
    for(int i = 0; i < NPROC; i++) {
        cow_group[i].count = 0;
        cow_group[i].group = -1;
        for(int j = 0; j < SHMEM_MAX; j++)
            cow_group[i].shmem[j] = 0;
    }
    initlock(&cow_lock, "cow_lock");
}

int uvmcopy_cow(pagetable_t old, pagetable_t new, uint64 sz) {
    pte_t *pte;
    uint64 pa, i;
    uint flags;
    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = walk(old, i, 0)) == 0)
            panic("uvmcopy_cow: pte should exist");
        if ((*pte & PTE_V) == 0)
            panic("uvmcopy_cow: page not present");
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        flags &= ~PTE_W; // Remove write permission
        flags |= PTE_R;  // Ensure read permission
        if (mappages(new, i, PGSIZE, pa, flags) != 0) {
            goto err;
        }
        *pte &= ~PTE_W; // Remove write permission from parent
        *pte |= PTE_R;  // Ensure read permission in parent
    }
    return 0;

err:
    uvmunmap(new, 0, i / PGSIZE, 1);
    return -1;
}

void copy_on_write() {
    struct proc *p = myproc();
    uint64 va = r_stval(); // Faulting virtual address
    pte_t *pte;
    uint64 pa;
    char *mem;
    uint flags;

    // Get the PTE for the faulting address
    if ((pte = walk(p->pagetable, va, 0)) == 0)
        panic("copy_on_write: pte should exist");
    if ((*pte & PTE_V) == 0)
        panic("copy_on_write: page not present");

    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    // Allocate a new page
    if ((mem = kalloc()) == 0)
        panic("copy_on_write: kalloc failed");

    // Copy contents from the shared page to the new page
    memmove(mem, (char*)pa, PGSIZE);

    // Map the new page in the faulting process's page table with write permissions
    flags |= PTE_W; // Add write permission
    flags &= ~PTE_R; // Remove read-only flag
    if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, flags) != 0) {
        kfree(mem);
        panic("copy_on_write: mappages failed");
    }

    // Add debug statement
    print_copy_on_write(p, va);
}
