#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// Struct to keep VM registers (Sample; feel free to change.)
struct vm_reg
{
    int code;
    int mode; // 0 machine 1 supervisor , 2 user modes
    uint64 val;
};

// Keep the virtual state of the VM's privileged registers
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

    uint64 priviledge_mode; // M-Mode = 3, S-Mode = 2, U-Mode = 1
    int allowed_mode;
    int pmp_setup;
    pagetable_t psuedo_pagetable;
    pagetable_t og_pagetable;
} vm;

int allowed_mode = 3;

uint64 *get_vm_privileged_register(uint32 reg, struct vm_virtual_state *vm)
{
    int base_reg = 0;
    uint64 base_addr = 0;

    // Machine trap handling registers
    if (reg >= 0x340 && reg <= 0x344)
    {
        base_reg = 0x340;
        base_addr = (uint64)&vm->mscratch;
        allowed_mode = 3;
    }
    else if (reg >= 0x34A && reg <= 0x34B)
    {
        base_reg = 0x34A;
        base_addr = (uint64)&vm->mtinst;
        allowed_mode = 3;
    }
    // Machine trap setup registers
    else if (reg >= 0x300 && reg <= 0x306)
    {
        base_reg = 0x300;
        base_addr = (uint64)&vm->mstatus;
        allowed_mode = 3;
    }
    else if (reg == 0x310)
    {
        base_reg = 0x310;
        base_addr = (uint64)&vm->mstatush;
        allowed_mode = 3;
    }
    // Machine information registers
    else if (reg >= 0xf11 && reg <= 0xf14)
    {
        base_reg = 0xf11;
        base_addr = (uint64)&vm->mvendorid;
        allowed_mode = 3;
    }
    // Machine memory protection registers
    else if (reg >= 0x3a0 && reg <= 0x3ef)
    {
        base_reg = 0x3a0;
        base_addr = (uint64)&vm->pmpcfg;
        allowed_mode = 3;
        if (reg <= base_reg + 15)
        {
            vm->pmp_setup = 1;
        }
    }
    // Supervisor page table register
    else if (reg == 0x180)
    {
        base_reg = 0x180;
        base_addr = (uint64)&vm->satp;
        allowed_mode = 1;
    }
    // Supervisor trap handling registers
    else if (reg >= 0x140 && reg <= 0x144)
    {
        base_reg = 0x140;
        base_addr = (uint64)&vm->sscratch;
        allowed_mode = 1;
    }
    // Supervisor trap setup registers
    else if (reg == 0x100)
    {
        base_reg = 0x100;
        base_addr = (uint64)&vm->sstatus;
        allowed_mode = 1;
    }
    else if (reg >= 0x102 && reg <= 0x106)
    {
        base_reg = 0x102;
        base_addr = (uint64)&vm->sedeleg;
        allowed_mode = 1;
    }
    // User trap handling registers
    else if (reg >= 0x040 && reg <= 0x044)
    {
        base_reg = 0x040;
        base_addr = (uint64)&vm->uscratch;
        allowed_mode = 1;
    }
    // User trap setup registers
    else if (reg == 0x000)
    {
        base_reg = 0x000;
        base_addr = (uint64)&vm->ustatus;
        allowed_mode = 1;
    }
    else if (reg >= 0x004 && reg <= 0x005)
    {
        base_reg = 0x004;
        base_addr = (uint64)&vm->uie;
        allowed_mode = 1;
    }
    return (uint64 *)((reg - base_reg) * 8 + base_addr);
}

uint64 *get_vm_trapframe_register(uint32 reg, struct trapframe *tf)
{
    uint64 base_reg = 1;
    uint64 base_addr = (uint64)&tf->ra;
    return (uint64 *)((reg - base_reg) * 8 + base_addr);
}

// In your ECALL, add the following for prints
// struct proc* p = myproc();
// printf("(EC at %p)\n", p->trapframe->epc);

int copy_psuedo(pagetable_t old, pagetable_t new, uint64 sz)
{
    pte_t *pte;
    uint64 pa, i;
    uint flags;
    //   char *mem;

    for (i = 0; i < sz; i += PGSIZE)
    {
        if ((pte = walk(old, i, 0)) == 0)
            panic("uvmcopy: pte should exist");
        if ((*pte & PTE_V) == 0)
            panic("uvmcopy: page not present");
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);

        // if((mem = kalloc()) == 0)
        //   goto err;
        // memmove(mem, (char*)pa, PGSIZE);
        if (mappages(new, i, PGSIZE, pa, flags) != 0)
        {
            printf("------ERROR");
            goto err;
        }
    }
    return 0;

err:
    uvmunmap(new, 0, i / PGSIZE, 1);
    return -1;
}

int map_psuedo(pagetable_t old, pagetable_t new, uint64 lowerbound, uint64 upperbound)
{

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

        // if((mem = kalloc()) == 0)
        //   goto err;
        // memmove(mem, (char*)pa, PGSIZE);
        if (mappages(new, i, PGSIZE, pa, flags) != 0)
        {
            printf("------ERROR");
            goto err;
        }
    }

    return 0;
err:
    uvmunmap(new, 0, i / PGSIZE, 1);
    return -1;
}

