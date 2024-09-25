OUTPUT_ARCH("riscv")

bootloader_start = 0x0000000080000000;

/* Task: ensure the correct starting function. */
ENTRY(_entry)

SECTIONS
{
  . = bootloader_start;

  .text :
  {
    *(.text)
    *(.text.*)
    ecode = .;
  }

  .data :
  {
    *(.data)
    *(.data.*)
    edata = .;
  }

  .rodata :
  {
    *(.rodata)
    *(.rodata.*)
    erodata = .;
  }

  .bss :
  {
    *(.bss)
    *(.bss.*)
    ebss = .;
  }

  end = .;
}