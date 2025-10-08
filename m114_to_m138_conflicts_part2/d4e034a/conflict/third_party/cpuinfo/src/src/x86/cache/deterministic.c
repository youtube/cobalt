#include <stdint.h>

#include <cpuinfo.h>
<<<<<<< HEAD
#include <cpuinfo/log.h>
#include <cpuinfo/utils.h>
#include <x86/cpuid.h>
=======
#include <x86/cpuid.h>
#include <cpuinfo/utils.h>
#include <cpuinfo/log.h>

>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))

enum cache_type {
	cache_type_none = 0,
	cache_type_data = 1,
	cache_type_instruction = 2,
	cache_type_unified = 3,
};

bool cpuinfo_x86_decode_deterministic_cache_parameters(
	struct cpuid_regs regs,
	struct cpuinfo_x86_caches* cache,
<<<<<<< HEAD
	uint32_t* package_cores_max) {
=======
	uint32_t* package_cores_max)
{
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	const uint32_t type = regs.eax & UINT32_C(0x1F);
	if (type == cache_type_none) {
		return false;
	}

	/* Level starts at 1 */
	const uint32_t level = (regs.eax >> 5) & UINT32_C(0x7);

	const uint32_t sets = 1 + regs.ecx;
	const uint32_t line_size = 1 + (regs.ebx & UINT32_C(0x00000FFF));
	const uint32_t partitions = 1 + ((regs.ebx >> 12) & UINT32_C(0x000003FF));
	const uint32_t associativity = 1 + (regs.ebx >> 22);

	*package_cores_max = 1 + (regs.eax >> 26);
	const uint32_t processors = 1 + ((regs.eax >> 14) & UINT32_C(0x00000FFF));
	const uint32_t apic_bits = bit_length(processors);

	uint32_t flags = 0;
	if (regs.edx & UINT32_C(0x00000002)) {
		flags |= CPUINFO_CACHE_INCLUSIVE;
	}
	if (regs.edx & UINT32_C(0x00000004)) {
		flags |= CPUINFO_CACHE_COMPLEX_INDEXING;
	}
	switch (level) {
		case 1:
			switch (type) {
				case cache_type_unified:
<<<<<<< HEAD
					cache->l1d = cache->l1i = (struct cpuinfo_x86_cache){
=======
					cache->l1d = cache->l1i = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags | CPUINFO_CACHE_UNIFIED,
<<<<<<< HEAD
						.apic_bits = apic_bits};
					break;
				case cache_type_data:
					cache->l1d = (struct cpuinfo_x86_cache){
=======
						.apic_bits = apic_bits
					};
					break;
				case cache_type_data:
					cache->l1d = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
<<<<<<< HEAD
						.apic_bits = apic_bits};
					break;
				case cache_type_instruction:
					cache->l1i = (struct cpuinfo_x86_cache){
=======
						.apic_bits = apic_bits
					};
					break;
				case cache_type_instruction:
					cache->l1i = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
<<<<<<< HEAD
						.apic_bits = apic_bits};
=======
						.apic_bits = apic_bits
					};
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
			}
			break;
		case 2:
			switch (type) {
				case cache_type_instruction:
<<<<<<< HEAD
					cpuinfo_log_warning(
						"unexpected L2 instruction cache reported in leaf 0x00000004 is ignored");
=======
					cpuinfo_log_warning("unexpected L2 instruction cache reported in leaf 0x00000004 is ignored");
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
<<<<<<< HEAD
					cache->l2 = (struct cpuinfo_x86_cache){
=======
					cache->l2 = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
<<<<<<< HEAD
						.apic_bits = apic_bits};
=======
						.apic_bits = apic_bits
					};
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
			}
			break;
		case 3:
			switch (type) {
				case cache_type_instruction:
<<<<<<< HEAD
					cpuinfo_log_warning(
						"unexpected L3 instruction cache reported in leaf 0x00000004 is ignored");
=======
					cpuinfo_log_warning("unexpected L3 instruction cache reported in leaf 0x00000004 is ignored");
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
<<<<<<< HEAD
					cache->l3 = (struct cpuinfo_x86_cache){
=======
					cache->l3 = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
<<<<<<< HEAD
						.apic_bits = apic_bits};
=======
						.apic_bits = apic_bits
					};
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
			}
			break;
		case 4:
			switch (type) {
				case cache_type_instruction:
<<<<<<< HEAD
					cpuinfo_log_warning(
						"unexpected L4 instruction cache reported in leaf 0x00000004 is ignored");
=======
					cpuinfo_log_warning("unexpected L4 instruction cache reported in leaf 0x00000004 is ignored");
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
<<<<<<< HEAD
					cache->l4 = (struct cpuinfo_x86_cache){
=======
					cache->l4 = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
<<<<<<< HEAD
						.apic_bits = apic_bits};
=======
						.apic_bits = apic_bits
					};
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
			}
			break;
		default:
