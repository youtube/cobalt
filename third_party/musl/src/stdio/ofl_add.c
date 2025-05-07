#include "stdio_impl.h"

FILE *__ofl_add(FILE *f)
{
// TODO: Investigate whether we need this and
// why is it crashing.
#if !defined(STARBOARD)
	FILE **head = __ofl_lock();
	f->next = *head;
	if (*head) (*head)->prev = f;
	*head = f;
	__ofl_unlock();
#endif
	return f;
}
