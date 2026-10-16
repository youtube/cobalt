// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/prctl.h>

#include <memory>

#include "base/android/library_loader/anchor_functions.h"
#include "base/android/library_loader/anchor_functions_buildflags.h"
#include "base/containers/heap_array.h"
#include "base/debug/elf_reader.h"
#include "base/debug/proc_maps_linux.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/memory/page_size.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation_features.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
#include <atomic>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/smaps_categorizer.h"
#include "third_party/abseil-cpp/absl/strings/match.h"
#include "third_party/abseil-cpp/absl/strings/numbers.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#endif

// Symbol with virtual address of the start of ELF header of the current binary.
extern char __ehdr_start;

namespace memory_instrumentation {

namespace {

using mojom::VmRegion;
using mojom::VmRegionPtr;

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
FILE* g_proc_smaps_rollup_for_testing = nullptr;
const size_t kPssValidationThresholdKb = 30720;

base::Lock& GetTestingGlobalsLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}
#endif

const char kClearPeakRssCommand[] = "5";
const uint32_t kMaxLineSize = 4096;

// TODO(chiniforooshan): Many of the utility functions in this anonymous
// namespace should move to base/process/process_metrics_linux.cc to make the
// code a lot cleaner.  However, we should do so after we made sure the metrics
// we are experimenting with here have real value.
base::FilePath GetProcPidDir(base::ProcessId pid) {
  return base::FilePath("/proc").Append(
      pid == base::kNullProcessId ? "self" : base::NumberToString(pid));
}

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
void GetSmapsRollup(base::ProcessHandle handle,
                    size_t* pss,
                    size_t* swap_pss) {
  std::string content;
  bool use_mock = false;
  {
    base::AutoLock lock(GetTestingGlobalsLock());
    if (g_proc_smaps_rollup_for_testing) {
      use_mock = true;
      fseek(g_proc_smaps_rollup_for_testing, 0, SEEK_SET);
      base::ReadStreamToString(g_proc_smaps_rollup_for_testing, &content);
    }
  }

  if (!use_mock) {
    std::string file_name =
        "/proc/" +
        (handle == base::kNullProcessHandle ? "self"
                                            : base::NumberToString(handle)) +
        "/smaps_rollup";
    if (!base::ReadFileToString(base::FilePath(file_name), &content)) {
      *pss = size_t(0);
      *swap_pss = size_t(0);
      return;
    }
  }

  if (content.empty()) {
    *pss = size_t(0);
    *swap_pss = size_t(0);
    return;
  }

  auto value = base::debug::ParseSmapsRollup(content);
  if (!value) {
    *pss = size_t(0);
    *swap_pss = size_t(0);
    return;
  }
  *pss = value->pss;
  *swap_pss = value->swap_pss;
}
#else   // !BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
void GetSmapsRollup(uint32_t* pss, uint32_t* swap_pss) {
  auto value = base::debug::ReadAndParseSmapsRollup();
  if (!value) {
    *pss = 0;
    *swap_pss = 0;
    return;
  }
  *pss = value->pss;
  *swap_pss = value->swap_pss;
}
#endif  // BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
bool ResetPeakRSSIfPossible(base::ProcessId pid) {
  static std::atomic<bool> is_peak_rss_resettable{true};
  if (!is_peak_rss_resettable.load(std::memory_order_relaxed)) {
    return false;
  }
  auto clear_refs_file = GetProcPidDir(pid).Append("clear_refs");
  base::ScopedFD clear_refs_fd(open(clear_refs_file.value().c_str(), O_WRONLY));

  bool success =
      clear_refs_fd.get() >= 0 &&
      base::WriteFileDescriptor(clear_refs_fd.get(), kClearPeakRssCommand);
  if (!success) {
    is_peak_rss_resettable.store(false, std::memory_order_relaxed);
  }
  return success;
}
#else   // !BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
bool ResetPeakRSSIfPossible(base::ProcessId pid) {
  static bool is_peak_rss_resettable = true;
  if (!is_peak_rss_resettable)
    return false;
  auto clear_refs_file = GetProcPidDir(pid).Append("clear_refs");
  base::ScopedFD clear_refs_fd(open(clear_refs_file.value().c_str(), O_WRONLY));
  is_peak_rss_resettable =
      clear_refs_fd.get() >= 0 &&
      base::WriteFileDescriptor(clear_refs_fd.get(), kClearPeakRssCommand);
  return is_peak_rss_resettable;
}
#endif  // BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)

struct ModuleData {
  std::string path;
  std::string build_id;
};

ModuleData GetMainModuleData() {
  ModuleData module_data;
#if !BUILDFLAG(IS_STARBOARD)
  Dl_info dl_info;
  if (dladdr(&__ehdr_start, &dl_info)) {
    base::debug::ElfBuildIdBuffer build_id;
    size_t build_id_length =
        base::debug::ReadElfBuildId(&__ehdr_start, true, build_id);
    if (build_id_length) {
      base::FilePath module_data_path = base::FilePath(dl_info.dli_fname);
      if (module_data_path.IsAbsolute()) {
        module_data.path = dl_info.dli_fname;
      } else {
        module_data.path = base::MakeAbsoluteFilePath(module_data_path).value();
      }
      module_data.build_id = std::string(build_id, build_id_length);
    }
  }
#endif
  return module_data;
}

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
bool ParseSmapsHeader(std::string_view header_line,
                      const ModuleData& main_module_data,
                      VmRegion* region) {
  // e.g., "00400000-00421000 r-xp 00000000 fc:01 1234  /foo.so\n"
  std::vector<std::string_view> tokens = base::SplitStringPiece(
      header_line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() < 5)
    return false;

  std::vector<std::string_view> addresses = base::SplitStringPiece(
      tokens[0], "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (addresses.size() != 2)
    return false;

  uint64_t start_addr = 0;
  uint64_t end_addr = 0;
  if (!base::HexStringToUInt64(addresses[0], &start_addr) ||
      !base::HexStringToUInt64(addresses[1], &end_addr)) {
    return false;
  }
  region->start_address = start_addr;

  if (end_addr > start_addr) {
    region->size_in_bytes = end_addr - start_addr;
  } else {
    region->size_in_bytes = 0;
    return false;
  }

  std::string_view perms = tokens[1];
  if (perms.size() < 4)
    return false;

  region->protection_flags = 0;
  if (perms[0] == 'r') {
    region->protection_flags |= VmRegion::kProtectionFlagsRead;
  }
  if (perms[1] == 'w') {
    region->protection_flags |= VmRegion::kProtectionFlagsWrite;
  }
  if (perms[2] == 'x') {
    region->protection_flags |= VmRegion::kProtectionFlagsExec;
  }
  if (perms[3] == 's') {
    region->protection_flags |= VmRegion::kProtectionFlagsMayshare;
  }

  if (tokens.size() >= 6) {
    size_t dev_pos = header_line.find(tokens[3]);
    if (dev_pos != std::string_view::npos) {
      size_t inode_pos =
          header_line.find(tokens[4], dev_pos + tokens[3].size());
      if (inode_pos != std::string_view::npos) {
        size_t filename_pos =
            header_line.find_first_not_of(' ', inode_pos + tokens[4].size());
        if (filename_pos != std::string_view::npos) {
          region->mapped_file = std::string(header_line.substr(filename_pos));
          base::TrimWhitespaceASCII(region->mapped_file, base::TRIM_ALL,
                                    &region->mapped_file);
        }
      }
    }
  }

  // Build ID is needed to symbolize heap profiles, and is generated only on
  // official builds. Build ID is only added for the current library (chrome)
  // since it is racy to read other libraries which can be unmapped any time.
#if defined(OFFICIAL_BUILD)
  if (!region->mapped_file.empty() &&
      base::StartsWith(main_module_data.path, region->mapped_file,
                       base::CompareCase::SENSITIVE) &&
      !main_module_data.build_id.empty()) {
    region->module_debugid = main_module_data.build_id;
  }
#endif

  return true;
}

uint64_t ReadCounterBytes(std::string_view counter_line) {
  std::vector<std::string_view> tokens = base::SplitStringPiece(
      counter_line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() < 3 || tokens[2] != "kB")
    return 0;
  uint64_t counter_value = 0;
  if (!base::StringToUint64(tokens[1], &counter_value))
    return 0;
  return counter_value * 1024;
}

uint32_t ParseSmapsCounter(std::string_view counter_line, VmRegion* region) {
  if (base::StartsWith(counter_line, "Pss:")) {
    region->byte_stats_proportional_resident = ReadCounterBytes(counter_line);
  } else if (base::StartsWith(counter_line, "Private_Dirty:")) {
    region->byte_stats_private_dirty_resident = ReadCounterBytes(counter_line);
  } else if (base::StartsWith(counter_line, "Private_Clean:")) {
    region->byte_stats_private_clean_resident = ReadCounterBytes(counter_line);
  } else if (base::StartsWith(counter_line, "Shared_Dirty:")) {
    region->byte_stats_shared_dirty_resident = ReadCounterBytes(counter_line);
  } else if (base::StartsWith(counter_line, "Shared_Clean:")) {
    region->byte_stats_shared_clean_resident = ReadCounterBytes(counter_line);
  } else if (base::StartsWith(counter_line, "Swap:")) {
    region->byte_stats_swapped = ReadCounterBytes(counter_line);
  } else if (base::StartsWith(counter_line, "Locked:")) {
    region->byte_locked = ReadCounterBytes(counter_line);
  } else {
    return 0;
  }
  return 1;
}
#else   // !BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
bool ParseSmapsHeader(const char* header_line,
                      const ModuleData& main_module_data,
                      VmRegion* region) {
  // e.g., "00400000-00421000 r-xp 00000000 fc:01 1234  /foo.so\n"
  bool res = true;  // Whether this region should be appended or skipped.
  uint64_t end_addr = 0;
  char protection_flags[5] = {};
  char mapped_file[kMaxLineSize];

  if (sscanf(header_line, "%" SCNx64 "-%" SCNx64 " %4c %*s %*s %*s%4095[^\n]\n",
             &region->start_address, &end_addr, protection_flags,
             mapped_file) != 4) {
    return false;
  }

  if (end_addr > region->start_address) {
    region->size_in_bytes = end_addr - region->start_address;
  } else {
    // This is not just paranoia, it can actually happen (See crbug.com/461237).
    region->size_in_bytes = 0;
    res = false;
  }

  region->protection_flags = 0;
  if (protection_flags[0] == 'r') {
    region->protection_flags |= VmRegion::kProtectionFlagsRead;
  }
  if (protection_flags[1] == 'w') {
    region->protection_flags |= VmRegion::kProtectionFlagsWrite;
  }
  if (protection_flags[2] == 'x') {
    region->protection_flags |= VmRegion::kProtectionFlagsExec;
  }
  if (protection_flags[3] == 's') {
    region->protection_flags |= VmRegion::kProtectionFlagsMayshare;
  }

  region->mapped_file = mapped_file;
  base::TrimWhitespaceASCII(region->mapped_file, base::TRIM_ALL,
                            &region->mapped_file);

  // Build ID is needed to symbolize heap profiles, and is generated only on
  // official builds. Build ID is only added for the current library (chrome)
  // since it is racy to read other libraries which can be unmapped any time.
#if defined(OFFICIAL_BUILD)
  if (!region->mapped_file.empty() &&
      base::StartsWith(main_module_data.path, region->mapped_file,
                       base::CompareCase::SENSITIVE) &&
      !main_module_data.build_id.empty()) {
    region->module_debugid = main_module_data.build_id;
  }
#endif  // defined(OFFICIAL_BUILD)

  return res;
}

uint64_t ReadCounterBytes(char* counter_line) {
  uint64_t counter_value = 0;
  int res = sscanf(counter_line, "%*s %" SCNu64 " kB", &counter_value);
  return res == 1 ? counter_value * 1024 : 0;
}

uint32_t ParseSmapsCounter(char* counter_line, VmRegion* region) {
  // A smaps counter lines looks as follows: "RSS:  0 Kb\n"
  uint32_t res = 1;
  char counter_name[20];
  int did_read = sscanf(counter_line, "%19[^\n ]", counter_name);
  if (did_read != 1)
    return 0;

  if (strcmp(counter_name, "Pss:") == 0) {
    region->byte_stats_proportional_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Private_Dirty:") == 0) {
    region->byte_stats_private_dirty_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Private_Clean:") == 0) {
    region->byte_stats_private_clean_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Shared_Dirty:") == 0) {
    region->byte_stats_shared_dirty_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Shared_Clean:") == 0) {
    region->byte_stats_shared_clean_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Swap:") == 0) {
    region->byte_stats_swapped = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Locked:") == 0) {
    region->byte_locked = ReadCounterBytes(counter_line);
  } else {
    res = 0;
  }

  return res;
}
#endif  // BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)

#if !BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
uint32_t ReadLinuxProcSmapsFile(FILE* smaps_file,
                                std::vector<VmRegionPtr>* maps) {
  if (!smaps_file)
    return 0;

  fseek(smaps_file, 0, SEEK_SET);

  char line[kMaxLineSize];
  const uint32_t kNumExpectedCountersPerRegion = 7;
  uint32_t counters_parsed_for_current_region = 0;
  uint32_t num_valid_regions = 0;
  bool should_add_current_region = false;
  VmRegion region;
  ModuleData main_module_data = GetMainModuleData();
  for (;;) {
    line[0] = '\0';
    if (fgets(line, kMaxLineSize, smaps_file) == nullptr || !strlen(line))
      break;
    if (absl::ascii_isxdigit(static_cast<unsigned char>(line[0])) &&
        !absl::ascii_isupper(static_cast<unsigned char>(line[0]))) {
      region = VmRegion();
      counters_parsed_for_current_region = 0;
      should_add_current_region =
          ParseSmapsHeader(line, main_module_data, &region);
    } else if (should_add_current_region) {
      counters_parsed_for_current_region += ParseSmapsCounter(line, &region);
      DCHECK_LE(counters_parsed_for_current_region,
                kNumExpectedCountersPerRegion);
      if (counters_parsed_for_current_region == kNumExpectedCountersPerRegion) {
        maps->push_back(VmRegion::New(region));
        ++num_valid_regions;
        should_add_current_region = false;
      }
    }
  }
  return num_valid_regions;
}
#endif  // !BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)

// RAII class making the current process dumpable via prctl(PR_SET_DUMPABLE, 1),
// in case it is not currently dumpable as described in proc(5) and prctl(2).
// Noop if the original dumpable state could not be determined.
class ScopedProcessSetDumpable {
 public:
  ScopedProcessSetDumpable() {
    int result = prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
    if (result < 0) {
      PLOG(ERROR) << "prctl";
      AvoidPrctlOnDestruction();
      return;
    }
    was_dumpable_ = result > 0;

    if (!was_dumpable_) {
      if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) != 0) {
        PLOG(ERROR) << "prctl";
        // PR_SET_DUMPABLE is often disallowed, avoid crashing in this case.
        AvoidPrctlOnDestruction();
      }
    }
  }

