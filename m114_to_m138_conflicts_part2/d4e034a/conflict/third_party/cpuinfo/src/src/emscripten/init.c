<<<<<<< HEAD
#include <math.h>
=======
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
<<<<<<< HEAD
=======
#include <math.h>
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))

#include <emscripten/threading.h>

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>

<<<<<<< HEAD
static const volatile float infinity = INFINITY;

static struct cpuinfo_package static_package = {};
=======

static const volatile float infinity = INFINITY;

static struct cpuinfo_package static_package = { };
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))

static struct cpuinfo_cache static_x86_l3 = {
	.size = 2 * 1024 * 1024,
	.associativity = 16,
	.sets = 2048,
	.partitions = 1,
	.line_size = 64,
};

void cpuinfo_emscripten_init(void) {
	struct cpuinfo_processor* processors = NULL;
	struct cpuinfo_core* cores = NULL;
	struct cpuinfo_cluster* clusters = NULL;
	struct cpuinfo_cache* l1i = NULL;
	struct cpuinfo_cache* l1d = NULL;
	struct cpuinfo_cache* l2 = NULL;

	const bool is_x86 = signbit(infinity - infinity);

	int logical_cores_count = emscripten_num_logical_cores();
	if (logical_cores_count <= 0) {
		logical_cores_count = 1;
	}
<<<<<<< HEAD
	uint32_t processor_count = (uint32_t)logical_cores_count;
=======
	uint32_t processor_count = (uint32_t) logical_cores_count;
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	uint32_t core_count = processor_count;
	uint32_t cluster_count = 1;
	uint32_t big_cluster_core_count = core_count;
	uint32_t processors_per_core = 1;
	if (is_x86) {
		if (processor_count % 2 == 0) {
			processors_per_core = 2;
			core_count = processor_count / 2;
			big_cluster_core_count = core_count;
		}
	} else {
		/* Assume ARM/ARM64 */
		if (processor_count > 4) {
			/* Assume big.LITTLE architecture */
			cluster_count = 2;
			big_cluster_core_count = processor_count >= 8 ? 4 : 2;
		}
	}
	uint32_t l2_count = is_x86 ? core_count : cluster_count;

	processors = calloc(processor_count, sizeof(struct cpuinfo_processor));
	if (processors == NULL) {
<<<<<<< HEAD
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " logical processors",
			processor_count * sizeof(struct cpuinfo_processor),
			processor_count);
=======
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" logical processors",
			processor_count * sizeof(struct cpuinfo_processor), processor_count);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		goto cleanup;
	}
	cores = calloc(processor_count, sizeof(struct cpuinfo_core));
	if (cores == NULL) {
<<<<<<< HEAD
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " cores",
			processor_count * sizeof(struct cpuinfo_core),
			processor_count);
=======
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" cores",
			processor_count * sizeof(struct cpuinfo_core), processor_count);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		goto cleanup;
	}
	clusters = calloc(cluster_count, sizeof(struct cpuinfo_cluster));
	if (clusters == NULL) {
<<<<<<< HEAD
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " clusters",
			cluster_count * sizeof(struct cpuinfo_cluster),
			cluster_count);
=======
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" clusters",
			cluster_count * sizeof(struct cpuinfo_cluster), cluster_count);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		goto cleanup;
	}

	l1i = calloc(core_count, sizeof(struct cpuinfo_cache));
	if (l1i == NULL) {
<<<<<<< HEAD
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1I caches",
			core_count * sizeof(struct cpuinfo_cache),
			core_count);
=======
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" L1I caches",
			core_count * sizeof(struct cpuinfo_cache), core_count);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		goto cleanup;
	}

	l1d = calloc(core_count, sizeof(struct cpuinfo_cache));
	if (l1d == NULL) {
<<<<<<< HEAD
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1D caches",
			core_count * sizeof(struct cpuinfo_cache),
			core_count);
=======
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" L1D caches",
			core_count * sizeof(struct cpuinfo_cache), core_count);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		goto cleanup;
	}

	l2 = calloc(l2_count, sizeof(struct cpuinfo_cache));
	if (l2 == NULL) {
<<<<<<< HEAD
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " L2 caches",
			l2_count * sizeof(struct cpuinfo_cache),
			l2_count);
