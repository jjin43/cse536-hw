#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// Struct to keep VM registers (Sample; feel free to change.)
// struct vm_reg {
//     int     code;
//     int     mode;
//     uint64  val;
// };

// Keep the virtual state of the VM's privileged registers
struct vm_virtual_state {
    // User trap setup
    // User trap handling
    // Supervisor trap setup
    // User trap handling
    // Supervisor page table register
    // Machine information registers
    // Machine trap setup registers

    uint64 sepc; // Supervisor exception program counter
    uint64 mepc; // Machine exception program counter
    uint64 stvec; // Supervisor trap vector base address
    uint64 mtvec; // Machine trap vector base address
    uint64 sstatus; // Supervisor status register
    uint64 mstatus; // Machine status register
    uint64 mvendorid; // Vendor ID

    int priv;
    int is_pmp;

    pagetable_t pmp_pt;
    pagetable_t org_pt; 

};

struct vm_virtual_state vm;

// In your ECALL, add the following for prints
// struct proc* p = myproc();
// printf("(EC at %p)\n", p->trapframe->epc);

uint32 get_instruction(struct proc* p, uint64 addr) {
    char* p_addr = kalloc();
    copyin(p->pagetable, p_addr, addr, PGSIZE);
    uint32 instr = (*(uint32*) p_addr);
    kfree(p_addr);
    return instr;
}

void map_pt(struct proc* p) {

}

void do_ecall(struct proc* p) {

}

void do_sret(struct proc* p) {

}

void do_mret(struct proc* p) {

}

void do_csrw(struct proc* p) {

}

void do_csrr(struct proc* p) {

}

void trap_and_emulate(void) {
    /* Comes here when a VM tries to execute a supervisor instruction. */

    /* Retrieve all required values from the instruction */
    struct proc *p = myproc();
    uint64 addr = r_sepc();
    uint32 instr = get_instruction(p, addr);

    // https://riscv.org/wp-content/uploads/2017/05/riscv-spec-v2.2.pdf - P.22
    uint32 op       = (instr & 0x7F);
    uint32 rd       = (instr >> 7) & 0x1F;
    uint32 funct3   = (instr >> 12) & 0x7;
    uint32 rs1      = (instr >> 15) & 0x1F;
    uint32 uimm     = (instr >> 20) & 0xFFF;

    if(funct3==0x0 && uimm==0x0){
        printf("(EC at %p)\n", p->trapframe->epc);
        do_ecall(p);
    }
    else if(funct3==0x0 && uimm==0x102){
        printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
        do_sret(p);
    }
    else if(funct3==0x0 && uimm==0x302){
        printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
        do_mret(p);
    }
    else if(funct3==0x1){
        printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
        do_csrw(p);
    }
    else if(funct3=0x2){
        printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
        do_csrr(p);
    }
    else{
        printf("[DEBUG] Unexpected instruction\n");
        setkilled(p);
    }
}

void trap_and_emulate_init(void) {
    /* Create and initialize all state for the VM */
    memset(&vm, 0, sizeof(vm));
    vm.priv = 3;
    vm.mvendorid = 0x637365353336;  // "cse536" in hex

}