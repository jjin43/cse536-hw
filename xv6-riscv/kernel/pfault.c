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
#include "file.h"

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
  struct inode *ip;
  struct elfhdr elf;
  struct proghdr ph;
  uint64 offset, va, sz;
  int i;


  // From exec.c
  begin_op();
  if((ip = namei(p->name)) == 0){
    end_op();
  }
  ilock(ip);

  // Check ELF header
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  if(elf.magic != ELF_MAGIC)
    goto bad;
  
  for(int i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    
    /* Check if faulting base addr is in current segment the allocate memory and load segment*/
    if((faulting_addr >= ph.vaddr) && (faulting_addr < (ph.vaddr + ph.memsz))){
    
      uvmalloc(p->pagetable, ph.vaddr, ph.vaddr + ph.memsz, flags2perm(ph.flags));
      
      if(loadseg(p->pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
        goto bad;
      break;
    }
  }


  if (strcmp(p->name, "cat") == 0) {
    struct inode *file_ip;
    if((file_ip = namei("cat")) == 0){ // Replace "cat" with the actual file name if different
      goto bad;
    }
    ilock(file_ip);

    // Allocate memory for the file contents
    uint64 file_size = file_ip->size;
    char *file_buffer = kalloc();
    if(file_buffer == 0)
      goto bad;

    // Read the file contents into the buffer
    if(readi(file_ip, 0, (uint64)file_buffer, 0, file_size) != file_size)
      goto bad;

    // Allocate memory in the process's address space
    uint64 file_va = PGROUNDUP(p->sz);
    if(uvmalloc(p->pagetable, file_va, file_va + file_size, PTE_U | PTE_R | PTE_W | PTE_X) < 0)
      goto bad;

    // Copy the file contents to the user space
    if(copyout(p->pagetable, file_va, file_buffer, file_size) < 0)
      goto bad;

    // Update the process size
    p->sz = file_va + file_size;

    iunlockput(file_ip);
    kfree(file_buffer);
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // OKAY, return
  print_load_seg(faulting_addr, ph.off, ph.memsz);
  return;

  bad:
  if(p->pagetable)
    proc_freepagetable(p->pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return;


}

void page_fault_handler(void) {
  struct proc *p = myproc();
  uint64 faulting_addr = r_stval() & (~(0xFFF));

  print_page_fault(p->name, faulting_addr);
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