=======
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" L2 caches",
			l2_count * sizeof(struct cpuinfo_cache), l2_count);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		goto cleanup;
	}

	static_package.processor_count = processor_count;
	static_package.core_count = core_count;
	static_package.cluster_count = cluster_count;
	if (is_x86) {
		strncpy(static_package.name, "x86 vCPU", CPUINFO_PACKAGE_NAME_MAX);
	} else {
		strncpy(static_package.name, "ARM vCPU", CPUINFO_PACKAGE_NAME_MAX);
	}

	for (uint32_t i = 0; i < core_count; i++) {
		for (uint32_t j = 0; j < processors_per_core; j++) {
<<<<<<< HEAD
			processors[i * processors_per_core + j] = (struct cpuinfo_processor){
				.smt_id = j,
				.core = cores + i,
				.cluster = clusters + (uint32_t)(i >= big_cluster_core_count),
				.package = &static_package,
				.cache.l1i = l1i + i,
				.cache.l1d = l1d + i,
				.cache.l2 = is_x86 ? l2 + i : l2 + (uint32_t)(i >= big_cluster_core_count),
=======
			processors[i * processors_per_core + j] = (struct cpuinfo_processor) {
				.smt_id = j,
				.core = cores + i,
				.cluster = clusters + (uint32_t) (i >= big_cluster_core_count),
				.package = &static_package,
				.cache.l1i = l1i + i,
				.cache.l1d = l1d + i,
				.cache.l2 = is_x86 ? l2 + i : l2 + (uint32_t) (i >= big_cluster_core_count),
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
				.cache.l3 = is_x86 ? &static_x86_l3 : NULL,
			};
		}

<<<<<<< HEAD
		cores[i] = (struct cpuinfo_core){
			.processor_start = i * processors_per_core,
			.processor_count = processors_per_core,
			.core_id = i,
			.cluster = clusters + (uint32_t)(i >= big_cluster_core_count),
=======
		cores[i] = (struct cpuinfo_core) {
			.processor_start = i * processors_per_core,
			.processor_count = processors_per_core,
			.core_id = i,
			.cluster = clusters + (uint32_t) (i >= big_cluster_core_count),
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			.package = &static_package,
			.vendor = cpuinfo_vendor_unknown,
			.uarch = cpuinfo_uarch_unknown,
			.frequency = 0,
		};

<<<<<<< HEAD
		l1i[i] = (struct cpuinfo_cache){
=======
		l1i[i] = (struct cpuinfo_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			.size = 32 * 1024,
			.associativity = 4,
			.sets = 128,
			.partitions = 1,
			.line_size = 64,
			.processor_start = i * processors_per_core,
			.processor_count = processors_per_core,
		};

<<<<<<< HEAD
		l1d[i] = (struct cpuinfo_cache){
=======
		l1d[i] = (struct cpuinfo_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			.size = 32 * 1024,
			.associativity = 4,
			.sets = 128,
			.partitions = 1,
			.line_size = 64,
			.processor_start = i * processors_per_core,
			.processor_count = processors_per_core,
		};

		if (is_x86) {
<<<<<<< HEAD
			l2[i] = (struct cpuinfo_cache){
=======
			l2[i] = (struct cpuinfo_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
				.size = 256 * 1024,
				.associativity = 8,
				.sets = 512,
				.partitions = 1,
				.line_size = 64,
				.processor_start = i * processors_per_core,
				.processor_count = processors_per_core,
			};
		}
	}

	if (is_x86) {
<<<<<<< HEAD
		clusters[0] = (struct cpuinfo_cluster){
=======
		clusters[0] = (struct cpuinfo_cluster) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			.processor_start = 0,
			.processor_count = processor_count,
			.core_start = 0,
			.core_count = core_count,
			.cluster_id = 0,
			.package = &static_package,
			.vendor = cpuinfo_vendor_unknown,
			.uarch = cpuinfo_uarch_unknown,
			.frequency = 0,
		};

		static_x86_l3.processor_count = processor_count;
	} else {
<<<<<<< HEAD
		clusters[0] = (struct cpuinfo_cluster){
=======
		clusters[0] = (struct cpuinfo_cluster) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			.processor_start = 0,
			.processor_count = big_cluster_core_count,
			.core_start = 0,
			.core_count = big_cluster_core_count,
			.cluster_id = 0,
			.package = &static_package,
			.vendor = cpuinfo_vendor_unknown,
			.uarch = cpuinfo_uarch_unknown,
			.frequency = 0,
		};

<<<<<<< HEAD
		l2[0] = (struct cpuinfo_cache){
=======
		l2[0] = (struct cpuinfo_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			.size = 1024 * 1024,
			.associativity = 8,
			.sets = 2048,
			.partitions = 1,
			.line_size = 64,
			.processor_start = 0,
			.processor_count = big_cluster_core_count,
		};

		if (cluster_count > 1) {
<<<<<<< HEAD
			l2[1] = (struct cpuinfo_cache){
=======
			l2[1] = (struct cpuinfo_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
				.size = 256 * 1024,
				.associativity = 8,
				.sets = 512,
				.partitions = 1,
				.line_size = 64,
				.processor_start = big_cluster_core_count,
				.processor_count = processor_count - big_cluster_core_count,
			};

<<<<<<< HEAD
			clusters[1] = (struct cpuinfo_cluster){
=======
			clusters[1] = (struct cpuinfo_cluster) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
				.processor_start = big_cluster_core_count,
				.processor_count = processor_count - big_cluster_core_count,
				.core_start = big_cluster_core_count,
				.core_count = processor_count - big_cluster_core_count,
				.cluster_id = 1,
				.package = &static_package,
				.vendor = cpuinfo_vendor_unknown,
				.uarch = cpuinfo_uarch_unknown,
				.frequency = 0,
			};
		}
	}

	/* Commit changes */
	cpuinfo_cache[cpuinfo_cache_level_1i] = l1i;
	cpuinfo_cache[cpuinfo_cache_level_1d] = l1d;
<<<<<<< HEAD
	cpuinfo_cache[cpuinfo_cache_level_2] = l2;
	if (is_x86) {
		cpuinfo_cache[cpuinfo_cache_level_3] = &static_x86_l3;
=======
	cpuinfo_cache[cpuinfo_cache_level_2]  = l2;
	if (is_x86) {
		cpuinfo_cache[cpuinfo_cache_level_3]  = &static_x86_l3;
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	}

	cpuinfo_processors = processors;
	cpuinfo_cores = cores;
	cpuinfo_clusters = clusters;
	cpuinfo_packages = &static_package;

	cpuinfo_cache_count[cpuinfo_cache_level_1i] = processor_count;
	cpuinfo_cache_count[cpuinfo_cache_level_1d] = processor_count;
<<<<<<< HEAD
	cpuinfo_cache_count[cpuinfo_cache_level_2] = l2_count;
	if (is_x86) {
		cpuinfo_cache_count[cpuinfo_cache_level_3] = 1;
	}

	cpuinfo_global_uarch = (struct cpuinfo_uarch_info){
=======
	cpuinfo_cache_count[cpuinfo_cache_level_2]  = l2_count;
	if (is_x86) {
		cpuinfo_cache_count[cpuinfo_cache_level_3]  = 1;
	}

	cpuinfo_global_uarch = (struct cpuinfo_uarch_info) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		.uarch = cpuinfo_uarch_unknown,
		.processor_count = processor_count,
		.core_count = core_count,
	};

	cpuinfo_processors_count = processor_count;
	cpuinfo_cores_count = processor_count;
	cpuinfo_clusters_count = cluster_count;
	cpuinfo_packages_count = 1;

	cpuinfo_max_cache_size = is_x86 ? 128 * 1024 * 1024 : 8 * 1024 * 1024;

	cpuinfo_is_initialized = true;

	processors = NULL;
	cores = NULL;
	clusters = NULL;
	l1i = l1d = l2 = NULL;

cleanup:
	free(processors);
	free(cores);
	free(clusters);
	free(l1i);
	free(l1d);
	free(l2);
}
