# Add stuff too
kern/arch/mips/syscall/syscall.c
kern/arch/mips/locore/trap.c
kern/include/file.h

# Implement in
syscall/file.c

# Test file
userland/testbin/ass2/asst.c

# Need to look at
kern/include/vfs.h
kern/include/vnode.h

# Important files
kern/arch/mips/include/trapframe.h
kern/vm/copyinout.c

# Loading and running programs
kern/syscall/loadelf.c
kern/syscall/runprogram.c
