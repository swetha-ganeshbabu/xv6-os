# User-Level Thread Context Switch
#
#   void thread_switch(struct thread *old, struct thread *next);
#
# This function saves the current thread's context and switches to
# the next thread's context.
#
# Thread structure layout (relevant fields):
#   offset 0:  tid (int)
#   offset 4:  state (int)
#   offset 8:  stack[STACK_SIZE] (char array)
#   offset 8+STACK_SIZE: sp (void*) - saved stack pointer
#
# With STACK_SIZE = 4096:
#   sp is at offset 4104 (0x1008)
#
# The stack pointer (sp) field stores the thread's saved %esp.
# When switching:
#   1. Push callee-saved registers onto current stack
#   2. Save current %esp into old->sp
#   3. Load next->sp into %esp
#   4. Pop callee-saved registers from new stack
#   5. Return (to wherever the new thread was executing)

# Define offset of sp field in struct thread
# tid(4) + state(4) + stack(4096) = 4104
.set SP_OFFSET, 4104

.globl thread_switch
thread_switch:
    # Get arguments from stack
    # old thread pointer is at 4(%esp)
    # next thread pointer is at 8(%esp)
    movl 4(%esp), %eax      # %eax = old thread
    movl 8(%esp), %edx      # %edx = next thread

    # Save old thread's callee-saved registers
    # x86 calling convention: callee must preserve %ebp, %ebx, %esi, %edi
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    # Save old thread's stack pointer into old->sp
    movl %esp, SP_OFFSET(%eax)

    # Load new thread's stack pointer from next->sp
    movl SP_OFFSET(%edx), %esp

    # Restore new thread's callee-saved registers
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    # Return - this pops the return address and jumps to it
    # For a newly created thread, this will jump to thread_entry
    # For a yielded thread, this returns to where thread_switch was called
    ret
