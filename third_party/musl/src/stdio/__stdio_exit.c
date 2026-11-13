#include "stdio_impl.h"

static FILE *volatile dummy_file = 0;
weak_alias(dummy_file, __stdin_used);
weak_alias(dummy_file, __stdout_used);
weak_alias(dummy_file, __stderr_used);

static void close_file(FILE *f)
{
	if (!f) return;
#if defined(STARBOARD)
	// Unlike upstream musl (which immediately terminates the process via
	// _Exit), Starboard continues running after __stdio_exit(), so locks
	// and buffer state must be released to avoid deadlocking later stdio
	// calls.
	FLOCK(f);
#else
	FFINALLOCK(f);
#endif
	if (f->wpos != f->wbase) f->write(f, 0, 0);
	if (f->rpos != f->rend) f->seek(f, f->rpos-f->rend, SEEK_CUR);
#if defined(STARBOARD)
	f->wpos = f->wbase = f->wend = 0;
	f->rpos = f->rend = 0;
	FUNLOCK(f);
#endif
}

void __stdio_exit(void)
{
	FILE *f;
	for (f=*__ofl_lock(); f; f=f->next) close_file(f);
#if defined(STARBOARD)
	__ofl_unlock();
#endif
	close_file(__stdin_used);
	close_file(__stdout_used);
	close_file(__stderr_used);
}

weak_alias(__stdio_exit, __stdio_exit_needed);
