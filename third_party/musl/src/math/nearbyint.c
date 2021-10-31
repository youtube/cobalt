#include <fenv.h>
#include <math.h>

/* nearbyint is the same as rint, but it must not raise the inexact exception */

#ifdef COBALT_MUSL_W_GLIBC_HEADERS
double rint_internal(double x);
#endif  // COBALT_MUSL_W_GLIBC_HEADERS
double nearbyint(double x)
{
#ifdef FE_INEXACT
	#pragma STDC FENV_ACCESS ON
	int e;

	e = fetestexcept(FE_INEXACT);
#endif
#ifdef COBALT_MUSL_W_GLIBC_HEADERS
	x = rint_internal(x);
#else
	x = rint(x);
#endif  // COBALT_MUSL_W_GLIBC_HEADERS
#ifdef FE_INEXACT
	if (!e)
		feclearexcept(FE_INEXACT);
#endif
	return x;
}
