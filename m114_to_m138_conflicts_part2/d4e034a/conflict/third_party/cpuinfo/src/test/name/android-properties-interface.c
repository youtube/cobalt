#include <string.h>

<<<<<<< HEAD
#include <arm/android/api.h>
#include <arm/api.h>
#include <arm/linux/api.h>
=======
#include <arm/api.h>
#include <arm/linux/api.h>
#include <arm/android/api.h>

>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))

void cpuinfo_arm_android_parse_chipset_properties(
	const char proc_cpuinfo_hardware[CPUINFO_HARDWARE_VALUE_MAX],
	const char ro_product_board[CPUINFO_BUILD_PROP_VALUE_MAX],
	const char ro_board_platform[CPUINFO_BUILD_PROP_VALUE_MAX],
	const char ro_mediatek_platform[CPUINFO_BUILD_PROP_VALUE_MAX],
	const char ro_arch[CPUINFO_BUILD_PROP_VALUE_MAX],
	const char ro_chipname[CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max,
<<<<<<< HEAD
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX]) {
=======
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX])
{
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	struct cpuinfo_android_properties properties;
	strncpy(properties.proc_cpuinfo_hardware, proc_cpuinfo_hardware, CPUINFO_HARDWARE_VALUE_MAX);
	strncpy(properties.ro_product_board, ro_product_board, CPUINFO_BUILD_PROP_VALUE_MAX);
	strncpy(properties.ro_board_platform, ro_board_platform, CPUINFO_BUILD_PROP_VALUE_MAX);
	strncpy(properties.ro_mediatek_platform, ro_mediatek_platform, CPUINFO_BUILD_PROP_VALUE_MAX);
	strncpy(properties.ro_arch, ro_arch, CPUINFO_BUILD_PROP_VALUE_MAX);
	strncpy(properties.ro_chipname, ro_chipname, CPUINFO_BUILD_PROP_VALUE_MAX);

<<<<<<< HEAD
	struct cpuinfo_arm_chipset chipset = cpuinfo_arm_android_decode_chipset(&properties, cores, max_cpu_freq_max);
=======
	struct cpuinfo_arm_chipset chipset =
		cpuinfo_arm_android_decode_chipset(&properties, cores, max_cpu_freq_max);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	if (chipset.series == cpuinfo_arm_chipset_series_unknown) {
		chipset_name[0] = 0;
	} else {
		cpuinfo_arm_chipset_to_string(&chipset, chipset_name);
	}
}

void cpuinfo_arm_android_parse_proc_cpuinfo_hardware(
<<<<<<< HEAD
	const char hardware[CPUINFO_HARDWARE_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	bool is_tegra,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX]) {
	struct cpuinfo_arm_chipset chipset = cpuinfo_arm_linux_decode_chipset_from_proc_cpuinfo_hardware(
		hardware, cores, max_cpu_freq_max, is_tegra);
=======
	const char hardware[CPUINFO_HARDWARE_VALUE_MAX], uint32_t cores, uint32_t max_cpu_freq_max, bool is_tegra,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX])
{
	struct cpuinfo_arm_chipset chipset =
		cpuinfo_arm_linux_decode_chipset_from_proc_cpuinfo_hardware(hardware, cores, max_cpu_freq_max, is_tegra);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	if (chipset.series == cpuinfo_arm_chipset_series_unknown) {
		chipset_name[0] = 0;
	} else {
		cpuinfo_arm_fixup_chipset(&chipset, cores, max_cpu_freq_max);
		cpuinfo_arm_chipset_to_string(&chipset, chipset_name);
	}
}

void cpuinfo_arm_android_parse_ro_product_board(
<<<<<<< HEAD
	const char board[CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX]) {
=======
	const char board[CPUINFO_BUILD_PROP_VALUE_MAX], uint32_t cores, uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX])
{
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	struct cpuinfo_arm_chipset chipset =
		cpuinfo_arm_android_decode_chipset_from_ro_product_board(board, cores, max_cpu_freq_max);
	if (chipset.series == cpuinfo_arm_chipset_series_unknown) {
		chipset_name[0] = 0;
	} else {
		cpuinfo_arm_fixup_chipset(&chipset, cores, max_cpu_freq_max);
		cpuinfo_arm_chipset_to_string(&chipset, chipset_name);
	}
}

void cpuinfo_arm_android_parse_ro_board_platform(
<<<<<<< HEAD
	const char platform[CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX]) {
=======
	const char platform[CPUINFO_BUILD_PROP_VALUE_MAX], uint32_t cores, uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX])
{
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	struct cpuinfo_arm_chipset chipset =
		cpuinfo_arm_android_decode_chipset_from_ro_board_platform(platform, cores, max_cpu_freq_max);
	if (chipset.series == cpuinfo_arm_chipset_series_unknown) {
		chipset_name[0] = 0;
	} else {
		cpuinfo_arm_fixup_chipset(&chipset, cores, max_cpu_freq_max);
		cpuinfo_arm_chipset_to_string(&chipset, chipset_name);
	}
}

void cpuinfo_arm_android_parse_ro_mediatek_platform(
<<<<<<< HEAD
	const char platform[CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX]) {
=======
	const char platform[CPUINFO_BUILD_PROP_VALUE_MAX], uint32_t cores, uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX])
{
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	struct cpuinfo_arm_chipset chipset = cpuinfo_arm_android_decode_chipset_from_ro_mediatek_platform(platform);
	if (chipset.series == cpuinfo_arm_chipset_series_unknown) {
		chipset_name[0] = 0;
	} else {
		cpuinfo_arm_fixup_chipset(&chipset, cores, max_cpu_freq_max);
		cpuinfo_arm_chipset_to_string(&chipset, chipset_name);
	}
}

void cpuinfo_arm_android_parse_ro_arch(
<<<<<<< HEAD
	const char arch[CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX]) {
=======
	const char arch[CPUINFO_BUILD_PROP_VALUE_MAX], uint32_t cores, uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX])
{
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	struct cpuinfo_arm_chipset chipset = cpuinfo_arm_android_decode_chipset_from_ro_arch(arch);
	if (chipset.series == cpuinfo_arm_chipset_series_unknown) {
		chipset_name[0] = 0;
	} else {
		cpuinfo_arm_fixup_chipset(&chipset, cores, max_cpu_freq_max);
		cpuinfo_arm_chipset_to_string(&chipset, chipset_name);
	}
}

void cpuinfo_arm_android_parse_ro_chipname(
<<<<<<< HEAD
	const char chipname[CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX]) {
=======
	const char chipname[CPUINFO_BUILD_PROP_VALUE_MAX], uint32_t cores, uint32_t max_cpu_freq_max,
	char chipset_name[CPUINFO_ARM_CHIPSET_NAME_MAX])
{
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	struct cpuinfo_arm_chipset chipset = cpuinfo_arm_android_decode_chipset_from_ro_chipname(chipname);
	if (chipset.series == cpuinfo_arm_chipset_series_unknown) {
		chipset_name[0] = 0;
	} else {
		cpuinfo_arm_fixup_chipset(&chipset, cores, max_cpu_freq_max);
		cpuinfo_arm_chipset_to_string(&chipset, chipset_name);
	}
}