  ScopedProcessSetDumpable(const ScopedProcessSetDumpable&) = delete;
  ScopedProcessSetDumpable& operator=(const ScopedProcessSetDumpable&) = delete;

  ~ScopedProcessSetDumpable() {
    if (!was_dumpable_) {
      PCHECK(prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) == 0) << "prctl";
    }
  }

 private:
  void AvoidPrctlOnDestruction() { was_dumpable_ = true; }

  bool was_dumpable_;
};

// Count how many mappings exist in a process.
//
// Return 0 in case of error (since a process necessarily has at least a
// mapping, this is an invalid value).
//
// The return value is approximate, as this happens while the process is
// running.
uint32_t CountMappings(base::ProcessId pid) {
  // seq_file only writes out a page-sized amount on each call.
  const size_t read_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  auto buffer = base::HeapArray<char>::Uninit(read_size);

  base::FilePath path = GetProcPidDir(pid).Append("maps");
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DPLOG(ERROR) << "Couldn't open /proc/PID/maps";
    return 0;
  }

  // /proc/PID/smaps has a single line per mapping, without a header, just count
  // the number of newline characters. See man proc(5) for the format.
  uint32_t newline_characters = 0;
  while (true) {
    ssize_t bytes_read =
        HANDLE_EINTR(read(fd.get(), &buffer[0], buffer.size()));
    if (bytes_read < 0) {
      DPLOG(ERROR) << "Couldn't read /proc/PID/maps";
      return 0;
    }

    if (bytes_read == 0) {
      break;
    }

    for (ssize_t i = 0; i < bytes_read; i++) {
      if (buffer[i] == '\n') {
        newline_characters++;
      }
    }
  }

  return newline_characters;
}

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
#if !BUILDFLAG(IS_ANDROID)
struct LibChrobaltMem {
  uint32_t pss_kb = 0;
  uint32_t rss_kb = 0;
};

