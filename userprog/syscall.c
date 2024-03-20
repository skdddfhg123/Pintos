#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"


#include "lib/user/syscall.h"

void		syscall_entry (void);
void		syscall_handler (struct intr_frame *);

void		user_address_check (uint64_t *addr) ;

void		halt (void) ;
void		exit (int status) ;
int			wait (pid_t pid);

pid_t		fork (const char *thread_name);
int			exec (const char *cmd_line);

bool		create (const char *file, unsigned initial_size);
bool		remove (const char *file);
int			open (const char *file);
int			filesize (int fd);
void		close (int fd);
int			read (int fd, void *buffer, unsigned size);
int			write (int fd, const void *buffer, unsigned size);
void		seek (int fd, unsigned position);
unsigned	tell (int fd);

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

static struct lock read_lock;
static struct lock write_lock;

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
	lock_init(&read_lock);
	lock_init(&write_lock);
}



/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.
	// printf("###\n");
	switch (f->R.rax) {
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit (f->R.rdi);
			break;
		case SYS_FORK:
			user_address_check(f->R.rdi);
			memcpy(&thread_current()->intr_f, f, sizeof(struct intr_frame));
			f->R.rax = fork (f->R.rdi);
			break;
		case SYS_EXEC:
			user_address_check(f->R.rdi);
			f->R.rax = exec (f->R.rdi);
			break;
		case SYS_WAIT:
			f->R.rax = wait (f->R.rdi);
			break;
		case SYS_CREATE:
			user_address_check(f->R.rdi);
			f->R.rax =  create (f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			user_address_check(f->R.rdi);
			f->R.rax = remove (f->R.rdi);
			break;
		case SYS_OPEN:
			user_address_check(f->R.rdi);
			f->R.rax = open (f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			user_address_check(f->R.rsi);
			f->R.rax = read (f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			user_address_check(f->R.rsi);
			f->R.rax = write (f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek (f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell (f->R.rdi);
			break;
		case SYS_CLOSE:
			close (f->R.rdi);
			break;
		case SYS_MMAP:
			user_address_check(f->R.rdi);
				// mmap (f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
			break;
		case SYS_MUNMAP:
			user_address_check(f->R.rdi);
				// munmap (f->R.rdi);
			break;
		case SYS_CHDIR:
			user_address_check(f->R.rdi);
				// chdir (f->R.rdi);
			break;
		case SYS_MKDIR:
			user_address_check(f->R.rdi);
				// mkdir (f->R.rdi);
			break;
		case SYS_READDIR:
			break;
		case SYS_ISDIR:
			break;
		case SYS_INUMBER:
			break;
		case SYS_SYMLINK:
			break;
		case SYS_DUP2:
			// dup2 (f->R.rdi, f->R.rsi);
			break;
		case SYS_MOUNT:
			break;
		case SYS_UMOUNT:
			break;
	}
}


void user_address_check (uint64_t *addr) {
	if (is_kernel_vaddr(addr) || pml4_get_page(thread_current()->pml4, addr) == NULL) 
		exit(-1);
}

void halt (void)  {
	power_off ();
}

void exit (int status)  {
	struct thread *cur = thread_current (); 

	cur->exit_status = status;
	printf("%s: exit(%d)\n" , cur -> name , status);
	
	thread_exit();
}

bool create (const char *file, unsigned initial_size) {
	bool ret = filesys_create(file, initial_size) != NULL ? true : false;
	return ret;
}

bool remove (const char *file) {
	return filesys_remove(file) != NULL ? true : false;
}

int filesize (int fd) {
	struct thread	*cur = thread_current();
	struct file		*cur_file = cur->fd_t[fd];

	if (fd < 0 || fd >= 64 || cur_file == NULL)
		return -1;

	return file_length(cur_file);
}

int open (const char *file) {
	struct thread	*cur = thread_current();
	cur->fd_t[cur->fd] = filesys_open(file);
	if (cur->fd_t[cur->fd] != NULL) {
		// file_deny_write(cur->fd_t[cur->fd]);
		return cur->fd++;
	}
	else
		return -1;
}

void close (int fd) {
	struct thread	*cur = thread_current();

	if (fd < 0 || fd >= 64 || cur->fd_t[fd] == NULL)
		return ;
	file_close(cur->fd_t[fd]);
	cur->fd_t[fd] = NULL;
}

int read (int fd, void *buffer, unsigned size) {
	struct thread	*cur = thread_current();
	int ret;

	if (fd < 0 || fd >= 64 || fd == 1 || fd == 2)
		return -1;
	struct file		*cur_file = cur->fd_t[fd];
	if (cur_file == NULL)
		return -1;
	if (fd == 0)
		return input_getc();

	lock_acquire(&read_lock);
	ret = file_read(cur_file, buffer, size);
	lock_release(&read_lock);
	return ret;
}

int write (int fd, const void *buffer, unsigned size) {
	struct thread	*cur = thread_current();
	int ret;

	if (fd == 1)
		putbuf(buffer, size);
	if (fd < 0 || fd >= 64 || fd == 0 || fd == 2)
		return -1;
	struct file		*cur_file = cur->fd_t[fd];
	if (cur_file == NULL)
		return -1;
	else {
		lock_acquire(&write_lock);
		ret = file_write(cur_file, buffer, size);
		lock_release(&write_lock);
		return ret;
	}
}

void	seek (int fd, unsigned position) {
	struct thread	*cur = thread_current();

	if (fd < 0 || fd >= 64 || cur->fd_t[fd] == NULL)
		return ;
	// off_t			new_pos = cur->fd_t[fd]->pos + position;
	file_seek(cur->fd_t[fd], position);
}

unsigned	tell (int fd) {
	struct thread	*cur = thread_current();

	if (fd < 0 || fd >= 64 || cur->fd_t[fd] == NULL)
		return ;
	return file_tell(cur->fd_t[fd]);
}

pid_t	fork (const char *thread_name) {
	return process_fork(thread_name, thread_current()->intr_f);
}

int		wait (pid_t pid) {
	return process_wait (pid);
}

int	exec (const char *cmd_line) {
	char *cmd_cp = palloc_get_page (PAL_ZERO);
	if (cmd_cp == NULL)
		return TID_ERROR;
	strlcpy (cmd_cp, cmd_line, PGSIZE);
	if (process_exec (cmd_cp) < 0) {
		exit(-1);
		return -1;
	}
	NOT_REACHED ();
}
