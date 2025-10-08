#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

#include <cpuinfo.h>
#include <x86/api.h>

struct cpuinfo_arm_linux_processor {
	/**
<<<<<<< HEAD
	 * Minimum processor ID on the package which includes this logical
	 * processor. This value can serve as an ID for the cluster of logical
	 * processors: it is the same for all logical processors on the same
	 * package.
	 */
	uint32_t package_leader_id;
	/**
	 * Minimum processor ID on the core which includes this logical
	 * processor. This value can serve as an ID for the cluster of logical
	 * processors: it is the same for all logical processors on the same
	 * package.
=======
	 * Minimum processor ID on the package which includes this logical processor.
	 * This value can serve as an ID for the cluster of logical processors: it is the
	 * same for all logical processors on the same package.
	 */
	uint32_t package_leader_id;
	/**
	 * Minimum processor ID on the core which includes this logical processor.
	 * This value can serve as an ID for the cluster of logical processors: it is the
	 * same for all logical processors on the same package.
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	 */
	/**
	 * Number of logical processors in the package.
	 */
	uint32_t package_processor_count;
	/**
	 * Maximum frequency, in kHZ.
<<<<<<< HEAD
	 * The value is parsed from
	 * /sys/devices/system/cpu/cpu<N>/cpufreq/cpuinfo_max_freq If failed to
	 * read or parse the file, the value is 0.
=======
	 * The value is parsed from /sys/devices/system/cpu/cpu<N>/cpufreq/cpuinfo_max_freq
	 * If failed to read or parse the file, the value is 0.
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	 */
	uint32_t max_frequency;
	/**
	 * Minimum frequency, in kHZ.
<<<<<<< HEAD
	 * The value is parsed from
	 * /sys/devices/system/cpu/cpu<N>/cpufreq/cpuinfo_min_freq If failed to
	 * read or parse the file, the value is 0.
=======
	 * The value is parsed from /sys/devices/system/cpu/cpu<N>/cpufreq/cpuinfo_min_freq
	 * If failed to read or parse the file, the value is 0.
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	 */
	uint32_t min_frequency;
	/** Linux processor ID */
	uint32_t system_processor_id;
	uint32_t flags;
};
