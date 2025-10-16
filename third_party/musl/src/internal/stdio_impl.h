#ifndef _STDIO_IMPL_H
#define _STDIO_IMPL_H

#include <stdio.h>

#if defined(STARBOARD)
#include "pthread_impl.h"
#endif

#include "syscall.h"

#define UNGET 8

#define FFINALLOCK(f) ((f)->lock>=0 ? __lockfile((f)) : 0)
#define FLOCK(f) int __need_unlock = ((f)->lock>=0 ? __lockfile((f)) : 0)
#define FUNLOCK(f) do { if (__need_unlock) __unlockfile((f)); } while (0)


#define F_PERM 1
#define F_NORD 4
#define F_NOWR 8
#define F_EOF 16
#define F_ERR 32
#define F_SVB 64
#define F_APP 128

struct _IO_FILE {
	unsigned flags;
	unsigned char *rpos, *rend;
	int (*close)(FILE *);
	unsigned char *wend, *wpos;
	unsigned char *mustbezero_1;
	unsigned char *wbase;
	size_t (*read)(FILE *, unsigned char *, size_t);
	size_t (*write)(FILE *, const unsigned char *, size_t);
	off_t (*seek)(FILE *, off_t, int);
	unsigned char *buf;
	size_t buf_size;
	FILE *prev, *next;
	int fd;
	int pipe_pid;
	long lockcount;
	int mode;
#if defined(STARBOARD)
	// To implement futex behavior with pthreads we need additional data, which
	// is held in StarboardPthreadCondMutex. A union with an int is used here,
	// allowing existing code using the int member to remain unchanged.
	union {
		volatile int lock;
		StarboardPthreadCondMutex cond_mutex;
	};
#else
	volatile int lock;
#endif
	int lbf;
	void *cookie;
	off_t off;
	char *getln_buf;
	void *mustbezero_2;
	unsigned char *shend;
	off_t shlim, shcnt;
	FILE *prev_locked, *next_locked;
	struct __locale_struct *locale;
};

#if defined(STARBOARD)
static inline void __init_file_lock(struct _IO_FILE* f) {
	if (f->lock < 0) return;
	__cond_mutex_pair_init(&f->cond_mutex);
}

static void __destroy_file_lock(struct _IO_FILE* f) {
	if (f->lock < 0) return;
	__cond_mutex_pair_destroy(&f->cond_mutex);
}

// File read and write functions that initialize the mutex and cond variable
// before their use.
size_t __stdio_read_init(FILE *f, unsigned char *buf, size_t len);
size_t __stdio_write_init(FILE *f, const unsigned char *buf, size_t len);
off_t __stdio_seek_init(FILE *f, off_t off, int whence);

// Stubs for calls to read from stdout/stderr, and write to stdin.
size_t __stdio_read_stub(FILE *f, unsigned char *buf, size_t len);
size_t __stdio_write_stub(FILE *f, const unsigned char *buf, size_t len);
#endif  // defined(STARBOARD)

extern hidden FILE *volatile __stdin_used;
extern hidden FILE *volatile __stdout_used;
extern hidden FILE *volatile __stderr_used;

hidden int __lockfile(FILE *);
hidden void __unlockfile(FILE *);

hidden size_t __stdio_read(FILE *, unsigned char *, size_t);
hidden size_t __stdio_write(FILE *, const unsigned char *, size_t);
hidden size_t __stdout_write(FILE *, const unsigned char *, size_t);
hidden off_t __stdio_seek(FILE *, off_t, int);
hidden int __stdio_close(FILE *);

hidden int __toread(FILE *);
hidden int __towrite(FILE *);

hidden void __stdio_exit(void);
hidden void __stdio_exit_needed(void);

#if defined(__PIC__) && (100*__GNUC__+__GNUC_MINOR__ >= 303)
__attribute__((visibility("protected")))
#endif
int __overflow(FILE *, int), __uflow(FILE *);

hidden int __fseeko(FILE *, off_t, int);
hidden int __fseeko_unlocked(FILE *, off_t, int);
hidden off_t __ftello(FILE *);
hidden off_t __ftello_unlocked(FILE *);
hidden size_t __fwritex(const unsigned char *, size_t, FILE *);
hidden int __putc_unlocked(int, FILE *);

hidden FILE *__fdopen(int, const char *);
hidden int __fmodeflags(const char *);

hidden FILE *__ofl_add(FILE *f);
hidden FILE **__ofl_lock(void);
hidden void __ofl_unlock(void);

struct __pthread;
hidden void __register_locked_file(FILE *, struct __pthread *);
hidden void __unlist_locked_file(FILE *);
hidden void __do_orphaned_stdio_locks(void);

#define MAYBE_WAITERS 0x40000000

hidden void __getopt_msg(const char *, const char *, const char *, size_t);

#define feof(f) ((f)->flags & F_EOF)
#define ferror(f) ((f)->flags & F_ERR)

#define getc_unlocked(f) \
	( ((f)->rpos != (f)->rend) ? *(f)->rpos++ : __uflow((f)) )

#define putc_unlocked(c, f) \
	( (((unsigned char)(c)!=(f)->lbf && (f)->wpos!=(f)->wend)) \
	? *(f)->wpos++ = (unsigned char)(c) \
	: __overflow((f),(unsigned char)(c)) )

/* Caller-allocated FILE * operations */
hidden FILE *__fopen_rb_ca(const char *, FILE *, unsigned char *, size_t);
hidden int __fclose_ca(FILE *);

#endif
