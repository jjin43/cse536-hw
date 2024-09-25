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
    // point elf struct
    uint64 kaddr = 0;
    if(ktype == NORMAL) {
        kernel_elfhdr = (struct elfhdr*)RAMDISK;
        kaddr = RAMDISK;
    } else if(ktype == RECOVERY) {
        kernel_elfhdr = (struct elfhdr*)RECOVERYDISK;
        kaddr = RECOVERYDISK;
    }

    uint64 phoff = kernel_elfhdr->phoff;
    uint64 phentsize = kernel_elfhdr->phentsize;

    // calc addr of second program header section
    uint64 phdr_addr_2 = kaddr + phoff + phentsize;

    kernel_phdr = (struct proghdr*)phdr_addr_2;
    uint64 kernel_text_addr = kernel_phdr->vaddr;

    return kernel_text_addr;
}


uint64 find_kernel_size(enum kernel ktype) {
    uint64 kaddr = 0;
    if(ktype == NORMAL) {
        kernel_elfhdr = (struct elfhdr*)RAMDISK;
        kaddr = RAMDISK;
    } else if(ktype == RECOVERY) {
        kernel_elfhdr = (struct elfhdr*)RECOVERYDISK;
        kaddr = RECOVERYDISK;
    }

    // Step 1: Program Headers
    // Get the number of program headers
    uint64 phnum = kernel_elfhdr->phnum;
    uint64 phoff = kernel_elfhdr->phoff;
    uint64 phentsize = kernel_elfhdr->phentsize;

    uint64 max_size = 0;

    // Iterate through each program header to find the end of the last segment
    for (uint64 i = 0; i < phnum; i++) {
        struct proghdr *prog_hdr = (struct proghdr *)(kaddr + phoff + i * phentsize);
        uint64 segment_end = prog_hdr->off + prog_hdr->filesz;
        if (segment_end > max_size) {
            max_size = segment_end;
        }
    }

    // Step 2: Section Headers
    // Get the section header offset, number, and entry size
    uint64 shoff = kernel_elfhdr->shoff;
    uint64 shnum = kernel_elfhdr->shnum;
    uint64 shentsize = kernel_elfhdr->shentsize;

    // Calculate the total size of the section headers
    uint64 section_end = shoff + (shnum * shentsize);
    
    // Update max_size if the section headers extend beyond the last segment
    if (section_end > max_size) {
        max_size = section_end;
    }

    // Return the total size based on both program headers and section headers
    return max_size;
}



uint64 find_kernel_entry_addr(enum kernel ktype) {
    if(ktype == NORMAL) {
        kernel_elfhdr = (struct elfhdr*)RAMDISK;
    } else if(ktype == RECOVERY) {
        kernel_elfhdr = (struct elfhdr*)RECOVERYDISK;
    }

    uint64 entry_addr = kernel_elfhdr->entry;
    return entry_addr;
}