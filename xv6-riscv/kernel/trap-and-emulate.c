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
// struct vm_virtual_state {
//     // User trap setup
//     // User trap handling
//     // Supervisor trap setup
//     // User trap handling
//     // Supervisor page table register
//     // Machine information registers
//     // Machine trap setup registers


//     uint64 mepc;
//     uint64 mtvec;
//     uint64 mstatus;
//     uint64 mvendorid;
//     uint64 mscratch;
//     uint64 mtinst;
//     uint64 mstatush;

//     uint64 satp;
//     uint64 sepc;
//     uint64 sscratch;
//     uint64 sstatus;
//     uint64 sedeleg;
//     uint64 stvec;

//     uint64 uscratch;
//     uint64 ustatus;
//     uint64 uie;

//     uint64 pmpcfg[16];

//     int priv;
//     int is_pmp;

//     pagetable_t new_pt;
//     pagetable_t org_pt; 

// };

struct vm_virtual_state
{
    // Machine trap handling registers
    // 0x340 - 0x344
    uint64 mscratch; // Scratch register for machine trap handlers
    uint64 mepc;     // Machine exception program counter
    uint64 mcause;   // Machine trap cause
    uint64 mtval;    // Machine bad address or instruction
    uint64 mip;      // Machine interrupt pending

    // 0x34A - 0x34B
    uint64 mtinst; // Machine trap instruction
    uint64 mtval2; // Machine bad guest physical address

    // Machine trap setup registers
    //  0x300 - 0x306
    uint64 mstatus;    // Machine status register
    uint64 misa;       // ISA and extensions
    uint64 medeleg;    // Machine exception delegation register
    uint64 mideleg;    // Machine interrupt delegation register
    uint64 mie;        // Machine interrupt-enable register
    uint64 mtvec;      // Machine trap vector base address register
    uint64 mcounteren; // Machine interrupt delegation register
    //  0x310
    uint64 mstatush; // Additional Machine status register

    //  Machine information registers
    //  0xF11 - 0xF14
    uint64 mvendorid; // Vendor ID
    uint64 marchid;   // Architecture ID
    uint64 mimpid;    // Implementation ID
    uint64 mhartid;   // Hardware thread ID

    // Machine physical memory protection registers
    // 0x3A0 - 0x3EF
    uint64 pmpcfg[16];  // PMP configuration registers (pmpcfg0 - pmpcfg15)
    uint64 pmpaddr[64]; // PMP address registers (pmpaddr0 - pmpaddr63)

    //  Supervisor page table register (satp)
    //  0x180
    uint64 satp; // Supervisor address translation and protection

    //  Supervisor trap handling registers
    //  0x140 - 0x144
    uint64 sscratch;
    uint64 sepc;
    uint64 scause;
    uint64 stval;
    uint64 sip;

    //  Supervisor trap setup
    //  0x100
    uint64 sstatus; // Supervisor status register
    //  0x102 - 0x106
    uint64 sedeleg; // Supervisor exception delegation register
    uint64 sideleg; // Supervisor interrupt delegation register
    uint64 sie;     // Supervisor interrupt-enable register
    uint64 stvec;   // Supervisor trap vector base address register
    uint64 scounteren;

    // User trap handling registers
    // 0x40 - 0x44
    uint64 uscratch;
    uint64 uepc;   // User exception program counter
    uint64 ucause; // User trap cause
    uint64 utval;  // User bad address or instruction
    uint64 uip;

    // User trap setup
    // 0x000
    uint64 ustatus; // User status register
    // 0x04 - 0x05
    uint64 uie;   // User interrupt-enable register
    uint64 utvec; // User trap vector base address register

    int priv; // M-Mode = 3, S-Mode = 2, U-Mode = 1
    int is_pmp;
    pagetable_t new_pt;
    pagetable_t org_pt;
};

struct vm_virtual_state vm;
int priv_req = 3;

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

