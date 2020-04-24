#include <stdlib.h>
#include <stdint.h>

#if defined(STARBOARD)
#include "starboard/system.h"
#else   // !defined(STARBOARD)
static uint64_t seed;
#endif  // defined(STARBOARD)

void srand(unsigned s)
{
#if !defined(STARBOARD)
	seed = s-1;
#endif  // !defined(STARBOARD)
}

int rand(void)
{
#if defined(STARBOARD)
	return SbSystemGetRandomUInt64() % RAND_MAX;
#else   // !defined(STARBOARD)
	seed = 6364136223846793005ULL*seed + 1;
	return seed>>33;
#endif  // defined(STARBOARD)
}
