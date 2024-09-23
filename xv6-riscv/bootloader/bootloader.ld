OUTPUT_ARCH("riscv")

/* Define the starting address of the bootloader binary */
bootloader_start = 0x0000000080000000;

/* Define the entry point */
ENTRY(_entry)

SECTIONS
{
  . = bootloader_start;

  /* Text section */
  .text :
  {
    *(.text)
    *(.text.*)
    ecode = .;
  }

  /* Data section */
  .data :
  {
    *(.data)
    *(.data.*)
    edata = .;
  }

  /* RO data section */
  .rodata :
  {
    *(.rodata)
    *(.rodata.*)
    erodata = .;
  }

  /* BSS section */
  .bss :
  {
    *(.bss)
    *(.bss.*)
    ebss = .;
  }

  /* Define the end of the stack */
  . = ALIGN(16);
  PROVIDE(bl_stack_end = .);

  /* End of Entire Program */
  end = .;
}