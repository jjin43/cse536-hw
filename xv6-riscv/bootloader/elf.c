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

    // get num of program headers
    uint64 phnum = kernel_elfhdr->phnum;
    uint64 phoff = kernel_elfhdr->phoff;
    uint64 phentsize = kernel_elfhdr->phentsize;

    uint64 max_size = 0;

    // find the maximum of the p_offset + p_filesz values
    for (uint64 i = 0; i < phnum; i++) {
        kernel_phdr = (struct proghdr*)(kaddr + phoff + i * phentsize);
        uint64 size = kernel_phdr->off + kernel_phdr->filesz;
        if (size > max_size) {
            max_size = size;
        }
    }

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