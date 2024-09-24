# _entry should be the first function invoked when
# the QEMU CPU starts executing. It's location should
# according to the memory layout provided in param.h

.section .text
.extern bl_stack_end
.global _entry
_entry:
        # task: setup a per-CPU stack for C and jump to start() function
        la sp, bl_stack_end
        j start
        
spin:
        j spin