struct SmapsRollup {
  uint32_t pss_kb = 0;
  uint32_t rss_kb = 0;
};

void GetSmapsRollup(base::ProcessId pid,
                    const char* needle,
                    SmapsRollup* rollup) {
  std::string file_name =
      "/proc/" +
      (pid == base::kNullProcessId ? "self" : base::NumberToString(pid)) +
      "/smaps";
  base::ScopedFILE smaps_file(fopen(file_name.c_str(), "r"));
  if (!smaps_file) {
    return;
  }

  char line[kMaxLineSize];
  uint64_t total_pss_kb = 0;
  uint64_t total_rss_kb = 0;
  bool is_matching_region = false;
  while (fgets(line, kMaxLineSize, smaps_file.get())) {
    if (base::IsHexDigit(static_cast<unsigned char>(line[0])) &&
        !base::IsAsciiUpper(static_cast<unsigned char>(line[0]))) {
      const char* found = strstr(line, needle);
      is_matching_region = (found != nullptr);
    } else if (is_matching_region) {
      uint64_t value_kb = 0;
      if (strncmp(line, "Pss:", 4) == 0) {
        if (sscanf(line, "Pss: %" SCNu64 " kB", &value_kb) == 1) {
          total_pss_kb += value_kb;
        }
      } else if (strncmp(line, "Rss:", 4) == 0) {
        if (sscanf(line, "Rss: %" SCNu64 " kB", &value_kb) == 1) {
          total_rss_kb += value_kb;
        }
      }
    }
  }
  rollup->pss_kb = base::saturated_cast<uint32_t>(total_pss_kb);
  rollup->rss_kb = base::saturated_cast<uint32_t>(total_rss_kb);
}