<<<<<<< HEAD
			cpuinfo_log_warning(
				"unexpected L%" PRIu32 " cache reported in leaf 0x00000004 is ignored", level);
=======
			cpuinfo_log_warning("unexpected L%"PRIu32" cache reported in leaf 0x00000004 is ignored", level);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			break;
	}
	return true;
}

<<<<<<< HEAD
bool cpuinfo_x86_decode_cache_properties(struct cpuid_regs regs, struct cpuinfo_x86_caches* cache) {
=======

bool cpuinfo_x86_decode_cache_properties(
	struct cpuid_regs regs,
	struct cpuinfo_x86_caches* cache)
{
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	const uint32_t type = regs.eax & UINT32_C(0x1F);
	if (type == cache_type_none) {
		return false;
	}

	const uint32_t level = (regs.eax >> 5) & UINT32_C(0x7);
	const uint32_t cores = 1 + ((regs.eax >> 14) & UINT32_C(0x00000FFF));
	const uint32_t apic_bits = bit_length(cores);

	const uint32_t sets = 1 + regs.ecx;
	const uint32_t line_size = 1 + (regs.ebx & UINT32_C(0x00000FFF));
	const uint32_t partitions = 1 + ((regs.ebx >> 12) & UINT32_C(0x000003FF));
	const uint32_t associativity = 1 + (regs.ebx >> 22);

	uint32_t flags = 0;
	if (regs.edx & UINT32_C(0x00000002)) {
		flags |= CPUINFO_CACHE_INCLUSIVE;
	}

	switch (level) {
		case 1:
			switch (type) {
				case cache_type_unified:
<<<<<<< HEAD
					cache->l1d = cache->l1i = (struct cpuinfo_x86_cache){
=======
					cache->l1d = cache->l1i = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags | CPUINFO_CACHE_UNIFIED,
<<<<<<< HEAD
						.apic_bits = apic_bits};
					break;
				case cache_type_data:
					cache->l1d = (struct cpuinfo_x86_cache){
=======
						.apic_bits = apic_bits
					};
					break;
				case cache_type_data:
					cache->l1d = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
<<<<<<< HEAD
						.apic_bits = apic_bits};
					break;
				case cache_type_instruction:
					cache->l1i = (struct cpuinfo_x86_cache){
=======
						.apic_bits = apic_bits
					};
					break;
				case cache_type_instruction:
					cache->l1i = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
<<<<<<< HEAD
						.apic_bits = apic_bits};
=======
						.apic_bits = apic_bits
					};
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
			}
			break;
		case 2:
			switch (type) {
				case cache_type_instruction:
<<<<<<< HEAD
					cpuinfo_log_warning(
						"unexpected L2 instruction cache reported in leaf 0x8000001D is ignored");
=======
					cpuinfo_log_warning("unexpected L2 instruction cache reported in leaf 0x8000001D is ignored");
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
<<<<<<< HEAD
					cache->l2 = (struct cpuinfo_x86_cache){
=======
					cache->l2 = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
<<<<<<< HEAD
						.apic_bits = apic_bits};
=======
						.apic_bits = apic_bits
					};
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
			}
			break;
		case 3:
			switch (type) {
				case cache_type_instruction:
<<<<<<< HEAD
					cpuinfo_log_warning(
						"unexpected L3 instruction cache reported in leaf 0x8000001D is ignored");
=======
					cpuinfo_log_warning("unexpected L3 instruction cache reported in leaf 0x8000001D is ignored");
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
<<<<<<< HEAD
					cache->l3 = (struct cpuinfo_x86_cache){
=======
					cache->l3 = (struct cpuinfo_x86_cache) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
<<<<<<< HEAD
						.apic_bits = apic_bits};
=======
						.apic_bits = apic_bits
					};
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					break;
			}
			break;
		default:
<<<<<<< HEAD
			cpuinfo_log_warning(
				"unexpected L%" PRIu32 " cache reported in leaf 0x8000001D is ignored", level);
=======
			cpuinfo_log_warning("unexpected L%"PRIu32" cache reported in leaf 0x8000001D is ignored", level);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			break;
	}
	return true;
}
