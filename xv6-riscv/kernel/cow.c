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

        // Remove write permission and set read-only permission
        flags &= ~PTE_W;
        flags |= PTE_R;

        // Check if the address is already mapped
        if (walk(new, i, 0) != 0) {
            panic("uvmcopy_cow: address already mapped");
        }

        // Map the page in the child process's page table
        if (mappages(new, i, PGSIZE, pa, flags) != 0) {
            goto err;
        }

        // Update the parent process's PTE to be read-only
        *pte &= ~PTE_W;
        *pte |= PTE_R;
    }
    return 0;

err:
    uvmunmap(new, 0, i / PGSIZE, 1);
    return -1;
}

int copy_on_write(struct proc* p, uint64 fault_addr) {
    /* CSE 536: (2.6.2) Handling Copy-on-write */
    
    pte_t *pte;
    uint64 pa;
    uint flags;
    
    // Allocate a new page 
    pte = walk(p->pagetable, fault_addr, 0);
    pa = PTE2PA(*pte);
    
    // Copy contents from the shared page to the new page
    if(is_shmem(p->cow_group, pa)){
        char *mem = kalloc();
        memmove(mem, (char*)pa, PGSIZE);
        
        // Map the new page in the faulting process's page table with write permissions
        flags = PTE_FLAGS(*pte) | PTE_W;
        uvmunmap(p->pagetable, fault_addr, 1, 0);
        if(mappages(p->pagetable, fault_addr, PGSIZE, (uint64)pa, flags) != 0){
        kfree(mem);
        }
        
        print_copy_on_write(p, fault_addr);
        
        kfree(mem);
        return 1;
    }
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