LibChrobaltMem GetLibChrobaltMem(base::ProcessId pid) {
  SmapsRollup rollup;
  GetSmapsRollup(pid, "libchrobalt", &rollup);
  if (rollup.pss_kb == 0 && rollup.rss_kb == 0) {
    GetSmapsRollup(pid, "libcobalt", &rollup);
  }
  return {rollup.pss_kb, rollup.rss_kb};
}

uint32_t GetPartitionAllocRss(base::ProcessId pid) {
  SmapsRollup rollup;
  GetSmapsRollup(pid, "<anon:partition_alloc>", &rollup);
  return rollup.rss_kb;
}
#else  // BUILDFLAG(IS_ANDROID)
namespace {
constexpr char kLibChrobaltPattern[] = "libchrobalt";
constexpr char kPartitionAllocPattern[] = "partition_alloc";
constexpr char kV8Pattern[] = "v8";
constexpr char kScudoPattern[] = "scudo";
constexpr char kHeapPattern[] = "[heap]";
constexpr char kSoExtension[] = ".so";
constexpr char kApkExtension[] = ".apk";
constexpr char kDexExtension[] = ".dex";
constexpr char kTtfExtension[] = ".ttf";
constexpr char kTtcExtension[] = ".ttc";
constexpr char kFontsPath[] = "fonts/";
constexpr char kAshmemPath[] = "/dev/ashmem/";
constexpr char kMemfdJitPattern[] = "memfd:jit";
constexpr char kArtExtension[] = ".art";
constexpr char kOatExtension[] = ".oat";
constexpr char kVdexExtension[] = ".vdex";
constexpr char kOdexExtension[] = ".odex";
constexpr char kJarExtension[] = ".jar";
constexpr char kHybExtension[] = ".hyb";
constexpr char kDalvikPrefix[] = "dalvik-";
constexpr char kStackAndTlsPattern[] = "stack_and_tls";
constexpr char kStackPattern[] = "[stack]";

enum class RegionType {
  kNone,
  kLibChrobalt,
  kPartitionAlloc,
  kV8,
  kMalloc,
  kCodeOther,
  kFonts,
  kAshmemJit,
  kAndroidRuntime,
  kStacks
};

RegionType GetRegionType(const char* line) {
  if (absl::StrContains(line, kLibChrobaltPattern) ||
      absl::StrContains(line, "libcobalt")) {
    return RegionType::kLibChrobalt;
  }
  if (absl::StrContains(line, kPartitionAllocPattern)) {
    return RegionType::kPartitionAlloc;
  }
  if (absl::StrContains(line, kV8Pattern)) {
    return RegionType::kV8;
  }
  if (absl::StrContains(line, kScudoPattern) ||
      absl::StrContains(line, kHeapPattern)) {
    return RegionType::kMalloc;
  }
  if (absl::StrContains(line, kTtfExtension) ||
      absl::StrContains(line, kTtcExtension) ||
      absl::StrContains(line, kFontsPath)) {
    return RegionType::kFonts;
  }
  if (absl::StrContains(line, kAshmemPath) ||
      absl::StrContains(line, kMemfdJitPattern)) {
    return RegionType::kAshmemJit;
  }
  if (absl::StrContains(line, kArtExtension) ||
      absl::StrContains(line, kOatExtension) ||
      absl::StrContains(line, kVdexExtension) ||
      absl::StrContains(line, kOdexExtension) ||
      absl::StrContains(line, kJarExtension) ||
      absl::StrContains(line, kHybExtension) ||
      absl::StrContains(line, kDalvikPrefix)) {
    return RegionType::kAndroidRuntime;
  }
  if (absl::StrContains(line, kStackAndTlsPattern) ||
      absl::StrContains(line, kStackPattern)) {
    return RegionType::kStacks;
  }
  if (absl::StrContains(line, kSoExtension) ||
      absl::StrContains(line, kApkExtension) ||
      absl::StrContains(line, kDexExtension)) {
    return RegionType::kCodeOther;
  }
  return RegionType::kNone;
}
}  // namespace