void trap_and_emulate(void)
{
    /* Comes here when a VM tries to execute a supervisor instruction. */
    struct proc *p = myproc();

    /* Retrieve all required values from the instruction */
    uint64 addr = r_sepc();
    char *pa = kalloc();
    copyin(p->pagetable, pa, addr, PGSIZE);
    uint32 inst = *(uint32 *)pa;
    uint32 op = (inst)&0x7F;
    uint32 rd = (inst >> 7) & 0x1F;
    uint32 funct3 = (inst >> 12) & 0x7;
    uint32 rs1 = (inst >> 15) & 0x1F;
    uint32 uimm = (inst) >> 20 & 0xFFF;
    // https://five-embeddev.com/quickref/interrupts.html
    // https://jborza.com/emulation/2021/04/22/ecalls-and-syscalls.html
    switch (funct3)
    {
    case 0x0:
        if (rd == 0x0 && rs1 == 0x0)
        {
            if (uimm == 0x0 && p->proc_te_vm == 1) // ECALL
            {
                printf("(EC at %p)\n", p->trapframe->epc);
                if (vm.priviledge_mode == 0)
                {
                    vm.sepc = p->trapframe->epc;  // save pc in SEPC
                    vm.priviledge_mode = 1;       // raise privilege to S
                    p->trapframe->epc = vm.stvec; // jump to STVEC
                    if (vm.pmp_setup == 2)
                        p->pagetable = vm.psuedo_pagetable;
                }
                else if (vm.priviledge_mode == 1)
                {
                    vm.mepc = p->trapframe->epc;  // save pc in MEPC
                    vm.priviledge_mode = 3;                  // raise privilege to M
                    p->trapframe->epc = vm.mtvec; // jump to MTVEC
                    if (vm.pmp_setup == 2)
                        p->pagetable = vm.og_pagetable;
                }
            }
            else if (uimm == 0x102 && vm.priviledge_mode == 1) // SRET
            {
                printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
                vm.priviledge_mode = (vm.sstatus & SSTATUS_SPP) >> 8;
                p->trapframe->epc = vm.sepc;
            }
            else if (uimm == 0x302 && vm.priviledge_mode == 3) // MRET
            {
                printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
                vm.priviledge_mode = (vm.mstatus & MSTATUS_MPP_MASK) >> 11;
                p->trapframe->epc = vm.mepc;
                if (vm.pmp_setup == 2)
                {
                    p->pagetable = vm.psuedo_pagetable;
                }
            }
            else
            {
                printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
                                p->pagetable = vm.og_pagetable;

                setkilled(p);
            }
        }
        else
        {
            printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
                            p->pagetable = vm.og_pagetable;

            setkilled(p);
        }
        break;
    case 0x1:
        if (rd == 0x0 && uimm != 0xf11) // CSRW
        {
            printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
            uint64 *src = get_vm_trapframe_register(rs1, p->trapframe);
            uint64 *dest = get_vm_privileged_register(uimm, &vm);
            // printf("-- allowed mode %d",allowed_mode);
            // printf(" vm : %d",vm.priviledge_mode);
            if (vm.priviledge_mode >= allowed_mode && uimm != 0xf11)
            {
                *dest = *src;
                p->trapframe->epc += 4;
                if (vm.pmp_setup == 1)
                {
                    int bit_a = (*dest >> 3) & 1;
                    if (bit_a == 1)
                    {
                        uint64 pmp_addr0 = *(dest + 16);
                        pmp_addr0 = pmp_addr0 << 2;
                        vm.og_pagetable = p->pagetable;
                        vm.psuedo_pagetable = proc_pagetable(p);
                        copy_psuedo(vm.og_pagetable, vm.psuedo_pagetable, p->sz);
                        map_psuedo(vm.og_pagetable, vm.psuedo_pagetable, 0x80000000, PGROUNDUP(pmp_addr0));
                        vm.pmp_setup = 2;
                    }
                    else
                    {           
                        vm.pmp_setup = 0;
                    }
                }
            }
            else if (vm.priviledge_mode < allowed_mode)
            {
                vm.sepc = p->trapframe->epc;  // save pc in SEPC
                vm.priviledge_mode = 1;       // raise privilege to S
                p->trapframe->epc = vm.stvec; // jump to STVEC
            }else{
                printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
                p->pagetable = vm.og_pagetable;
                setkilled(p);
            }
        }
        else
        {
            printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
            p->pagetable = vm.og_pagetable;
            setkilled(p);
        }
        break;
    case 0x2:
        if (rs1 == 0x0) // CSRR
        {
            
            printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n",
                   addr, op, rd, funct3, rs1, uimm);
            uint64 *src = get_vm_privileged_register(uimm, &vm);
            uint64 *dest = get_vm_trapframe_register(rd, p->trapframe);
            if (vm.priviledge_mode >= allowed_mode)
            {
                *dest = *src;
                p->trapframe->epc += 4;
            }
            else
            {
                vm.sepc = p->trapframe->epc;  // save pc in SEPC
                vm.priviledge_mode = 1;       // raise privilege to S
                p->trapframe->epc = vm.stvec; // jump to STVEC
            }
        }
        else
        {
            printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", addr, op, rd, funct3, rs1, uimm);
            p->pagetable = vm.og_pagetable;
            // printf("killing CSRR1");
            setkilled(p);
        }
        break;
    default:
        break;
    }

    /* Print the statement */
    // printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n",addr, op, rd, funct3, rs1, uimm);
}

void trap_and_emulate_init(void)
{
    /* Create and initialize all state for the VM */
    // Initialize VM state struct
    //  vm = { 0 };   // Set all registers to zero
    vm.mvendorid = 0xc5e536; // Set mvendorid to "cse536" in hexadecimal
    vm.priviledge_mode = 3;
}