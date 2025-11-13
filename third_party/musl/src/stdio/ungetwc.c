#include "stdio_impl.h"
#include "locale_impl.h"
#include <wchar.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>

wint_t ungetwc(wint_t c, FILE *f)
{
	unsigned char mbc[MB_LEN_MAX];
	int l;
#if !defined(STARBOARD)
	// Skip locale set and restore for the file, since that is only necessary 
	// after calls to fwide() which is not included with Starboard.
	locale_t *ploc = &CURRENT_LOCALE, loc = *ploc;
#endif

	FLOCK(f);

#if !defined(STARBOARD)
	if (f->mode <= 0) fwide(f, 1);
	*ploc = f->locale;
#endif

	if (!f->rpos) __toread(f);
	if (!f->rpos || c == WEOF || (l = wcrtomb((void *)mbc, c, 0)) < 0 ||
	    f->rpos < f->buf - UNGET + l) {
		FUNLOCK(f);
#if !defined(STARBOARD)
		*ploc = loc;
#endif		
		return WEOF;
	}

	if (isascii(c)) *--f->rpos = c;
	else memcpy(f->rpos -= l, mbc, l);

	f->flags &= ~F_EOF;

	FUNLOCK(f);
#if !defined(STARBOARD)
	*ploc = loc;
#endif
	return c;
}