void PopulateCobaltSmapsMetrics(base::ProcessId pid,
                                mojom::RawOSMemDump* dump) {

  std::string file_name =
      "/proc/" +
      (pid == base::kNullProcessId ? "self" : base::NumberToString(pid)) +
      "/smaps";
  base::ScopedFILE smaps_file(fopen(file_name.c_str(), "r"));
  if (!smaps_file) {
    return;
  }

  char line[kMaxLineSize];

  uint64_t libchrobalt_pss_kb = 0;
  uint64_t libchrobalt_rss_kb = 0;
  uint64_t pa_rss_kb = 0;
  uint64_t v8_rss_kb = 0;
  uint64_t malloc_rss_kb = 0;
  uint64_t code_other_rss_kb = 0;
  uint64_t fonts_rss_kb = 0;
  uint64_t ashmem_jit_rss_kb = 0;
  uint64_t android_runtime_rss_kb = 0;
  uint64_t stacks_rss_kb = 0;

  RegionType current_type = RegionType::kNone;

  while (fgets(line, kMaxLineSize, smaps_file.get())) {
    if (base::IsHexDigit(static_cast<unsigned char>(line[0]))) {
      current_type = GetRegionType(line);
      continue;
    }

    if (current_type == RegionType::kNone) {
      continue;
    }

    absl::string_view line_sv(line);
    uint64_t value_kb = 0;
    if (absl::StartsWith(line_sv, "Pss:")) {
      size_t num_start = line_sv.find_first_not_of(' ', 4);
      if (num_start != absl::string_view::npos) {
        size_t num_end = line_sv.find(" kB", num_start);
        if (num_end != absl::string_view::npos) {
          if (absl::SimpleAtoi(line_sv.substr(num_start, num_end - num_start),
                               &value_kb)) {
            if (current_type == RegionType::kLibChrobalt) {
              libchrobalt_pss_kb += value_kb;
            }
          }
        }
      }
    } else if (absl::StartsWith(line_sv, "Rss:")) {
      size_t num_start = line_sv.find_first_not_of(' ', 4);
      if (num_start != absl::string_view::npos) {
        size_t num_end = line_sv.find(" kB", num_start);
        if (num_end != absl::string_view::npos) {
          if (absl::SimpleAtoi(line_sv.substr(num_start, num_end - num_start),
                               &value_kb)) {
            switch (current_type) {
              case RegionType::kLibChrobalt:
                libchrobalt_rss_kb += value_kb;
                break;
              case RegionType::kPartitionAlloc:
                pa_rss_kb += value_kb;
                break;
              case RegionType::kV8:
                v8_rss_kb += value_kb;
                break;
              case RegionType::kMalloc:
                malloc_rss_kb += value_kb;
                break;
              case RegionType::kCodeOther:
                code_other_rss_kb += value_kb;
                break;
              case RegionType::kFonts:
                fonts_rss_kb += value_kb;
                break;
              case RegionType::kAshmemJit:
                ashmem_jit_rss_kb += value_kb;
                break;
              case RegionType::kAndroidRuntime:
                android_runtime_rss_kb += value_kb;
                break;
              case RegionType::kStacks:
                stacks_rss_kb += value_kb;
                break;
              default:
                break;
            }
          }
        }
      }
    }
  }
  base::flat_map<std::string, uint64_t> detailed_stats;
  detailed_stats["pss:lib_chrobalt"] = libchrobalt_pss_kb;
  detailed_stats["rss:lib_chrobalt"] = libchrobalt_rss_kb;
  detailed_stats["rss:partition_alloc"] = pa_rss_kb;
  detailed_stats["rss:v8"] = v8_rss_kb;
  detailed_stats["rss:malloc"] = malloc_rss_kb;
  detailed_stats["rss:code_other"] = code_other_rss_kb;
  detailed_stats["rss:fonts"] = fonts_rss_kb;
  detailed_stats["rss:ashmem_jit"] = ashmem_jit_rss_kb;
  detailed_stats["rss:android_runtime"] = android_runtime_rss_kb;
  detailed_stats["rss:stacks"] = stacks_rss_kb;

  dump->detailed_stats_kb = std::move(detailed_stats);
  dump->last_detailed_dump_time = base::TimeTicks::Now();
}
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
}  // namespace

