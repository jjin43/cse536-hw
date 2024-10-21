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
    /* CSE 536: (2.6.1) Handling Copy-on-write fork() */
    
    /*
    following code referred from -  https://github.com/mit-pdos/xv6-riscv/blob/f5b93ef12f7159f74f80f94729ee4faabe42c360/kernel/vm.c#L306
    */
    printf("COW-1\n");
    pte_t *pte;
    uint64 pa, i;
    uint flags;
    // Copy user virtual memory from old(parent) to new(child) process

    // Map pages as Read-Only in both the processes
    for(i = 0; i < sz; i += PGSIZE){
        if((pte = walk(old, i, 0)) == 0)
            panic("uvmcopy: pte should exist");
        if((*pte & PTE_V) == 0)
            panic("uvmcopy: page not present");
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte) & (~PTE_W); // removing write permission
    
        mappages(new, i, PGSIZE, pa, flags);
        uvmunmap(old, i, 1, 0);
        mappages(old, i, PGSIZE, pa, flags);
    }
    return 0;
}

int copy_on_write(struct proc* p, uint64 fault_addr) {
    /* CSE 536: (2.6.2) Handling Copy-on-write */
    
    pte_t *pte;
    uint64 pa;
    uint flags;
    
    // Allocate a new page 
    pte = walk(p->pagetable, fault_addr, 0);
    if (pte == 0 || (*pte & PTE_V) == 0)
        panic("copy_on_write: pte should exist and be valid");
    
    pa = PTE2PA(*pte);
    printf("CP-1\n");
    // Check if the page is shared
    if(is_shmem(p->cow_group, pa)){
        char *mem = kalloc();
        if (mem == 0)
            panic("copy_on_write: kalloc failed");
        printf("CP-2\n");

        // Copy contents from the shared page to the new page
        memmove(mem, (char*)pa, PGSIZE);
        printf("CP-3\n");

        // Map the new page in the faulting process's page table with write permissions
        flags = PTE_FLAGS(*pte) | PTE_W;
        uvmunmap(p->pagetable, fault_addr, 1, 0);
        printf("CP-4\n");
        
        if(mappages(p->pagetable, fault_addr, PGSIZE, (uint64)mem, flags) != 0){
            kfree(mem);
            panic("copy_on_write: mappages failed");
        }
        
        // Add the new page to the shared memory list
        add_shmem(p->cow_group, (uint64)mem);
        
        // Print debug statement
        print_copy_on_write(p, fault_addr);
        
        return 1;
    }
    printf("CP-5\n");
    return 0;
}

void erase_cow_group(int pid){

  for(int i=0; i<NPROC; i++){
    if(cow_group[i].group == pid){
      cow_group[i].count = 0;
      cow_group[i].group = -1;
      
      for(int j=0; j<SHMEM_MAX; j++){
	cow_group[i].shmem[j] = 0;      
      }
      
      return;
    }
  }

}
