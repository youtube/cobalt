#include <float.h>
#include <math.h>
#include <stdint.h>
#include "libc.h"

#if FLT_EVAL_METHOD==0 || FLT_EVAL_METHOD==1
#define EPS DBL_EPSILON
#elif FLT_EVAL_METHOD==2
#define EPS LDBL_EPSILON
#endif
static const double_t toint = 1/EPS;

#ifdef COBALT_MUSL_W_GLIBC_HEADERS
double rint(double x);
double rint_internal(double x)
#else  // COBALT_MUSL_W_GLIBC_HEADERS
double rint(double x)
#endif  // COBALT_MUSL_W_GLIBC_HEADERS
{
	union {double f; uint64_t i;} u = {x};
	int e = u.i>>52 & 0x7ff;
	int s = u.i>>63;
	double_t y;

	if (e >= 0x3ff+52)
		return x;
	if (s)
		y = x - toint + toint;
	else
		y = x + toint - toint;
	if (y == 0)
		return s ? -0.0 : 0;
	return y;
}

#ifdef COBALT_MUSL_W_GLIBC_HEADERS
weak_alias(rint_internal, rint);
#endif  // COBALT_MUSL_W_GLIBC_HEADERS