uint64* get_register(uint32 reg, struct vm_virtual_state* vm) {
    int base_reg = 0;
    uint64 base_addr = 0;

    // Machine trap handling registers
    if (reg >= 0x340 && reg <= 0x344)
    {
        base_reg = 0x340;
        base_addr = (uint64)&vm->mscratch;
        priv_req = 3;
    }
    else if (reg >= 0x34A && reg <= 0x34B)
    {
        base_reg = 0x34A;
        base_addr = (uint64)&vm->mtinst;
        priv_req = 3;
    }
    // Machine trap setup registers
    else if (reg >= 0x300 && reg <= 0x306)
    {
        base_reg = 0x300;
        base_addr = (uint64)&vm->mstatus;
        priv_req = 3;
    }
    else if (reg == 0x310)
    {
        base_reg = 0x310;
        base_addr = (uint64)&vm->mstatush;
        priv_req = 3;
    }
    // Machine information registers
    else if (reg >= 0xf11 && reg <= 0xf14)
    {
        base_reg = 0xf11;
        base_addr = (uint64)&vm->mvendorid;
        priv_req = 3;
    }
    // Machine memory protection registers
    else if (reg >= 0x3a0 && reg <= 0x3ef)
    {
        base_reg = 0x3a0;
        base_addr = (uint64)&vm->pmpcfg;
        priv_req = 3;
        if (reg <= base_reg + 15)
        {
            vm->is_pmp = 1;
        }
    }
    // Supervisor page table register
    else if (reg == 0x180)
    {
        base_reg = 0x180;
        base_addr = (uint64)&vm->satp;
        priv_req = 1;
    }
    // Supervisor trap handling registers
    else if (reg >= 0x140 && reg <= 0x144)
    {
        base_reg = 0x140;
        base_addr = (uint64)&vm->sscratch;
        priv_req = 1;
    }
    // Supervisor trap setup registers
    else if (reg == 0x100)
    {
        base_reg = 0x100;
        base_addr = (uint64)&vm->sstatus;
        priv_req = 1;
    }
    else if (reg >= 0x102 && reg <= 0x106)
    {
        base_reg = 0x102;
        base_addr = (uint64)&vm->sedeleg;
        priv_req = 1;
    }
    // User trap handling registers
    else if (reg >= 0x040 && reg <= 0x044)
    {
        base_reg = 0x040;
        base_addr = (uint64)&vm->uscratch;
        priv_req = 1;
    }
    // User trap setup registers
    else if (reg == 0x000)
    {
        base_reg = 0x000;
        base_addr = (uint64)&vm->ustatus;
        priv_req = 1;
    }
    else if (reg >= 0x004 && reg <= 0x005)
    {
        base_reg = 0x004;
        base_addr = (uint64)&vm->uie;
        priv_req = 1;
    }

    return (uint64 *)((reg - base_reg) * 8 + base_addr);
}

uint64* get_vm_trapframe_register(uint32 reg, struct trapframe *tf)
{
    uint64 base_reg = 1;
    uint64 base_addr = (uint64)&tf->ra;
    return (uint64 *)((reg - base_reg) * 8 + base_addr);
}

void map_pt(pagetable_t old, pagetable_t new, uint64 lowerbound, uint64 upperbound){
    pte_t *pte;
    uint64 pa, i;
    uint flags;

    for (i = lowerbound; i < upperbound; i += PGSIZE)
    {
        if ((pte = walk(old, i, 0)) == 0)
            panic("uvmcopy: pte should exist");
        if ((*pte & PTE_V) == 0)
            panic("uvmcopy: page not present");
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);

        if (mappages(new, i, PGSIZE, pa, flags) != 0)
        {
            printf("[DEBUG] Error in map_pt\n");
            uvmunmap(new, lowerbound, (i - lowerbound) / PGSIZE, 1);
            return;
        }
    }

}

