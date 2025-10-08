<<<<<<< HEAD
#include <alloca.h>
#include <errno.h>
#include <string.h>

#include <sys/sysctl.h>
#include <sys/types.h>
=======
#include <string.h>
#include <alloca.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/sysctl.h>
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))

#include <cpuinfo/log.h>
#include <mach/api.h>

#include <TargetConditionals.h>

<<<<<<< HEAD
=======

>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
struct cpuinfo_mach_topology cpuinfo_mach_detect_topology(void) {
	int cores = 1;
	size_t sizeof_cores = sizeof(cores);
	if (sysctlbyname("hw.physicalcpu_max", &cores, &sizeof_cores, NULL, 0) != 0) {
		cpuinfo_log_error("sysctlbyname(\"hw.physicalcpu_max\") failed: %s", strerror(errno));
	} else if (cores <= 0) {
		cpuinfo_log_error("sysctlbyname(\"hw.physicalcpu_max\") returned invalid value %d", cores);
		cores = 1;
	}

	int threads = 1;
	size_t sizeof_threads = sizeof(threads);
	if (sysctlbyname("hw.logicalcpu_max", &threads, &sizeof_threads, NULL, 0) != 0) {
		cpuinfo_log_error("sysctlbyname(\"hw.logicalcpu_max\") failed: %s", strerror(errno));
	} else if (threads <= 0) {
		cpuinfo_log_error("sysctlbyname(\"hw.logicalcpu_max\") returned invalid value %d", threads);
		threads = cores;
	}

	int packages = 1;
#if !TARGET_OS_IPHONE
	size_t sizeof_packages = sizeof(packages);
	if (sysctlbyname("hw.packages", &packages, &sizeof_packages, NULL, 0) != 0) {
		cpuinfo_log_error("sysctlbyname(\"hw.packages\") failed: %s", strerror(errno));
	} else if (packages <= 0) {
		cpuinfo_log_error("sysctlbyname(\"hw.packages\") returned invalid value %d", packages);
		packages = 1;
	}
#endif

<<<<<<< HEAD
	cpuinfo_log_debug("mach topology: packages = %d, cores = %d, threads = %d", packages, (int)cores, (int)threads);
	struct cpuinfo_mach_topology topology = {
		.packages = (uint32_t)packages, .cores = (uint32_t)cores, .threads = (uint32_t)threads};
=======
	cpuinfo_log_debug("mach topology: packages = %d, cores = %d, threads = %d", packages, (int) cores, (int) threads);
	struct cpuinfo_mach_topology topology = {
		.packages = (uint32_t) packages,
		.cores = (uint32_t) cores,
		.threads = (uint32_t) threads
	};
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))

#if !TARGET_OS_IPHONE
	size_t cacheconfig_size = 0;
	if (sysctlbyname("hw.cacheconfig", NULL, &cacheconfig_size, NULL, 0) != 0) {
		cpuinfo_log_error("sysctlbyname(\"hw.cacheconfig\") failed: %s", strerror(errno));
	} else {
		uint64_t* cacheconfig = alloca(cacheconfig_size);
		if (sysctlbyname("hw.cacheconfig", cacheconfig, &cacheconfig_size, NULL, 0) != 0) {
			cpuinfo_log_error("sysctlbyname(\"hw.cacheconfig\") failed: %s", strerror(errno));
		} else {
			size_t cache_configs = cacheconfig_size / sizeof(uint64_t);
			cpuinfo_log_debug("mach hw.cacheconfig count: %zu", cache_configs);
			if (cache_configs > CPUINFO_MACH_MAX_CACHE_LEVELS) {
				cache_configs = CPUINFO_MACH_MAX_CACHE_LEVELS;
			}
			for (size_t i = 0; i < cache_configs; i++) {
<<<<<<< HEAD
				cpuinfo_log_debug("mach hw.cacheconfig[%zu]: %" PRIu64, i, cacheconfig[i]);
=======
				cpuinfo_log_debug("mach hw.cacheconfig[%zu]: %"PRIu64, i, cacheconfig[i]);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
				topology.threads_per_cache[i] = cacheconfig[i];
			}
		}
	}
#endif
	return topology;
}
