#pragma once

<<<<<<< HEAD
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <stdint.h>

=======
#include <stdint.h>


>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
inline static uint32_t bit_length(uint32_t n) {
	const uint32_t n_minus_1 = n - 1;
	if (n_minus_1 == 0) {
		return 0;
	} else {
<<<<<<< HEAD
#ifdef _MSC_VER
		unsigned long bsr;
		_BitScanReverse(&bsr, n_minus_1);
		return bsr + 1;
#else
		return 32 - __builtin_clz(n_minus_1);
#endif
=======
		#ifdef _MSC_VER
			unsigned long bsr;
			_BitScanReverse(&bsr, n_minus_1);
			return bsr + 1;
		#else
			return 32 - __builtin_clz(n_minus_1);
		#endif
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	}
}
