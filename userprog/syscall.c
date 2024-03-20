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
#include "lib/kernel/console.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void check_address(void *addr);
void half(void);
void exit(int status);
tid_t fork (const char *thread_name,struct intr_frame *f);
int exec (const char *cmd_line);
int wait (tid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size) ;
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

int filesize(int fd);
void putbuf(const char * buffer, size_t n);
void checkadd(void * file);
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

void checkadd(void * file){
	if (file == NULL || !(is_user_vaddr(file)) ||pml4_get_page(thread_current()->pml4, file) == NULL){
	exit(-1);
	}
}

int to_fdt(struct file * file){
	struct thread * t = thread_current();
	struct file ** fdt = t -> fdt;
	int fd = t->file_index;

	while (t->file_index < FDT_COUNT_LIMIT && fdt[t->file_index]){
		t->file_index++;
	}

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

struct file * fd_to_file(int fd){
	if (fd < 0 || fd >= 129) {
		return NULL;
	}
	
	struct thread *t = thread_current();
	struct file ** fdt = t->fdt;
	
	struct file * file = fdt[fd];
	return file;
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	uint64_t sys_number = f->R.rax;
	
    switch (sys_number)
    {
		case SYS_HALT:
				halt();
				break;
		case SYS_EXIT:
				exit(f->R.rdi); //
				break;
		// case SYS_FORK:
		// 		f->R.rax = fork(f->R.rdi, f); 
		// 		break;
		// case SYS_EXEC:
		// 		exec(f->R.rdi);
		// 		break;
		// case SYS_WAIT:
		// 		f->R.rax = wait(f->R.rdi);
		// 		break;
		case SYS_CREATE:
				f->R.rax = create(f->R.rdi, f->R.rsi);
				break;
		// case SYS_REMOVE:
		// 		f->R.rax = remove(f->R.rdi);
		// 		break;
		case SYS_OPEN:
				f->R.rax = open(f->R.rdi);
				break;
		case SYS_FILESIZE:
				f->R.rax = filesize(f->R.rdi);
				break;
		case SYS_READ:
				f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
				break;
		case SYS_WRITE:
				f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
				break;
		// case SYS_SEEK:
		// 		seek(f->R.rdi, f->R.rsi);
		// 		break;
		// case SYS_TELL:
		// 		f->R.rax = tell(f->R.rdi);
		// 		break;
		// case SYS_CLOSE:
		// 		close(f->R.rdi);
		// 		break; 
		default:
			exit(-1);
			break;
		}
}

void halt(void) {
    power_off();
}
	
void exit(int status){
	thread_current()->exit = status;
	printf("%s: exit(%d)\n", thread_current()->name , status);
	thread_exit();
}

bool create(const char *file, unsigned initial_size) {
	checkadd(file);
	return filesys_create(file, initial_size);
}

int open (const char *file){
	checkadd(file);
	struct file * filefd = filesys_open(file);	
	// thread의 file descripter table에 추가
	// 이거 안 하면 read/write 안 됨(testcase보면 read안에서 open을 call 함)
	if (filefd == NULL){
		return -1;
	}
	int fd = to_fdt(filefd);

	if (fd == -1){
		file_close(filefd);
	}
	// rax에 file_open에서 나온 값을 추가
	return fd;
}

int filesize(int fd){
	struct file * file = to_fdt(fd);
	if (file == NULL) {
		return -1;
	}
	file_length(fd);
}

int read (int fd, void *buffer, unsigned size) {
	lock_acquire(&syslock);
	if(fd == 0){
		input_getc();
		lock_release(&syslock);
		return size;
	}
  	struct file *fileobj= fd_to_file(fd);
	size = file_read(fileobj,buffer,size);
	lock_release(&syslock);	
	return size;
}

int write (int fd, const void *buffer, unsigned size){
	lock_acquire(&syslock);
	if(fd == STDOUT_FILENO){
		 putbuf(buffer, size);
		lock_release(&syslock);
		return size;
	}
	struct file * file = fd_to_file(fd);
	if(file == NULL){
		lock_release(&syslock);
		return -1;
	}
	
	size = file_write(file,buffer,size);
	lock_release(&syslock);
	return size;
}

void close (int fd) {
	/* 해당 파일 디스크립터에 해당하는 파일을 닫음 */
	struct thread * t = thread_current();
	t->fdt[fd] = 0; /* 파일 디스크립터 엔트리 초기화 */
}