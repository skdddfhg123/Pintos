#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "filesys/filesys.h"
#include "userprog/process.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// switch (f->R.rax) //syscall number
	// {
	// case SYS_EXIT:
	// 	printf("%s: exit(0)\n",thread_current()->name);
	// 	thread_exit();
	// 	break;
	// case SYS_WRITE:
	// 	putbuf(f->R.rsi,f->R.rdx);
	// default:
	// 	break;
	// }

	if (f->R.rax){
		if (f->R.rax == SYS_EXIT){
			printf("%s: exit(%d)\n", thread_current()->name , f->R.rdi);
			thread_exit();
		}

		if (f->R.rax == SYS_CREATE){

			if (f->R.rdi == NULL){
				// exit(-1)이랑 같음
				printf("%s: exit(%d)\n", thread_current()->name , -1);
				thread_exit();
			}
			filesys_create(f->R.rdi, f->R.rsi);
		}

		if (f->R.rax == SYS_REMOVE){
			filesys_remove(f->R.rdi);
		}

		if (f->R.rax == SYS_OPEN){
			filesys_open(f->R.rdi);
		}

		if (f->R.rax == SYS_READ){
			
		}

		if (f->R.rax == SYS_WRITE){
			putbuf(f->R.rsi,f->R.rdx);
		}
		// if (f->R.rax == SYS_FORK){
		// 	process_fork (const char *name, struct intr_frame *if_ UNUSED);
		// }
		else{
			return;
		}
	}
}
