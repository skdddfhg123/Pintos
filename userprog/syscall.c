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
#include "filesys/file.h"
#include "threads/synch.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);

int filesize(int fd);
void exiting(int status);
void putbuf(const char * buffer, size_t n);
void checkadd(int file);
int to_fdt(struct file * file);

struct lock syslock;

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

	lock_init(&syslock);
}

int filesize(int fd){
	struct file * file = to_fdt(fd);
	if (fd == NULL) {
		return -1;
	}
	file_length(fd);
}

void exiting(int status){
	thread_current()->exit = status;
	printf("%s: exit(%d)\n", thread_current()->name , status);
	thread_exit();
}

// void putbuf(const char * buffer, size_t n){
// 	acquire_console ();
// 		while (n-- > 0)
// 	putchar_have_lock (*buffer++);
// 	release_console ();
// }

void checkadd(int file){
	if (file == NULL || !(is_user_vaddr(file)) ||pml4_get_page(thread_current()->pml4, file) == NULL){
	exiting(-1);
	}
}

int to_fdt(struct file * file){
	struct thread * t = thread_current();
	struct file ** fdt = t -> fdt;
	int fd = t->file_index;

	while (t->fdt[fd] != NULL && fd < 129){
		fd ++;
	}

	if (fd >= 3){
		return -1;
	}
	t -> fdt = fd;
	fdt[fd] = file;
	return fd;
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

	// if문보다 switch문(O(N))이 더 빠르긴 하지만 원리는 비슷함
	
	if (f->R.rax){
		if (f->R.rax == SYS_EXIT){
			exiting(f->R.rdi);
		}

		if (f->R.rax == SYS_CREATE){
			checkadd(f->R.rdi);
			f->R.rax = filesys_create(f->R.rdi, f->R.rsi);
		}

		if (f->R.rax == SYS_READ){
			// 버퍼 맨 처음 부분 검사
			checkadd(f->R.rsi);
			// 버퍼 맨 끝 부분 검사
			checkadd(f->R.rsi + f->R.rdx -1);
			// rdi : fd / rsi : * buffer / rdx : size
			// 구현
			
			// 우선 fd를 받아서 해당 파일을 가르키는 구조체를 반환시켜야 함.
			struct file * file = to_fdt(f->R.rdi);
			int read_count = 0;
			unsigned char * buffer = f->R.rsi;
			int * size = f->R.rdx;

			if (file == NULL){
				f->R.rax = -1;
			}

			// 만약 fd가 0이라면, input_getc()를 사용해서 키보드에서 읽음
			if (f->R.rdi == STDIN_FILENO){
				char key;
				while (read_count < size){
					key = input_getc();
					*buffer++ = key;
					if (key == '\0'){
						break;
					}
					read_count ++;
				}
			}
			// 만약 fd가 1이라면, STDOUT을 의미하기 때문에 -1을 반환하게 됨
			else if (f->R.rdi == STDOUT_FILENO){
				f->R.rax = -1;
			}

			else {
				lock_acquire(&syslock);
				read_count = file_read(file, buffer , size);
				lock_release(&syslock);
			}

			if (f->R.rax != -1){
				f->R.rax = read_count;
			}
		}

		if (f->R.rax == SYS_OPEN){
			checkadd(f->R.rdi);

			struct file * file = filesys_open(f->R.rdi);	
			// thread의 file descripter table에 추가
			// 이거 안 하면 read/write 안 됨(testcase보면 read안에서 open을 call 함)
			if (file == NULL){
				f->R.rax = -1;
			}
			int fd = to_fdt(file);

			if (fd == -1){
				file_close(file);
			}
			// rax에 file_open에서 나온 값을 추가
			if (f->R.rax != -1){
				f->R.rax = fd;
			}
		}

		if (f->R.rax == SYS_WRITE){
			if (f->R.rdi = STDOUT_FILENO)
				putbuf(f->R.rsi, f->R.rdx);
			f->R.rax = f->R.rdx;
		}

		else{
			return;
		}
	}
}