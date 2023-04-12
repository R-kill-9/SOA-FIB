/*
 * entry.S - Entry point to system mode from user mode
 */

#include <asm.h>
#include <segment.h>
#include <errno.h>

/**************************************************/
/**** Save & Restore ******************************/
/**                                              **/
/** When we change to privilege level 0 (kernel) **/
/** (through an interrupt, a system call, an     **/
/** exception ...) we must save the state of the **/
/** currently running task (save).               **/
/**                                              **/
/** Stack layout in 'systemCall':                **/
/**                                              **/
/**   0(%esp) - %ebx    \                        **/
/**   4(%esp) - %ecx     |                       **/
/**   8(%esp) - %edx     |                       **/
/**   C(%esp) - %esi     | Register saved        **/
/**  10(%esp) - %edi     |  by 'save'            **/
/**  14(%esp) - %ebp     |                       **/
/**  18(%esp) - %eax     |                       **/
/**  1C(%esp) - %ds      |                       **/
/**  20(%esp) - %es      |                       **/
/**  24(%esp) - %fs      |                       **/
/**  28(%esp) - %gs     /                        **/
/**  2C(%esp) - %eip    \                        **/
/**  30(%esp) - %cs      |                       **/
/**  34(%esp) - %eflags  |  Return context saved **/
/**  38(%esp) - %oldesp  |   by the processor.   **/
/**  3C(%esp) - %oldss  /                        **/
/**                                              **/
/**************************************************/

#define SAVE_ALL \
      pushl %gs; \
      pushl %fs; \
      pushl %es; \
      pushl %ds; \
      pushl %eax; \
      pushl %ebp; \
      pushl %edi; \
      pushl %esi; \
      pushl %edx; \
      pushl %ecx; \
      pushl %ebx; \
      movl $__KERNEL_DS, %edx;    \
      movl %edx, %ds;           \
      movl %edx, %es

#define RESTORE_ALL \
      popl %ebx; \
      popl %ecx; \
      popl %edx; \
      popl %esi; \
      popl %edi; \
      popl %ebp; \
      popl %eax; \
      popl %ds; \
      popl %es; \
      popl %fs; \
      popl %gs; \


/**EOI (End of interrupt) se utiliza para enviar una señal
al controlador de interrupciones para informar que la interrupción
ha sido manejada y ya se puede continuar con la ejecución normal**/
/** el movb Ox20 es el valor de "finalizacion de interrupción
y el outb 0x20 es el puerto del controlador de interrupciones de sistema**/

#define EOI \
      movb $0x20, %al; \
      outb %al, $0x20;

      
ENTRY(clock_handler)
      SAVE_ALL
      pushl %eax
      call user_to_system
      popl %eax;
      EOI
      call clock_routine
      pushl %eax
      call system_to_user
      popl %eax
      RESTORE_ALL
      iret

ENTRY(keyboard_handler)
      SAVE_ALL
      pushl %eax
      call user_to_system
      popl %eax
      call keyboard_routine
      pushl %eax
      call system_to_user
      popl %eax
      EOI
      RESTORE_ALL
      iret
# Para la clock ponemos EOI antes de la llamada

ENTRY(page_fault2_handler)
     # SAVE_ALL
      call page_fault2_routine
     # EOI
     # RESTORE_ALL
    # iret

ENTRY (system_call_handler)
      SAVE_ALL          /* Save the current context */
      cmpl $0, %EAX     # Is syscall number negative?
      jl err            # If it is, jump to return an error
      cmpl $MAX_SYSCALL, %EAX # Is syscall greater than MAX_SYSCALL (4)?
      jg err            # If it is, jump to return an error
      call *sys_call_table(, %EAX, 0x04) # Call the corresponding service routine
      jmp fin           # Finish
 err:
      movl $-ENOSYS, %EAX # Move to EAX the ENOSYS error
 fin:
      movl %EAX, 0x18(%esp) # Change the EAX value in the stack
      RESTORE_ALL           # Restore the context
      iret



ENTRY(syscall_handler_sysenter)
      push $__USER_DS
      push %EBP # User stack address
      pushfl
      push $__USER_CS
      push 4(%EBP) # User return address
      SAVE_ALL
      cmpl $0, %EAX
      jl sysenter_err
      cmpl $MAX_SYSCALL, %EAX
      jg sysenter_err
      call *sys_call_table(, %EAX, 0x04)
      jmp sysenter_fin
sysenter_err:
      movl $-ENOSYS, %EAX
sysenter_fin:
      movl %EAX, 0x18(%ESP)
      RESTORE_ALL
      movl (%ESP), %EDX # Return address
      movl 12(%ESP), %ECX # User stack address
      sti # Enable interrupts again
      sysexit


ENTRY(writeMSR)
      pushl %ebp
      movl  %esp, %ebp
      movl 0x08(%ebp), %ecx
      movl 0x0C(%ebp), %edx
      movl 0x10(%ebp), %eax
      wrmsr 
      movl %ebp, %esp
      popl %ebp
      ret


ENTRY(task_switch)
      pushl %ebp
	movl %esp, %ebp
	pushl %esi
	pushl %edi
	pushl %ebx
	pushl 8(%ebp)
	call inner_task_switch
	addl $4, %esp
	popl %ebx
	popl %edi
	popl %esi
	popl %ebp
	ret
      


ENTRY(stack_swap)
      mov 4(%esp), %eax 
      mov %ebp, (%eax) # kernel_esp <- ebp
      mov 8(%esp), %esp # new.kernel_esp -> esp
      popl %ebp
      ret

