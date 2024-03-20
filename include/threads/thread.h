#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

#define FDT_PAGES 3
#define FDT_COUNT_LIMIT FDT_PAGES *(1<<9)

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */

	int priority;            /* Priority. */

	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
	int64_t wakeup_tick; 			  /* Wake up tick */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */

	int last_priority;
	struct list donations;
	struct list_elem d_elem;
	struct lock * wait_on_lock;

	int nice;
	int recent_cpu;

	int exit;
	struct file ** fdt;
	int file_index;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* 추가한 것들 - running > blocked */
void thread_sleep(int64_t ticks);
/* 추가한 것들 - blocked > ready */
void thread_wakeup(int64_t ticks);
/* 추가한 것들 - tick의 최소값을 저장 */
void minimum_set(int64_t ticks);
/* 추가한 것들 - 저장해 놓은 tick의 최소값을 반환 */
int64_t minimum_get(void);
/* 추가한 것들 - compare를 통해 list안에 element 중 a가 b보다 더 크면 true, 아니면 false */
bool cmp_priority(struct list_elem *a , struct list_elem *b);
/* 추가한 것들 - thread 생성할 때 ready list에 있는 priority를 running하고 있는 priority랑 비교해서 높으면 yield하는 함수*/
void thread_priority(void);

bool sma_priority(struct list_elem *a , struct list_elem *b);
void lock_compare(struct list_elem *a , struct list_elem *b);

/* 평균 */
float load_avg;
/* 대기리스트 카운트 */
int ready_threads(void);

void
donate_priority (void);
void
refresh_priority (void);
void
remove_with_lock (struct lock *lock);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

// advanced //
// void
// mlfqs_priority(struct thread * t);

// void
// mlfqs_recent_cpu (struct thread *t);

// #define F (1 << 14)
// #define INT_MAX ((1 << 31) - 1)
// #define INT_MIN (-(1 << 31))

// int int_to_fp (int n) {
//   return n * F;
// }

// int fp_to_int (int x) {
//   return x / F;
// }

// int fp_to_int_round (int x) {
//   if (x >= 0) return (x + F / 2) / F;
//   else return (x - F / 2) / F;
// }

// int add_fp (int x, int y) {
//   return x + y;
// }

// int sub_fp (int x, int y) {
//   return x - y;
// }

// int add_mixed (int x, int n) {
//   return x + n * F;
// }

// int sub_mixed (int x, int n) {
//   return x - n * F;
// }

// int mult_fp (int x, int y) {
//   return ((int64_t) x) * y / F;
// }

// int mult_mixed (int x, int n) {
//   return x * n;
// }

// int div_fp (int x, int y) {
//   return ((int64_t) x) * F / y;
// }

// int div_mixed (int x, int n) {
//   return x / n;
// }

#endif /* threads/thread.h */
