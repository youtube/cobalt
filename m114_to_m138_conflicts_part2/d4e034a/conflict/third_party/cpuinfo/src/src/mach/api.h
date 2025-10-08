#pragma once

#include <stdint.h>

#define CPUINFO_MACH_MAX_CACHE_LEVELS 8

<<<<<<< HEAD
=======

>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
struct cpuinfo_mach_topology {
	uint32_t packages;
	uint32_t cores;
	uint32_t threads;
	uint32_t threads_per_cache[CPUINFO_MACH_MAX_CACHE_LEVELS];
};

<<<<<<< HEAD
=======

>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
struct cpuinfo_mach_topology cpuinfo_mach_detect_topology(void);
