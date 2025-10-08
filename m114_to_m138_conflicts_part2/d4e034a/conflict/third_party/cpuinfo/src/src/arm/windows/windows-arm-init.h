#pragma once

<<<<<<< HEAD
/* Efficiency class = 0 means little core, while 1 means big core for now. */
#define MAX_WOA_VALID_EFFICIENCY_CLASSES 2

/* Topology information hard-coded by SoC/chip name */
struct core_info_by_chip_name {
	enum cpuinfo_vendor vendor;
=======
/* List of known and supported Windows on Arm SoCs/chips. */
enum woa_chip_name {
	woa_chip_name_microsoft_sq_1 = 0,
	woa_chip_name_microsoft_sq_2 = 1,
	woa_chip_name_unknown = 2,
	woa_chip_name_last = woa_chip_name_unknown
};

/* Topology information hard-coded by SoC/chip name */
struct core_info_by_chip_name {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	enum cpuinfo_uarch uarch;
	uint64_t frequency; /* Hz */
};

/* SoC/chip info that's currently not readable by logical system information,
 * but can be read from registry.
 */
struct woa_chip_info {
<<<<<<< HEAD
	wchar_t* chip_name_string;
	struct core_info_by_chip_name uarchs[MAX_WOA_VALID_EFFICIENCY_CLASSES];
};

bool cpu_info_init_by_logical_sys_info(const struct woa_chip_info* chip_info, enum cpuinfo_vendor vendor);
=======
	char* chip_name_string;
	enum woa_chip_name chip_name;
	struct core_info_by_chip_name uarchs[woa_chip_name_last];
};

bool get_core_uarch_for_efficiency(
	enum woa_chip_name chip, BYTE EfficiencyClass,
	enum cpuinfo_uarch* uarch, uint64_t* frequency);

bool cpu_info_init_by_logical_sys_info(
	const struct woa_chip_info *chip_info,
	enum cpuinfo_vendor vendor);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