void do_ecall(struct proc* p) {
    if(vm.priv == 0) {
        vm.sepc = p->trapframe->epc;
        vm.priv = 1;
        p->trapframe->epc = vm.stvec;
        if(vm.is_pmp == 2) {
            p->pagetable = vm.new_pt;
        }
    } else if(vm.priv == 1) {
        vm.mepc = p->trapframe->epc;
        vm.priv = 3;
        p->trapframe->epc = vm.mtvec;
        if(vm.is_pmp == 2) {
            p->pagetable = vm.org_pt;
        }
    }

}

void do_sret(struct proc* p) {
    if(vm.priv == 1) {
        vm.priv = (int) (vm.sstatus & SSTATUS_SPP) >> 8;
        p->trapframe->epc = vm.sepc;
    } else {
        p->pagetable = vm.org_pt;
        setkilled(p);
    }

}

void do_mret(struct proc* p) {
    if(vm.priv == 3) {
        vm.priv = (int) (vm.mstatus & MSTATUS_MPP_MASK) >> 11;
        p->trapframe->epc = vm.mepc;
        if(vm.is_pmp == 2) {
            p->pagetable = vm.new_pt;
        }
    } else {
        p->pagetable = vm.org_pt;
        setkilled(p);
    }

}

void do_csrw(struct proc* p, uint32 rs1, uint32 uimm) {
    uint64 *src = get_vm_trapframe_register(rs1, p->trapframe);
    uint64 *dest = get_register(uimm, &vm);
    if (vm.priv >= priv_req && uimm != 0xf11){
        *dest = *src;
        p->trapframe->epc += 4;
        if (vm.is_pmp == 1){
            int bit_a = (*dest >> 3) & 1;
            if (bit_a == 1){
                uint64 pmp_addr0 = *(dest + 16);
                pmp_addr0 = pmp_addr0 << 2;
                vm.org_pt = p->pagetable;
                vm.new_pt = proc_pagetable(p);
                // copy_psuedo(vm.org_pt, vm.new_pt, p->sz);
                // map_psuedo(vm.org_pt, vm.new_pt, 0x80000000, PGROUNDUP(pmp_addr0));
                map_pt(vm.org_pt, vm.new_pt, 0x80000000, PGROUNDUP(pmp_addr0));
                vm.is_pmp = 2;
            }
            else{           
                vm.is_pmp = 0;
            }
        }
    }
    else if (vm.priv< priv_req){
        vm.sepc = p->trapframe->epc;  // save pc in SEPC
        vm.priv = 1;       // raise privilege to S
        p->trapframe->epc = vm.stvec; // jump to STVEC
    }
    else{
        p->pagetable = vm.org_pt;
        setkilled(p);
    }
}

void do_csrr(struct proc* p, uint32 rd, uint32 uimm) {
    uint64 *src = get_register(uimm, &vm);
    uint64 *dest = get_vm_trapframe_register(rd, p->trapframe);
    if (vm.priv >= priv_req){
        *dest = *src;
        p->trapframe->epc += 4;
    }
    else {
        vm.sepc = p->trapframe->epc;  // save pc in SEPC
        vm.priv = 1;       // raise privilege to S
        p->trapframe->epc = vm.stvec; // jump to STVEC
    }

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
        do_csrw(p, rs1, uimm);
    }
    else if(funct3==0x2){
        printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
        do_csrr(p, rd, uimm);
    }
    else{
        printf("[DEBUG] Unexpected instruction\n");
        setkilled(p);
    }
    
    if (p->killed) {
        // Perform any necessary cleanup
        printf("[DEBUG] Process killed\n");
        exit(-1); // Terminate the process
    }

    if(vm.mvendorid == 0x0){
        // Graceful Shutdown when VendorID = 0
        setkilled(p);
    }
}

void trap_and_emulate_init(void) {
    /* Create and initialize all state for the VM */
    memset(&vm, 0, sizeof(vm));
    vm.priv = 3;
    vm.mvendorid = 0x637365353336;  // "cse536" in hex

}