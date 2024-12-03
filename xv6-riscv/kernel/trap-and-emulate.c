#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

/*
Struct to keep VM registers (Sample; feel free to change.)
struct vm_reg {
    int     code;
    int     mode;
    uint64  val;
};
*/

struct vm_virtual_state
{
    // User trap setup
    // User trap handling
    // Supervisor trap setup
    // User trap handling
    // Supervisor page table register
    // Machine information registers
    // Machine trap setup registers
    // Machine trap handling registers

    // 0x340 - 0x344
    uint64 mscratch;
    uint64 mepc;
    uint64 mcause;
    uint64 mtval;
    uint64 mip;

    // 0x34A - 0x34B
    uint64 mtinst;
    uint64 mtval2;

    //  0x300 - 0x306
    uint64 mstatus;
    uint64 misa;
    uint64 medeleg;
    uint64 mideleg;
    uint64 mie;
    uint64 mtvec;
    uint64 mcounteren;
    //  0x310
    uint64 mstatush;

    //  0xF11 - 0xF14
    uint64 mvendorid;
    uint64 marchid;
    uint64 mimpid;
    uint64 mhartid;

    // 0x3A0 - 0x3EF
    uint64 pmpcfg[16];
    uint64 pmpaddr[64];

    //  0x180
    uint64 satp;

    //  0x140 - 0x144
    uint64 sscratch;
    uint64 sepc;
    uint64 scause;
    uint64 stval;
    uint64 sip;

    //  0x100
    uint64 sstatus;
    //  0x102 - 0x106
    uint64 sedeleg;
    uint64 sideleg;
    uint64 sie;
    uint64 stvec;
    uint64 scounteren;

    // 0x40 - 0x44
    uint64 uscratch;
    uint64 uepc;
    uint64 ucause;
    uint64 utval;
    uint64 uip;

    // 0x000
    uint64 ustatus;
    // 0x04 - 0x05
    uint64 uie;
    uint64 utvec;

    int priv; // M-Mode = 3, S-Mode = 2, U-Mode = 1
    int is_pmp;
    pagetable_t new_pt;
    pagetable_t org_pt;
};

struct vm_virtual_state vm;
int priv_req = 3;
int pmp_pages = 0;

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

uint64* get_register(uint32 uimm, struct vm_virtual_state* vm) {
    int base_reg = 0;
    uint64 base_addr = 0;

    // Machine trap handling registers
    if (uimm >= 0x340 && uimm <= 0x344){
        base_reg = 0x340;
        base_addr = (uint64)&vm->mscratch;
        priv_req = 3;
    }
    else if (uimm >= 0x34A && uimm <= 0x34B){
        base_reg = 0x34A;
        base_addr = (uint64)&vm->mtinst;
        priv_req = 3;
    }
    // Machine trap setup registers
    else if (uimm >= 0x300 && uimm <= 0x306){
        base_reg = 0x300;
        base_addr = (uint64)&vm->mstatus;
        priv_req = 3;
    }
    else if (uimm == 0x310){
        base_reg = 0x310;
        base_addr = (uint64)&vm->mstatush;
        priv_req = 3;
    }
    // Machine information registers
    else if (uimm >= 0xf11 && uimm <= 0xf14){
        base_reg = 0xf11;
        base_addr = (uint64)&vm->mvendorid;
        priv_req = 3;
    }
    // Machine memory protection registers
    else if (uimm >= 0x3a0 && uimm <= 0x3ef){
        base_reg = 0x3a0;
        base_addr = (uint64)&vm->pmpcfg;
        priv_req = 3;
        if (uimm <= base_reg + 15)
        {
            vm->is_pmp = 1;
        }
    }
    // Supervisor page table register
    else if (uimm == 0x180){
        base_reg = 0x180;
        base_addr = (uint64)&vm->satp;
        priv_req = 1;
    }
    // Supervisor trap handling registers
    else if (uimm >= 0x140 && uimm <= 0x144){
        base_reg = 0x140;
        base_addr = (uint64)&vm->sscratch;
        priv_req = 1;
    }
    // Supervisor trap setup registers
    else if (uimm == 0x100){
        base_reg = 0x100;
        base_addr = (uint64)&vm->sstatus;
        priv_req = 1;
    }
    else if (uimm >= 0x102 && uimm <= 0x106){
        base_reg = 0x102;
        base_addr = (uint64)&vm->sedeleg;
        priv_req = 1;
    }
    // User trap handling registers
    else if (uimm >= 0x040 && uimm <= 0x044){
        base_reg = 0x040;
        base_addr = (uint64)&vm->uscratch;
        priv_req = 1;
    }
    // User trap setup registers
    else if (uimm == 0x000){
        base_reg = 0x000;
        base_addr = (uint64)&vm->ustatus;
        priv_req = 1;
    }
    else if (uimm >= 0x004 && uimm <= 0x005){
        base_reg = 0x004;
        base_addr = (uint64)&vm->uie;
        priv_req = 1;
    }

    return (uint64 *)((uimm - base_reg) * 8 + base_addr);
}

