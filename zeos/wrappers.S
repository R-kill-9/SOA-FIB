#include <asm.h>

ENTRY(syscall_sysenter)
	push %ecx
	push %edx
	push $sys_ret
	push %ebp
	mov %esp, %ebp
	sysenter
ENTRY(sys_ret)
	pop %ebp
	pop %edx
	pop %edx
	pop %ecx
	ret

ENTRY(write)
    pushl %ebp  
    movl  %esp,%ebp

    pushl %ebx # Pusheamos ebx porque en el bucle del test toca la variable i

    movl 0x08(%ebp), %ebx  /** fd->ebx **/
    movl 0x0C(%ebp), %ecx  /** *buffer-> ecx**/
    movl 0x10(%ebp), %edx  /** size -> edx*/

    movl $4, %eax  /**Ponemos el identifier de la system call en EAX**/
    
    pushl %ecx /** El llamador salva los registros**/
    pushl %edx

    pushl $write_return /** Fake dynamic link**/
    pushl %ebp
    mov %esp, %ebp

    sysenter

write_return:
    popl %ebp
    popl %ebx
    popl %edx
    popl %ecx
    popl %ebx

    cmpl $0, %eax /** Miramos si hay error o no **/
    jge write_no_error
    /** Si hay error lo procesamos**/
    neg %eax
    mov %eax, errno
    mov $-1, %eax

write_no_error:
    popl %ebp
    ret




ENTRY(gettime)
    pushl %ebp
	movl %esp, %ebp
	movl $10, %eax
	call syscall_sysenter
	popl %ebp
	ret

ENTRY(getpid)
    pushl %ebp
    movl %esp, %ebp
    movl $20, %eax
    call syscall_sysenter
    popl %ebp
    ret

ENTRY(fork)
    pushl %ebp
    movl %esp, %ebp
    movl $2, %eax 
    call syscall_sysenter
    cmpl $0, %eax /** Miramos si hay error o no **/
    jl perror
    popl %ebp
    ret

ENTRY(exit)
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    call syscall_sysenter
    popl %ebp
    ret
    
ENTRY(get_stats)
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx;
    movl $35, %eax
    movl 0x8(%ebp), %ebx
    movl 0xC(%ebp), %ecx
    call syscall_sysenter
    popl %ebx
    cmpl $0, %eax
    jl perror
    popl %ebp
    ret



perror:
    neg %eax
    mov %eax, errno
    mov $-1, %eax 
    popl %ebp 
    ret