FILE* g_proc_smaps_for_testing = nullptr;

// static
void OSMetrics::SetProcSmapsForTesting(FILE* f) {
#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
  base::AutoLock lock(GetTestingGlobalsLock());
#endif
  g_proc_smaps_for_testing = f;
}

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
// static
void OSMetrics::SetSmapsRollupForTesting(FILE* f) {
  base::AutoLock lock(GetTestingGlobalsLock());
  g_proc_smaps_rollup_for_testing = f;
}
#endif

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
// static
base::File OSMetrics::GetSmapsFileForScanning() {
  base::AutoLock testing_lock(GetTestingGlobalsLock());
  if (g_proc_smaps_for_testing) {
    int fd = dup(fileno(g_proc_smaps_for_testing));
    if (fd >= 0) {
      base::File file(fd);
      file.Seek(base::File::FROM_BEGIN, 0);
      return file;
    }
  }
  return base::File(base::FilePath("/proc/self/smaps"),
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
}
#endif



bool OSMetrics::FillOSMemoryDump(base::ProcessHandle handle,
                                 const MemDumpFlagSet& flags,
                                 mojom::RawOSMemDump* dump) {
  auto info = GetMemoryInfo(handle);
  if (!info.has_value()) {
    return false;
  }

  dump->platform_private_footprint->rss_anon_bytes = info->rss_anon_bytes;
  dump->platform_private_footprint->vm_swap_bytes = info->vm_swap_bytes;
  dump->resident_set_kb =
      base::saturated_cast<uint32_t>(info->resident_set_bytes / 1024);
  dump->peak_resident_set_kb = GetPeakResidentSetSize(handle);
  dump->is_peak_rss_resettable = ResetPeakRSSIfPossible(handle);

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
  dump->vm_size_kb =
      base::saturated_cast<uint32_t>(info->vm_size_bytes / 1024);
  // TODO(cleanup): Remove this legacy code when appropriate.
#if BUILDFLAG(IS_ANDROID)
  PopulateCobaltSmapsMetrics(handle, dump);
#else  // BUILDFLAG(IS_LINUX)
  LibChrobaltMem lib_mem = GetLibChrobaltMem(handle);
  base::flat_map<std::string, uint64_t> detailed_stats;
  detailed_stats["pss:lib_chrobalt"] = lib_mem.pss_kb;
  detailed_stats["rss:lib_chrobalt"] = lib_mem.rss_kb;
  detailed_stats["rss:partition_alloc"] = GetPartitionAllocRss(handle);
  dump->detailed_stats_kb = std::move(detailed_stats);
  dump->last_detailed_dump_time = base::TimeTicks::Now();
#endif  // BUILDFLAG(IS_LINUX)
#endif  // BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
  if (flags.Has(mojom::MemDumpFlags::MEM_DUMP_COUNT_MAPPINGS)) {
    dump->mappings_count = CountMappings(handle);
  }
  if (flags.Has(mojom::MemDumpFlags::MEM_DUMP_PSS)) {
#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
    size_t pss, swap_pss;
    GetSmapsRollup(handle, &pss, &swap_pss);
    dump->pss_kb = base::saturated_cast<uint32_t>(pss / 1024);
    dump->swap_pss_kb = base::saturated_cast<uint32_t>(swap_pss / 1024);
#else
    GetSmapsRollup(&dump->pss_kb, &dump->swap_pss_kb);
#endif
  }



#if BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
  if (flags.Has(mojom::MemDumpFlags::MEM_DUMP_PAGES_BITMAP)) {
    if (!base::android::AreAnchorsSane()) {
      DLOG(WARNING) << "Incorrect code ordering";
      return false;
    }

    std::vector<uint8_t> accessed_pages_bitmap;
    OSMetrics::MappedAndResidentPagesDumpState state =
        OSMetrics::GetMappedAndResidentPages(base::android::kStartOfText,
                                             base::android::kEndOfText,
                                             &accessed_pages_bitmap);
    UMA_HISTOGRAM_ENUMERATION(
        "Memory.NativeLibrary.MappedAndResidentMemoryFootprintCollectionStatus",
        state);

    // MappedAndResidentPagesDumpState |state| can be |kAccessPagemapDenied|
    // for Android devices running a kernel version < 4.4 or because the process
    // is not "dumpable", as described in proc(5).
    if (state != OSMetrics::MappedAndResidentPagesDumpState::kSuccess) {
      return state != OSMetrics::MappedAndResidentPagesDumpState::kFailure;
    }

    dump->native_library_pages_bitmap = std::move(accessed_pages_bitmap);
  }
#endif  // BUILDFLAG(SUPPORTS_CODE_ORDERING)
#endif  //  BUILDFLAG(IS_ANDROID)

  return true;
}

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
bool OSMetrics::FillOSMemoryDump(base::ProcessHandle handle,
                                 const MemDumpFlagSet& flags,
                                 mojom::RawOSMemDump* dump,
                                 base::WeakPtr<DetailedMetricsDelegate> delegate) {
  if (!FillOSMemoryDump(handle, flags, dump)) {
    return false;
  }

  if (flags.Has(mojom::MemDumpFlags::MEM_DUMP_DETAILED_STATS)) {
    // We intentionally ignore the return value of FillDetailedMetrics. If detailed
    // metrics collection or validation fails, we gracefully fall back to basic
    // metrics (which are populated by FillOSMemoryDump above) rather than failing
    // the entire OS memory dump request.
    FillDetailedMetrics(handle, flags, dump, delegate);
  }

  return true;
}
#endif

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
// static
std::vector<VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    base::ProcessHandle handle) {
  std::vector<VmRegionPtr> maps;
  {
    base::AutoLock lock(GetTestingGlobalsLock());
    if (g_proc_smaps_for_testing) {
      fseek(g_proc_smaps_for_testing, 0, SEEK_SET);
      std::string smaps;
      if (base::ReadStreamToString(g_proc_smaps_for_testing, &smaps)) {
        return OSMetrics::GetProcessMemoryMaps(smaps);
      }
      return std::vector<VmRegionPtr>();
    }
  }

  std::string file_name =
      "/proc/" +
      (handle == base::kNullProcessHandle ? "self"
                                          : base::NumberToString(handle)) +
      "/smaps";
  std::string smaps;
  if (!base::ReadFileToString(base::FilePath(file_name), &smaps)) {
    return std::vector<VmRegionPtr>();
  }

  return OSMetrics::GetProcessMemoryMaps(smaps);
}

