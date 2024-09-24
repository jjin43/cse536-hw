/* These files have been taken from the open-source xv6 Operating System codebase (MIT License).  */

#include "types.h"
#include "param.h"
#include "layout.h"
#include "riscv.h"
#include "defs.h"
#include "buf.h"
#include "measurements.h"
#include <stdbool.h>

void main();
void timerinit();

/* entry.S needs one stack per CPU */
__attribute__ ((aligned (16))) char bl_stack[STSIZE * NCPU];
char *bl_stack_end = bl_stack + sizeof(bl_stack);


/* Context (SHA-256) for secure boot */
SHA256_CTX sha256_ctx;
#define SYSINFOADDR 0x80080000
#define bootloader_start 0x80000000

/* Structure to collects system information */
struct sys_info {
  /* Bootloader binary addresses */
  uint64 bl_start;
  uint64 bl_end;
  /* Accessible DRAM addresses (excluding bootloader) */
  uint64 dr_start;
  uint64 dr_end;
  /* Kernel SHA-256 hashes */
  BYTE expected_kernel_measurement[32];
  BYTE observed_kernel_measurement[32];
};
struct sys_info* sys_info_ptr;

extern void _entry(void);
void panic(char *s)
{
  for(;;)
    ;
}

/* CSE 536: Boot into the RECOVERY kernel instead of NORMAL kernel
 * when hash verification fails. */
void setup_recovery_kernel(void) {
}

/* CSE 536: Function verifies if NORMAL kernel is expected or tampered. */
bool is_secure_boot(void) {
  bool verification = true;

  /* Read the binary and update the observed measurement 
   * (simplified template provided below) */
  sha256_init(&sha256_ctx);
  struct buf b;
  sha256_update(&sha256_ctx, (const unsigned char*) b.data, BSIZE);
  sha256_final(&sha256_ctx, sys_info_ptr->observed_kernel_measurement);

  /* Three more tasks required below: 
   *  1. Compare observed measurement with expected hash
   *  2. Setup the recovery kernel if comparison fails
   *  3. Copy expected kernel hash to the system information table */
  if (!verification)
    setup_recovery_kernel();
  
  return verification;
}

// entry.S jumps here in machine mode on stack0.
void start()
{
  /* CSE 536: Define the system information table's location. */
  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // disable paging
  w_satp(0);

  /* CSE 536: Unless kernelpmp[1-2] booted, allow all memory 
   * regions to be accessed in S-mode. */ 
  #if !defined(KERNELPMP1) || !defined(KERNELPMP2)
    w_pmpaddr0(0x3fffffffffffffull);
    w_pmpcfg0(0xf);
  #endif

  /* CSE 536: With kernelpmp1, isolate upper 10MBs using TOR */ 
  #if defined(KERNELPMP1)

    // set pmcfg register
    uint64 pmpcfg0_value = 0;
    pmpcfg0_value |= (1 << 0); // read perm bit
    pmpcfg0_value |= (1 << 1); // write perm bit
    pmpcfg0_value |= (1 << 2); // execute perm bit
    pmpcfg0_value |= (0b100 << 3); // TOR bit = 0b100

    // Write to pmpcfg0 register (config for PMP region0)
    w_pmpcfg0(pmpcfg0_value);

    // set pmpaddr0 register
    // range = 117MB , pmpaddr0 = (bootloader_start + 117MB) / 4KB
    uint64 pmpaddr0_value = (bootloader_start + 122683392) / 4096;

    // write to pmpaddr0 register
    w_pmpaddr0(pmpaddr0_value);
  #endif

  /* CSE 536: With kernelpmp2, isolate 118-120 MB and 122-126 MB using NAPOT */ 
  #if defined(KERNELPMP2)
    w_pmpaddr0(0x0ull);
    w_pmpcfg0(0x0);
  #endif

  /* CSE 536: Verify if the kernel is untampered for secure boot */
  //if (!is_secure_boot()) {
  if(0==1){ 
    /* Skip loading since we should have booted into a recovery kernel 
     * in the function is_secure_boot() */
    goto out;
  }
  
  /* CSE 536: Load the NORMAL kernel binary (assuming secure boot passed). */
  uint64 kernel_load_addr = find_kernel_load_addr(NORMAL);
  uint64 kernel_binary_size = find_kernel_size(NORMAL);     
  uint64 kernel_entry = find_kernel_entry_addr(NORMAL);
  
  struct buf b;
  uint64 num_blocks = kernel_binary_size / BSIZE;
  if (kernel_binary_size % BSIZE != 0) {
    num_blocks++;
  }

  // skip ELF header, first 4kb
  for (uint64 i = 4; i < num_blocks; i++) {
    b.blockno = i;
    kernel_copy(NORMAL, &b);
    uint64 memory_offset = (i - 4) * BSIZE;
    memmove((void*) (kernel_load_addr + memory_offset), b.data, BSIZE);
  }

  /* CSE 536: Write the correct kernel entry point */
  w_mepc((uint64) kernel_entry);

  /* CSE 536: Provide system information to the kernel. */
  sys_info_ptr = (struct sys_info*) SYSINFOADDR;
  sys_info_ptr->bl_start = bootloader_start;
  sys_info_ptr->bl_end = (uint64) end;
  sys_info_ptr->dr_start = 0x80000000;
  sys_info_ptr->dr_end = 0x88000000;

 out:

  /* CSE 536: Send the observed hash value to the kernel (using sys_info_ptr) */

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // return address fix
  uint64 addr = (uint64) panic;
  asm volatile("mv ra, %0" : : "r" (addr));

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}
