#include "types.h"
#include "param.h"
#include "layout.h"
#include "riscv.h"
#include "defs.h"
#include "buf.h"
#include "elf.h"
#include <stdbool.h>

struct elfhdr* kernel_elfhdr;
struct proghdr* kernel_phdr;

uint64 find_kernel_load_addr(enum kernel ktype) {
    // Point to the ELF header in RAMDISK or RECOVERYDISK
    if (ktype == NORMAL) {
        kernel_elfhdr = (struct elfhdr*)RAMDISK;
    } else if (ktype == RECOVERY) {
        kernel_elfhdr = (struct elfhdr*)RECOVERYDISK;
    }

    // Retrieve the program header offset and number of program headers
    struct proghdr* ph = (struct proghdr*)((uint64)kernel_elfhdr + kernel_elfhdr->phoff);
    
    // Iterate through program headers to find the one corresponding to the .text section
    for (int i = 0; i < kernel_elfhdr->phnum; i++, ph++) {
        // Check if the program header is of type "LOAD" and if it is executable
        if (ph->type == ELF_PROG_LOAD && (ph->flags & ELF_PROG_FLAG_EXEC)) {
            // Return the virtual address where the .text section should be loaded
            return ph->vaddr;  // This is the virtual address for the .text section
        }
    }
    
    // If no executable section is found, return an error or invalid address
    panic("No loadable .text section found");
    return 0;
}

uint64 find_kernel_size(enum kernel ktype) {
    // Point an ELF struct to RAMDISK to initialize it with the kernel binary’s ELF header
    if(ktype == NORMAL) {
        kernel_elfhdr = (struct elfhdr*)RAMDISK;
    } else if(ktype == RECOVERY) {
        kernel_elfhdr = (struct elfhdr*)RECOVERYDISK;
    }

    // Retrieve the number of program headers
    uint64 phnum = kernel_elfhdr->phnum;
    uint64 phoff = kernel_elfhdr->phoff;
    uint64 phentsize = kernel_elfhdr->phentsize;

    uint64 max_size = 0;

    // Iterate through each program header to find the maximum of the p_offset + p_filesz values
    for (uint64 i = 0; i < phnum; i++) {
        kernel_phdr = (struct proghdr*)(RAMDISK + phoff + i * phentsize);
        uint64 size = kernel_phdr->off + kernel_phdr->filesz;
        if (size > max_size) {
            max_size = size;
        }
    }

    return max_size;
}

uint64 find_kernel_entry_addr(enum kernel ktype) {
    // Point an ELF struct to RAMDISK to initialize it with the kernel binary’s ELF header
    if(ktype == NORMAL) {
        kernel_elfhdr = (struct elfhdr*)RAMDISK;
    } else if(ktype == RECOVERY) {
        kernel_elfhdr = (struct elfhdr*)RECOVERYDISK;
    }

    // Retrieve the entry point address from the ELF header
    uint64 entry_addr = kernel_elfhdr->entry;

    return entry_addr;
}