// static
std::vector<VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    const std::string& smaps_content) {
  std::vector<VmRegionPtr> maps;
  std::vector<std::string_view> lines = base::SplitStringPiece(
      smaps_content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  const uint32_t kNumExpectedCountersPerRegion = 7;
  uint32_t counters_parsed_for_current_region = 0;
  bool should_add_current_region = false;
  VmRegion region;
  ModuleData main_module_data = GetMainModuleData();

  for (const auto& line_view : lines) {
    if (line_view.empty())
      continue;
    if (base::IsHexDigit(line_view[0]) && !base::IsAsciiUpper(line_view[0])) {
      region = VmRegion();
      counters_parsed_for_current_region = 0;
      should_add_current_region =
          ParseSmapsHeader(line_view, main_module_data, &region);
    } else if (should_add_current_region) {
      counters_parsed_for_current_region +=
          ParseSmapsCounter(line_view, &region);
      DCHECK_LE(counters_parsed_for_current_region,
                kNumExpectedCountersPerRegion);
      if (counters_parsed_for_current_region == kNumExpectedCountersPerRegion) {
        maps.push_back(VmRegion::New(region));
        should_add_current_region = false;
      }
    }
  }
  return maps;
}
#else   // !BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
// static
std::vector<VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    base::ProcessHandle handle) {
  std::vector<VmRegionPtr> maps;
  uint32_t res = 0;
  if (g_proc_smaps_for_testing) {
    res = ReadLinuxProcSmapsFile(g_proc_smaps_for_testing, &maps);
  } else {
    std::string file_name =
        "/proc/" +
        (handle == base::kNullProcessHandle ? "self"
                                            : base::NumberToString(handle)) +
        "/smaps";
    base::ScopedFILE smaps_file(fopen(file_name.c_str(), "r"));
    res = ReadLinuxProcSmapsFile(smaps_file.get(), &maps);
  }

  if (!res)
    return std::vector<VmRegionPtr>();

  return maps;
}
#endif  // !BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)

#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
// static
bool OSMetrics::FillDetailedMetrics(base::ProcessHandle handle,
                                    const MemDumpFlagSet& flags,
                                    mojom::RawOSMemDump* dump,
                                    base::WeakPtr<DetailedMetricsDelegate> delegate) {
  if (!delegate) {
    return true;
  }

  static base::NoDestructor<base::Lock> detailed_metrics_lock;
  base::AutoLock lock(*detailed_metrics_lock);

  base::File file_to_scan = GetSmapsFileForScanning();

  base::flat_map<std::string, uint64_t> stats;
  delegate->GetAndResetStats(&stats);
  if (stats.size() <= 1) {
    auto results = SmapsCategorizer::ScanSmaps(std::move(file_to_scan));
    if (!results.has_value()) {
      LOG(WARNING) << "Failed to read detailed metrics, continuing with basic metrics.";
      return true;
    }
    for (const auto& entry : results.value()) {
      if (!delegate) {
        break;
      }
      delegate->OnSmapsEntry(entry.name, entry.metrics);
    }
    delegate->GetAndResetStats(&stats);
  }



  if (!flags.Has(mojom::MemDumpFlags::MEM_DUMP_DETAILED_STATS)) {
    return true;
  }

  // Validate against smaps_rollup.
  size_t rollup_pss, rollup_swap_pss;
  GetSmapsRollup(handle, &rollup_pss, &rollup_swap_pss);

  uint64_t total_pss_kb = 0;
  auto it_total_pss = stats.find("total_pss");
  if (it_total_pss != stats.end()) {
    total_pss_kb = it_total_pss->second;
  }

  // If the difference is too large, we might have inconsistent data.
  size_t rollup_pss_kb = rollup_pss / 1024;
  size_t abs_diff = (total_pss_kb > rollup_pss_kb) ? (total_pss_kb - rollup_pss_kb)
                                                  : (rollup_pss_kb - total_pss_kb);
  
  bool use_mock = false;
  {
    base::AutoLock testing_lock(GetTestingGlobalsLock());
    use_mock = (g_proc_smaps_for_testing != nullptr);
  }

  if (!use_mock && total_pss_kb > 0 && abs_diff > kPssValidationThresholdKb) {
    LOG(WARNING) << "Detailed metrics PSS validation failed. "
                 << "Detailed: " << total_pss_kb
                 << " KiB, Rollup: " << rollup_pss / 1024 << " KiB";
    return false;
  }

  dump->detailed_stats_kb = std::move(stats);
  dump->last_detailed_dump_time = base::TimeTicks::Now();

  return true;
}
#endif

