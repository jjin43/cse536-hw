/* This file contains code for a generic page fault handler for processes. */
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"

#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

int loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz);
int flags2perm(int flags);

/* CSE 536: (2.4) read current time. */
uint64 read_current_timestamp() {
  uint64 curticks = 0;
  acquire(&tickslock);
  curticks = ticks;
  wakeup(&ticks);
  release(&tickslock);
  return curticks;
}

bool psa_tracker[PSASIZE];

/* All blocks are free during initialization. */
void init_psa_regions(void)
{
    for (int i = 0; i < PSASIZE; i++) 
        psa_tracker[i] = false;
}

/* Evict heap page to disk when resident pages exceed limit */
void evict_page_to_disk(struct proc* p) {
    /* Find free block */
    int blockno = 0;
    /* Find victim page using FIFO. */
    /* Print statement. */
    print_evict_page(0, 0);
    /* Read memory from the user to kernel memory first. */
    
    /* Write to the disk blocks. Below is a template as to how this works. There is
     * definitely a better way but this works for now. :p */
    struct buf* b;
    b = bread(1, PSASTART+(blockno));
        // Copy page contents to b.data using memmove.
    bwrite(b);
    brelse(b);

    /* Unmap swapped out page */
    /* Update the resident heap tracker. */
}

/* Retrieve faulted page from disk. */
void retrieve_page_from_disk(struct proc* p, uint64 uvaddr) {
    /* Find where the page is located in disk */

    /* Print statement. */
    print_retrieve_page(0, 0);

    /* Create a kernel page to read memory temporarily into first. */
    
    /* Read the disk block into temp kernel page. */

    /* Copy from temp kernel page to uvaddr (use copyout) */
}

void handle_heap_page_fault(struct proc *p, uint64 faulting_addr) {
  // Check if the faulting address is within the heap region
  if (faulting_addr >= p->sz) {
    // Grow the process's memory to include the faulting address
    uint64 new_sz = PGROUNDUP(faulting_addr + 1);
    if (new_sz > p->sz) {
      if (growproc(new_sz - p->sz) < 0) {
        panic("page_fault_handler: growproc failed");
      }
    }

    // Update the heap tracker
    for (int i = 0; i < MAXHEAP; i++) {
      if (p->heap_tracker[i].addr == faulting_addr) {
        p->heap_tracker[i].last_load_time = read_current_timestamp();
        p->heap_tracker[i].loaded = true;
        break;
      }
    }

    // Track that another heap page has been brought into memory
    p->resident_heap_pages++;
  }
}

void handle_program_segment_fault(struct proc *p, uint64 faulting_addr) {
  struct inode *ip = p->cwd;
  struct elfhdr elf;
  struct proghdr ph;
  uint64 offset, va, sz;
  int i;

  ilock(ip);
  if (readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf)) {
    panic("exec: read elf header failed");
  }

  if (elf.magic != ELF_MAGIC) {
    panic("exec: not an ELF file");
  }

  for (i = 0, offset = elf.phoff; i < elf.phnum; i++, offset += sizeof(ph)) {
    if (readi(ip, 0, (uint64)&ph, offset, sizeof(ph)) != sizeof(ph)) {
      panic("exec: read program header failed");
    }

    if (ph.type != ELF_PROG_LOAD) {
      continue;
    }

    if (faulting_addr >= ph.vaddr && faulting_addr < ph.vaddr + ph.memsz) {
      va = PGROUNDDOWN(faulting_addr);
      sz = PGSIZE;
      if (uvmalloc(p->pagetable, va, va + sz, PTE_W | PTE_X | PTE_R | PTE_U) == 0) {
        panic("exec: uvmalloc failed");
      }

      if (loadseg(p->pagetable, va, ip, ph.off + (va - ph.vaddr), sz) < 0) {
        panic("exec: loadseg failed");
      }

      break;
    }
  }
  iunlock(ip);
}

void page_fault_handler(uint64 faulting_addr) {
  struct proc *p = myproc();
  printf("Page fault at address %p in process %s (pid: %d)\n", faulting_addr, p->name, p->pid);

  // Check if the faulting address is within the heap region
  if (faulting_addr >= p->sz) {
    // Handle heap page fault
    handle_heap_page_fault(p, faulting_addr);
  } else {
    // Handle program segment page fault
    handle_program_segment_fault(p, faulting_addr);
  }

  // Flush stale page table entries
  sfence_vma();
}