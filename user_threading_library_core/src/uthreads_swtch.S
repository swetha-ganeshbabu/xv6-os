/*
 * uthreads_swtch.S - User-Level Thread Context Switching for xv6
 *
 * This file implements the low-level context switch between user-level threads.
 * It saves and restores the callee-saved registers and stack pointer.
 *
 * x86 Calling Convention (cdecl):
 * - Caller-saved registers: EAX, ECX, EDX (not saved here)
 * - Callee-saved registers: EBX, ESI, EDI, EBP (must be preserved)
 * - Arguments passed on stack, right to left
 * - Return value in EAX
 *
 * Stack Layout After Saving Context:
 *   Higher addresses
 *   +----------------+
 *   | return addr    | <- Original ESP + 0
 *   +----------------+
 *   | old (arg 1)    | <- ESP + 4
 *   +----------------+
 *   | next (arg 2)   | <- ESP + 8
 *   +----------------+
 *   |    ...         |
 *   +----------------+
 *
 *   After pushing callee-saved:
 *   +----------------+
 *   | EBP            | <- First pushed
 *   +----------------+
 *   | EBX            |
 *   +----------------+
 *   | ESI            |
 *   +----------------+
 *   | EDI            | <- ESP points here (lowest)
 *   +----------------+
 *
 * Thread Structure Layout (must match struct thread in uthreads.h):
 *   Offset 0:  tid (int, 4 bytes)
 *   Offset 4:  state (enum, 4 bytes)
 *   Offset 8:  stack (char*, 4 bytes)
 *   Offset 12: sp (char*, 4 bytes)  <- THIS IS WHAT WE SAVE/RESTORE
 *   ...
 */

.text
.globl thread_switch

/*
 * thread_switch(struct thread *old, struct thread *next)
 *
 * Save current thread's context and switch to next thread.
 *
 * Parameters (on stack):
 *   4(%esp)  - old: pointer to current thread's struct thread
 *   8(%esp)  - next: pointer to next thread's struct thread
 *
 * The sp field is at offset 12 in struct thread.
 */
thread_switch:
    /* Get the 'old' thread pointer into EAX */
    movl 4(%esp), %eax

    /* Get the 'next' thread pointer into EDX */
    movl 8(%esp), %edx

    /*
     * Save callee-saved registers on current stack.
     * These must be saved because the C calling convention
     * requires them to be preserved across function calls.
     */
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    /*
     * Save current stack pointer to old->sp
     * The sp field is at offset 12 in struct thread
     */
    movl %esp, 12(%eax)

    /*
     * Load next thread's stack pointer from next->sp
     * This switches us to the new thread's stack
     */
    movl 12(%edx), %esp

    /*
     * Restore callee-saved registers from new stack.
     * These were saved by a previous call to thread_switch
     * from this thread.
     */
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    /*
     * Return to the new thread.
     * The return address on the new thread's stack will either be:
     * 1. The address after thread_switch was called (if resuming)
     * 2. thread_entry (if this is a new thread's first run)
     */
    ret


/*
 * thread_entry - Entry point for new threads
 *
 * This function is called when a new thread starts executing for the first time.
 * The stack has been set up by thread_create to contain:
 *   - thread_exit address (for when start_routine returns)
 *   - arg (argument to start_routine)
 *   - start_routine address
 *
 * Stack layout when we get here (after pops in thread_switch):
 *   +------------------+
 *   | thread_exit      | <- will be return address when start_routine returns
 *   +------------------+
 *   | arg              | <- argument for start_routine
 *   +------------------+
 *   | start_routine    | <- ESP points here
 *   +------------------+
 *
 * We need to:
 * 1. Pop start_routine into a register
 * 2. Call it (arg is already on stack in right position)
 * 3. When it returns, thread_exit will be called with the return value
 */
.globl thread_entry
thread_entry:
    /* Pop the start_routine address into EAX */
    popl %eax

    /*
     * Now ESP points to arg, and above it is thread_exit.
     * Call the start routine - when we call, the return address
     * will be the next value on stack (thread_exit_wrapper)
     */
    call *%eax

    /*
     * If start_routine returns normally, EAX contains its return value.
     * We need to call thread_exit with this value.
     * Push the return value as the argument to thread_exit.
     */
    pushl %eax
    call thread_exit

    /* thread_exit never returns, but just in case: */
    hlt
