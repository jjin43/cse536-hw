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
  // Define the working set time window (e.g., 100 ticks)
  uint64 working_set_window = 100;
  uint64 current_time = read_current_timestamp();

  // Find free PSA blocks
  int blockno = -1;
  for (int i = 0; i < PSASIZE; i+=4) {
      if (psa_tracker[i] == false) {
          psa_tracker[i] = true;
          blockno = i;
          break;
      }
  }
  
  if (blockno == -1) {
    panic("evict_page_to_disk: no free PSA blocks");
  }

  // // Find a victim page using the Working Set algorithm
  // int victim_idx = -1;
  // for (int i = 0; i < MAXHEAP; i++) {
  //   if (current_time - p->heap_tracker[i].last_access_time > working_set_window) {
  //     victim_idx = i;
  //     break;
  //   }
  // }

  /* Find victim page using FIFO. */
  int victim_idx = -1;
  for (int i = 0; i < MAXHEAP; i++) {
    if (p->heap_tracker[i].loaded == true && victim_idx == -1)
      victim_idx = i;
    else if (p->heap_tracker[i].loaded == true && p->heap_tracker[i].last_load_time < p->heap_tracker[victim_idx].last_load_time)
      victim_idx = i;
    else {}
  }

  if (victim_idx == -1) {
    panic("evict_page_to_disk: no victim page found");
  }

  uint64 victim_va = p->heap_tracker[victim_idx].addr;
  char *kpage = kalloc();
  if (kpage == 0) {
    panic("evict_page_to_disk: kalloc");
  }
  if (copyin(p->pagetable, kpage, victim_va, PGSIZE) != 0) {
    kfree(kpage);
    panic("evict_page_to_disk: copyin");
  }

  struct buf* b;
  for (int i = 0; i < 4; i++) {
    b = bread(1, PSASTART + blockno + i);
    memmove(b->data, kpage + i * 1024, 1024);
    bwrite(b);
    brelse(b);
  }
  kfree(kpage);

  // Unmap the victim page
  uvmunmap(p->pagetable, victim_va, 1, 1);

  // Update heap_tracker
  p->heap_tracker[victim_idx].startblock = blockno;
  p->heap_tracker[victim_idx].loaded = false;
  p->resident_heap_pages--;

  // Print statement
  print_evict_page(victim_va, blockno);
}

/* Retrieve faulted page from disk. */
void retrieve_page_from_disk(struct proc* p, uint64 uvaddr) {
  // Find where the page is located in disk
  int blockno = -1;
  for (int i = 0; i < MAXHEAP; i++) {
    if (p->heap_tracker[i].addr == uvaddr) {
      blockno = p->heap_tracker[i].startblock;
      break;
    }
  }
  if (blockno == -1) {
    panic("retrieve_page_from_disk: page not found in disk");
  }

  // Create a kernel page to read memory temporarily into first
  char *kpage = kalloc();
  if (kpage == 0) {
    panic("retrieve_page_from_disk: kalloc");
  }

  // Read the disk block into temp kernel page
  struct buf* b;
  for (int i = 0; i < 4; i++) {
    b = bread(1, PSASTART + blockno + i);
    memmove(kpage + i * 1024, b->data, 1024);
    brelse(b);
  }

  // Copy from temp kernel page to uvaddr
  if (copyout(p->pagetable, uvaddr, kpage, PGSIZE) != 0) {
    kfree(kpage);
    panic("retrieve_page_from_disk: copyout");
  }
  kfree(kpage);

  // Print statement
  print_retrieve_page(uvaddr, blockno);
}

void handle_heap_page_fault(struct proc *p, uint64 faulting_addr) {
  // Allocate a new physical page and map it to the faulted address
  char *mem = kalloc();
  if (mem == 0) {
    panic("handle_heap_page_fault: kalloc");
  }
  memset(mem, 0, PGSIZE);
  if (mappages(p->pagetable, faulting_addr, PGSIZE, (uint64)mem, PTE_W | PTE_X | PTE_R | PTE_U) != 0) {
    kfree(mem);
    panic("handle_heap_page_fault: mappages");
  }
  
  // Track heap page load and access time
  for (int i = 0; i < MAXHEAP; i++) {
    if (p->heap_tracker[i].addr == faulting_addr) {
      uint64 current_time = read_current_timestamp();
      p->heap_tracker[i].last_load_time = current_time;
      p->heap_tracker[i].last_access_time = current_time;
      p->heap_tracker[i].loaded = true;
      break;
    }
  }
}


void handle_program_segment_fault(struct proc *p, uint64 faulting_addr) {
  struct inode *ip;
  struct elfhdr elf;
  struct proghdr ph;
  uint64 off, va, sz;
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

  /* cow checking*/
  if((p->cow_enabled) && (r_scause() == 0xf || r_scause()== 0xd)){
    if(copy_on_write(p, faulting_addr) == 1)
      sfence_vma();
      return;  
  }

  // Check if the faulting address is within the heap region
  bool is_heap_page = false;
  bool load_from_disk = false;
  int index = -1;

  for (int i = 0; i < MAXHEAP; i++) {
    if (p->heap_tracker[i].addr == faulting_addr) {
      is_heap_page = true;
      index = i;
      if (p->heap_tracker[i].startblock != -1) {
        load_from_disk = true;
      }
      break;
    }
  }

  if (is_heap_page) {
    // Handle heap page fault
    if (p->resident_heap_pages >= MAXRESHEAP) {
      evict_page_to_disk(p);
    }

    handle_heap_page_fault(p, faulting_addr);

    if (load_from_disk) {
      retrieve_page_from_disk(p, faulting_addr);
    }

    // Update the last load time for the loaded heap page
    p->heap_tracker[index].last_load_time = read_current_timestamp();
    p->heap_tracker[index].loaded = true;

    // Track that another heap page has been brought into memory
    p->resident_heap_pages++;
  } else {
    // Handle program segment page fault
    handle_program_segment_fault(p, faulting_addr);
  }

  // Flush stale page table entries
  sfence_vma();
}