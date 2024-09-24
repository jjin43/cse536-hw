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
    // Point an ELF struct to RAMDISK to initialize it with the kernel binary’s ELF header
    if(ktype == NORMAL) {
        kernel_elfhdr = (struct elfhdr*)RAMDISK;
    } else if(ktype == RECOVERY) {
        kernel_elfhdr = (struct elfhdr*)RECOVERYDISK;
    }

    // Retrieve the offset and size of the program headers
    uint64 phoff = kernel_elfhdr->phoff;
    uint64 phentsize = kernel_elfhdr->phentsize;

    // Calculate the address of the second program header section
    uint64 second_phdr_addr = RAMDISK + phoff + phentsize;

    // Point a program header struct to this address to initialize it with the .text section’s header
    kernel_phdr = (struct proghdr*)second_phdr_addr;

    // Retrieve the starting address of the .text section
    uint64 kernload_start = kernel_phdr->vaddr;

    return kernload_start;
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

    print(kernel_elfhdr->entry, kernel_elfhdr->magic);
    // Retrieve the entry point address from the ELF header
    uint64 entry_addr = kernel_elfhdr->entry;

    return entry_addr;
}