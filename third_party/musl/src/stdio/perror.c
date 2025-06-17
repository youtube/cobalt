#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "build/build_config.h"
#if !BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
#include "stdio_impl.h"
#endif // !BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)

void perror(const char *msg)
{
#if BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
	// TODO: Explore ways to capture standard output and 
	// standard error for testing. (b/425692168).

	// Hack method to introduce the symbol into Chrobalt.
	// Once the iobuffer has been implemented into Chrobalt, this
	// hack method should be removed with a proper implementation.
	printf("%s: %s", msg, strerror(errno));
#else // BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
	FILE *f = stderr;
	char *errstr = strerror(errno);

	FLOCK(f);

	/* Save stderr's orientation and encoding rule, since perror is not
	 * permitted to change them. */
	void *old_locale = f->locale;
	int old_mode = f->mode;
	
	if (msg && *msg) {
		fwrite(msg, strlen(msg), 1, f);
		fputc(':', f);
		fputc(' ', f);
	}
	fwrite(errstr, strlen(errstr), 1, f);
	fputc('\n', f);

	f->mode = old_mode;
	f->locale = old_locale;

	FUNLOCK(f);
#endif // BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
}