void map_pt(pagetable_t from_pt, pagetable_t to_pt, uint64 lower, uint64 upper){
    
    // map pages only from lower to upper bounds
    pte_t *pte;
    uint64 pa;
    uint flags;

    for (uint64 i = lower; i < upper; i += PGSIZE){
        if ((pte = walk(from_pt, i, 0)) == 0){
            printf("[DEBUG] map_pt - walk failed\n");
            return;
        }
        if ((*pte & PTE_V) == 0){
            printf("[DEBUG] map_pt - page not found\n");
            return;
        }

        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);

        if (mappages(to_pt, i, PGSIZE, pa, flags) != 0){
            printf("[DEBUG] map_pt - error when mapping\n");
            uvmunmap(to_pt, lower, (i - lower) / PGSIZE, 1);
            return;
        }
    }

}

void do_ecall(struct proc* p) {
    if (vm.priv == 0){
        // if U-Mode, rase priv, jump to Supervisor
        vm.sepc = p->trapframe->epc;
        vm.priv = 1;
        p->trapframe->epc = vm.stvec;

        // check pmp
        if(pmp_pages) {
            p->pagetable = vm.new_pt;
        }
    }
    else if (vm.priv == 1){
        // if S-Mode, rase priv, jump to Machine
        vm.mepc = p->trapframe->epc;
        vm.priv = 3;
        p->trapframe->epc = vm.mtvec;

        // check pmp
        if(pmp_pages) {
            p->pagetable = vm.org_pt;
        }
    }

}

void do_sret(struct proc* p) {
    if (vm.priv == 1){

        // fetch prev privlege, from Sstatus, to Sepc
        vm.priv = (int) (vm.sstatus & SSTATUS_SPP) >> 8;
        p->trapframe->epc = vm.sepc;
    }
    else{
        // no priv
        p->pagetable = vm.org_pt;
        setkilled(p);
    }

}

void do_mret(struct proc* p){
    if(vm.priv == 3){

        // fetch prev privlege, from Mstatus, to Mepc
        vm.priv = (int) (vm.mstatus & MSTATUS_MPP_MASK) >> 11;
        p->trapframe->epc = vm.mepc;

        // check if pmp
        if(pmp_pages) {
            p->pagetable = vm.new_pt;
        }
    }
    else{
        // no priv
        p->pagetable = vm.org_pt;
        setkilled(p);
    }

}

void do_csrw(struct proc* p, uint32 rs1, uint32 uimm) {

    // get registers
    uint64 ba = (uint64)&p->trapframe->ra;
    uint64 *addr_from = (uint64 *)((rs1 - 1) * 8 + ba);
    uint64 *addr_to = get_register(uimm, &vm);

    if (vm.priv >= priv_req) {
        *addr_to = *addr_from;  // copy content
        p->trapframe->epc += 4; // next instr

        if (vm.is_pmp == 1){
            // if pmp, check vm flag bit
            int flag_bit = (*addr_to >> 3) & 1;

            if (flag_bit == 1){
                // get pmp addr
                uint64 pmp_addr0 = (*(addr_to + 16)) << 2;

                // map pages
                vm.org_pt = p->pagetable;
                vm.new_pt = proc_pagetable(p);
                
                map_pt(vm.org_pt, vm.new_pt, 0x80000000, PGROUNDUP(pmp_addr0));
                pmp_pages = 1;
            }
            else{
                // reset pmp
                pmp_pages = 0;
                vm.is_pmp = 0;
            }
        }
    }
    else if (vm.priv < priv_req){

        // save and raise priv
        vm.priv = 1;
        vm.sepc = p->trapframe->epc;
        p->trapframe->epc = vm.stvec;
    }
    else{
        p->pagetable = vm.org_pt;
        setkilled(p);
    }
}

void do_csrr(struct proc* p, uint32 rd, uint32 uimm) {
    uint64 *addr_from = get_register(uimm, &vm);
    uint64 ba = (uint64)&p->trapframe->ra;
    uint64 *addr_to = (uint64 *)((rd - 1) * 8 + ba);

    // check priv
    if (vm.priv >= priv_req){
        *addr_to = *addr_from;  // copy content
        p->trapframe->epc += 4; // next instr
    }
    else{
        // save and raise priv
        vm.sepc = p->trapframe->epc;
        vm.priv = 1;
        p->trapframe->epc = vm.stvec;
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

    if(p->killed){
        printf("[DEBUG] Process killed\n");
        p->parent->state = RUNNABLE;

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