// static
OSMetrics::MappedAndResidentPagesDumpState OSMetrics::GetMappedAndResidentPages(
    const size_t start_address,
    const size_t end_address,
    std::vector<uint8_t>* accessed_pages_bitmap) {
  const char* kPagemap = "/proc/self/pagemap";

  base::ScopedFILE pagemap_file(fopen(kPagemap, "r"));
  if (!pagemap_file.get()) {
    {
      ScopedProcessSetDumpable set_dumpable;
      pagemap_file.reset(fopen(kPagemap, "r"));
    }
    if (!pagemap_file.get()) {
      DLOG(WARNING) << "Could not open " << kPagemap;
      return OSMetrics::MappedAndResidentPagesDumpState::kAccessPagemapDenied;
    }
  }
#if BUILDFLAG(IS_COBALT)
  // Disable stdio buffering for the pagemap file.
  // In musl libc, buffered reads (like fread) are optimized using readv, which
  // splits the read such that the first segment has size N-1. For special files
  // like /proc/self/pagemap, the Linux kernel strictly enforces that all reads
  // must be in multiples of 8 bytes. An unaligned read size (like N-1) will
  // fail with EINVAL. Making the stream unbuffered forces musl to perform
  // direct, aligned reads, avoiding this issue.
  setvbuf(pagemap_file.get(), NULL, _IONBF, 0);
#endif  // BUILDFLAG(IS_COBALT)

  const size_t kPageSize = base::GetPageSize();
  const size_t start_page = start_address / kPageSize;
  // |end_address| is exclusive.
  const size_t end_page = (end_address - 1) / kPageSize;
  const size_t total_pages = end_page - start_page + 1;

  // The pagemap has one 64 bit entry per page or 8 bytes.
  auto offset = static_cast<long>(start_page * 8);
  if (fseek(pagemap_file.get(), offset, SEEK_SET) != 0) {
#if BUILDFLAG(IS_COBALT)
    PLOG(ERROR) << "Error in fseek " << kPagemap << " to offset " << offset;
#else
    DLOG(ERROR) << "Error in fseek " << kPagemap;
#endif  // BUILDFLAG(IS_COBALT)
    return OSMetrics::MappedAndResidentPagesDumpState::kFailure;
  }

  // |entries| will be 2kB/MB (if |kPageSize| = 4096),
  // that would only be ~80kB on Android, and up to 200kB on Linux (for 100MB)
  std::vector<uint64_t> entries(total_pages);
#if BUILDFLAG(IS_COBALT)
  size_t read_bytes = fread(&entries[0], sizeof(uint64_t), total_pages, pagemap_file.get());
  if (read_bytes != total_pages) {
    PLOG(ERROR) << "Error in fread " << kPagemap << ", read " << read_bytes << " of " << total_pages << " entries";
    if (ferror(pagemap_file.get())) {
      LOG(ERROR) << "pagemap_file has error indicator set";
    }
    if (feof(pagemap_file.get())) {
      LOG(ERROR) << "pagemap_file has EOF indicator set";
    }
    return OSMetrics::MappedAndResidentPagesDumpState::kFailure;
  }
#else
  if (fread(&entries[0], sizeof(uint64_t), total_pages, pagemap_file.get()) !=
      total_pages) {
    return OSMetrics::MappedAndResidentPagesDumpState::kFailure;
  }
#endif  // BUILDFLAG(IS_COBALT)

  accessed_pages_bitmap->resize(1 + (total_pages - 1) / 8);
  for (size_t page = 0; page < total_pages; page++) {
    // Bit 63 is "page present" according to
    // https://www.kernel.org/doc/Documentation/vm/pagemap.txt.
    if (entries[page] & (1LL << 63)) {
      auto byte = page / 8;
      auto bit = page & 0x7;
      CHECK_LT(byte, accessed_pages_bitmap->size());
      (*accessed_pages_bitmap)[byte] |= 1 << bit;
    }
  }
  return OSMetrics::MappedAndResidentPagesDumpState::kSuccess;
}

// static
size_t OSMetrics::GetPeakResidentSetSize(base::ProcessId pid) {
  std::string data;
  {
    // Synchronously reading files in /proc does not hit the disk.
    base::ScopedAllowBlocking allow_blocking;
    if (!base::ReadFileToString(GetProcPidDir(pid).Append("status"), &data))
      return 0;
  }
  base::StringPairs pairs;
  base::SplitStringIntoKeyValuePairs(data, ':', '\n', &pairs);
  for (auto& pair : pairs) {
    base::TrimWhitespaceASCII(pair.first, base::TRIM_ALL, &pair.first);
    // VmHWM gives the peak resident set size since the start of the process or
    // since the last time it was reset. HWM stands for "High Water Mark".
    if (pair.first == "VmHWM") {
      base::TrimWhitespaceASCII(pair.second, base::TRIM_ALL, &pair.second);
      auto split_value_str = base::SplitStringPiece(
          pair.second, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      if (split_value_str.size() != 2 || split_value_str[1] != "kB") {
        NOTREACHED();
      }
      size_t res;
      if (!base::StringToSizeT(split_value_str[0], &res)) {
        NOTREACHED();
      }
      return res;
    }
  }
  return 0;
}

}  // namespace memory_instrumentation
