// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file contains implementations of SbCPUFeaturesGet() on all supported
// architectures defined in Starboard header cpu_features.h on linux platform.
// We use the most applicable and precise approach to get CPU features on each
// architecture. To be specific,
//
// On Arm/Arm64, we read CPU version information from /proc/cpuinfo. We get
// feature flags by parsing HWCAP bitmasks retrieved from function getauxval()
// or /proc/self/auxv, or by reading the flags from /proc/cpuinfo.
//
// On X86/X86_64, we read CPU version strings from /proc/cpuinfo. We get other
// CPU version information and feature flags by CPUID instruction, which gives
// more precise and complete information than reading from /proc/cpuinfo.

#include "starboard/cpu_features.h"

#include <dlfcn.h>         // dlsym, dlclose, dlopen
#include <linux/auxvec.h>  // AT_HWCAP
#include <stdio.h>         // fopen, fclose
#include <string.h>
#include <strings.h>   // strcasecmp
#include <sys/auxv.h>  // getauxval()
#include <unistd.h>    // sysconf()
#include <memory>

#include <cstdlib>

#if defined(ANDROID) && (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64))
#include <asm/hwcap.h>
#endif

#include "starboard/common/log.h"
#include "starboard/shared/starboard/cpu_features.h"

namespace {

#if SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)

// Android hwcap.h defines these flags conditionally, depending on target arch
#if SB_IS(32_BIT) || defined(ANDROID)
// See <arch/arm/include/uapi/asm/hwcap.h> kernel header.
#define HWCAP_VFP (1 << 6)
#define HWCAP_IWMMXT (1 << 9)
#define HWCAP_NEON (1 << 12)
#define HWCAP_VFPv3 (1 << 13)
#define HWCAP_VFPv3D16 (1 << 14) /* also set for VFPv4-D16 */
#define HWCAP_VFPv4 (1 << 16)
#define HWCAP_IDIVA (1 << 17)
#define HWCAP_IDIVT (1 << 18)
#define HWCAP_VFPD32 (1 << 19) /* set if VFP has 32 regs (not 16) */
#define HWCAP_IDIV (HWCAP_IDIVA | HWCAP_IDIVT)

#define HWCAP2_AES (1 << 0)
#define HWCAP2_PMULL (1 << 1)
#define HWCAP2_SHA1 (1 << 2)
#define HWCAP2_SHA2 (1 << 3)
#define HWCAP2_CRC32 (1 << 4)

// Preset hwcap for Armv8
#define HWCAP_SET_FOR_ARMV8 \
  (HWCAP_VFP | HWCAP_NEON | HWCAP_VFPv3 | HWCAP_VFPv4 | HWCAP_IDIV)

#elif SB_IS(64_BIT)
// See <arch/arm64/include/uapi/asm/hwcap.h> kernel header
#define HWCAP_FP (1 << 0)
#define HWCAP_ASIMD (1 << 1)
#define HWCAP_AES (1 << 3)
#define HWCAP_PMULL (1 << 4)
#define HWCAP_SHA1 (1 << 5)
#define HWCAP_SHA2 (1 << 6)
#define HWCAP_CRC32 (1 << 7)

#endif  // SB_IS(32_BIT)

#elif SB_IS(ARCH_X86) || SB_IS(ARCH_X64)

// x86_xgetbv returns the value of an Intel Extended Control Register (XCR).
// Currently only XCR0 is defined by Intel so |xcr| should always be zero.
uint64_t x86_xgetbv(uint32_t xcr) {
  uint32_t eax, edx;

  __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
  return (static_cast<uint64_t>(edx) << 32) | eax;
}

#if SB_IS(32_BIT)
void x86_cpuid(int cpuid_info[4], int info_type) {
  __asm__ volatile(
      "mov %%ebx, %%edi\n"
      "cpuid\n"
      "xchg %%edi, %%ebx\n"
      : "=a"(cpuid_info[0]), "=D"(cpuid_info[1]), "=c"(cpuid_info[2]),
        "=d"(cpuid_info[3])
      : "a"(info_type), "c"(0));
}
#elif SB_IS(64_BIT)
void x86_cpuid(int cpuid_info[4], int info_type) {
  __asm__ volatile("cpuid\n"
                   : "=a"(cpuid_info[0]), "=b"(cpuid_info[1]),
                     "=c"(cpuid_info[2]), "=d"(cpuid_info[3])
                   : "a"(info_type), "c"(0));
}
#endif  // SB_IS(32_BIT)

#endif  // SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)

using starboard::shared::SetX86FeaturesInvalid;
using starboard::shared::SetArmFeaturesInvalid;
using starboard::shared::SetGeneralFeaturesInvalid;

// Class that holds the information in system file /proc/cpuinfo.
class ProcCpuInfo {
  // Raw data of the file /proc/cpuinfo
  std::unique_ptr<char[]> file_data_;
  // Size of the raw data
  size_t file_data_size_;

 public:
  explicit ProcCpuInfo(const char* file_path = "/proc/cpuinfo") {
    file_data_size_ = 0;
    // Get the size of the cpuinfo file by reading it until the end. This is
    // required because files under /proc do not always return a valid size
    // when using fseek(0, SEEK_END) + ftell(). Nor can the be mmap()-ed.
    FILE* file = fopen(file_path, "r");
    if (file != nullptr) {
      for (;;) {
        char file_buffer[256];
        size_t data_size = fread(file_buffer, 1, sizeof(file_buffer), file);
        if (data_size == 0) {
          break;
        }
        file_data_size_ += data_size;
      }
      fclose(file);
    }

    // Read the contents of the cpuinfo file.
    file_data_ = std::unique_ptr<char[]>(new char[file_data_size_ + 1]);
    char* file_data_ptr = file_data_.get();
    memset(file_data_ptr, 0, file_data_size_ + 1);

    file = fopen(file_path, "r");
    if (file != nullptr) {
      for (size_t offset = 0; offset < file_data_size_;) {
        size_t data_size =
            fread(file_data_ptr + offset, 1, file_data_size_ - offset, file);
        if (data_size == 0) {
          break;
        }
        offset += data_size;
      }
      fclose(file);
    }

    // Zero-terminate the data.
    file_data_ptr[file_data_size_] = '\0';
  }
  ~ProcCpuInfo() { file_data_.reset(); }

  // Extract the string feature data named by |feature| and store in
  // |out_feature| whose size is |out_feature_size|.
  bool ExtractStringFeature(const char* feature,
                            char* out_feature,
                            size_t out_feature_size) {
    if (feature == nullptr) {
      return false;
    }

    // Look for first feature occurrence, and ensure it starts the line.
    size_t feature_name_size = strlen(feature);
    const char* feature_ptr = nullptr;
    char* file_data_ptr = file_data_.get();

    const char* line_start_ptr = file_data_.get();
    while (line_start_ptr < file_data_ptr + file_data_size_) {
      // Find the end of the line.
      const char* line_end_ptr = strchr(line_start_ptr, '\n');
      if (line_end_ptr == nullptr) {
        line_end_ptr = file_data_ptr + file_data_size_;
      }

      char line_buffer[line_end_ptr - line_start_ptr +
                       1];  // NOLINT(runtime/arrays)
      memset(line_buffer, 0, sizeof(line_buffer));
      memcpy(line_buffer, line_start_ptr, line_end_ptr - line_start_ptr);
      line_buffer[line_end_ptr - line_start_ptr] = '\0';

      // Find the colon
      const char* colon_ptr = strchr(line_buffer, ':');
      if (colon_ptr == nullptr || !isspace(colon_ptr[1])) {
        line_start_ptr = line_end_ptr + 1;
        continue;
      }

      line_buffer[colon_ptr - line_buffer] = '\0';

      // Trim trailing white space of the line before colon
      const char* feature_end_ptr = colon_ptr - 1;
      while (feature_end_ptr >= line_buffer &&
             isspace(line_buffer[feature_end_ptr - line_buffer])) {
        line_buffer[feature_end_ptr - line_buffer] = '\0';
        feature_end_ptr--;
      }
      // Out of boundary
      if (feature_end_ptr < line_buffer) {
        line_start_ptr = line_end_ptr + 1;
        continue;
      }
      // Trim leading white space of the line
      const char* feature_start_ptr = line_buffer;
      while (feature_start_ptr <= feature_end_ptr &&
             isspace(line_buffer[feature_start_ptr - line_buffer])) {
        line_buffer[feature_start_ptr - line_buffer] = '\0';
        feature_start_ptr++;
      }
      // There is no need to check feature_start_ptr out of boundary, because if
      // feature_start_ptr > feature_end_ptr, it means the line before colon is
      // all white space, and feature_end_ptr will be out of boundary already.

      if (strcmp(feature_start_ptr, feature) != 0) {
        line_start_ptr = line_end_ptr + 1;
        continue;
      }

      feature_ptr = colon_ptr + 2 - line_buffer + line_start_ptr;
      break;
    }

    if (feature_ptr == nullptr) {
      return false;
    }

    // Find the end of the line.
    const char* line_end_ptr = strchr(feature_ptr, '\n');
    if (line_end_ptr == nullptr) {
      line_end_ptr = file_data_ptr + file_data_size_;
    }

    // Get the size of the feature data
    int feature_size = line_end_ptr - feature_ptr;

    if (out_feature_size < feature_size + 1) {
      SB_LOG(WARNING) << "CPU Feature " << feature << " is truncated.";
      feature_size = out_feature_size - 1;
    }
    memcpy(out_feature, feature_ptr, feature_size);
    out_feature[feature_size] = '\0';

    return true;
  }

  // Extract a integer feature field identified by |feature| from /proc/cpuinfo
  int ExtractIntegerFeature(const char* feature) {
    int feature_data = -1;
    // Allocate 128 bytes for an integer field.
    char feature_buffer[128] = {0};
    if (!ExtractStringFeature(
            feature, feature_buffer,
            sizeof(feature_buffer) / sizeof(feature_buffer[0]))) {
      return feature_data;
    }

    char* end;
    feature_data = static_cast<int>(strtol(feature_buffer, &end, 0));
    if (end == feature_buffer) {
      feature_data = -1;
    }
    return feature_data;
  }
};

// Check if getauxval() is supported
bool IsGetauxvalSupported() {
  // TODO: figure out which linking flags are needed to use
  // dl* functions. Currently the linker complains symbols
  // like "__dlopen" undefined, even though "-ldl" and "-lc"
  // are added to linker flags.

  // dlerror();
  // void* libc_handle = dlopen("libc.so", RTLD_NOW);
  // if (!libc_handle) {
  //   printf("Could not dlopen() C library: %s\n", dlerror());
  //   return false;
  // }

  // typedef unsigned long getauxval_func_t(unsigned long);
  // getauxval_func_t* func = (getauxval_func_t*)
  //         dlsym(libc_handle, "getauxval");
  // if (!func) {
  //   printf("Could not find getauxval() in C library\n");
  //   return false;
  // }
  // dlclose(libc_handle);
  return true;
}

// Get hwcap bitmask by getauxval() or by reading /proc/self/auxv
uint32_t ReadElfHwcaps(uint32_t hwcap_type) {
  uint32_t hwcap = 0;
  if (IsGetauxvalSupported()) {
    hwcap = static_cast<uint32_t>(getauxval(hwcap_type));
  } else {
    // Read the ELF HWCAP flags by parsing /proc/self/auxv.
    FILE* file_ptr = fopen("/proc/self/auxv", "r");
    if (file_ptr == nullptr) {
      return hwcap;
    }
    struct {
      uint32_t tag;
      uint32_t value;
    } entry;
    for (;;) {
      size_t n = fread(&entry, sizeof(entry), 1, file_ptr);
      if (n == 0 || (entry.tag == 0 && entry.value == 0)) {
        break;
      }
      if (entry.tag == hwcap_type) {
        hwcap = entry.value;
        break;
      }
    }
    fclose(file_ptr);
  }
  return hwcap;
}

#if SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)
// Checks if a space-separated list of items |list|, in the form of a string,
// contains one given item |item|.
bool HasItemInList(const char* list, const char* flag) {
  ssize_t flag_length = strlen(flag);
  const char* list_ptr = list;
  if (list_ptr == nullptr) {
    return false;
  }
  while (*list_ptr != '\0') {
    // Skip whitespace.
    while (isspace(*list_ptr))
      ++list_ptr;

    // Find end of current list flag.
    const char* end_ptr = list_ptr;
    while (*end_ptr != '\0' && !isspace(*end_ptr))
      ++end_ptr;

    if (flag_length == end_ptr - list_ptr &&
        memcmp(list_ptr, flag, flag_length) == 0) {
      return true;
    }

    // Continue to the next flag.
    list_ptr = end_ptr;
  }
  return false;
}

// Construct hwcap bitmask by the feature flags in /proc/cpuinfo
uint32_t ConstructHwcapFromCPUInfo(ProcCpuInfo* cpu_info,
                                   int16_t architecture_generation,
                                   uint32_t hwcap_type) {
  if (hwcap_type == AT_HWCAP && architecture_generation >= 8) {
    // This is a 32-bit ARM binary running on a 64-bit ARM64 kernel.
    // The 'Features' line only lists the optional features that the
    // device's CPU supports, compared to its reference architecture
    // which are of no use for this process.
    SB_LOG(INFO) << "Faking 32-bit ARM HWCaps on ARMv"
                 << architecture_generation;
    return HWCAP_SET_FOR_ARMV8;
  }

  uint32_t hwcap_value = 0;

  // Allocate 1024 bytes for "Features", which is a list of flags.
  char flags_buffer[1024] = {0};
  if (!cpu_info->ExtractStringFeature(
          "Features", flags_buffer,
          sizeof(flags_buffer) / sizeof(flags_buffer[0]))) {
    return hwcap_value;
  }

  if (hwcap_type == AT_HWCAP) {
    hwcap_value |= HasItemInList(flags_buffer, "vfp") ? HWCAP_VFP : 0;
    hwcap_value |= HasItemInList(flags_buffer, "vfpv3") ? HWCAP_VFPv3 : 0;
    hwcap_value |= HasItemInList(flags_buffer, "vfpv3d16") ? HWCAP_VFPv3D16 : 0;
    hwcap_value |= HasItemInList(flags_buffer, "vfpv4") ? HWCAP_VFPv4 : 0;
    hwcap_value |= HasItemInList(flags_buffer, "neon") ? HWCAP_NEON : 0;
    hwcap_value |= HasItemInList(flags_buffer, "idiva") ? HWCAP_IDIVA : 0;
    hwcap_value |= HasItemInList(flags_buffer, "idivt") ? HWCAP_IDIVT : 0;
    hwcap_value |=
        HasItemInList(flags_buffer, "idiv") ? (HWCAP_IDIVA | HWCAP_IDIVT) : 0;
    hwcap_value |= HasItemInList(flags_buffer, "iwmmxt") ? HWCAP_IWMMXT : 0;
  } else if (hwcap_type == AT_HWCAP2) {
    hwcap_value |= HasItemInList(flags_buffer, "aes") ? HWCAP2_AES : 0;
    hwcap_value |= HasItemInList(flags_buffer, "pmull") ? HWCAP2_PMULL : 0;
    hwcap_value |= HasItemInList(flags_buffer, "sha1") ? HWCAP2_SHA1 : 0;
    hwcap_value |= HasItemInList(flags_buffer, "sha2") ? HWCAP2_SHA2 : 0;
    hwcap_value |= HasItemInList(flags_buffer, "crc32") ? HWCAP2_CRC32 : 0;
  }
  return hwcap_value;
}

bool SbCPUFeaturesGet_ARM(SbCPUFeatures* features) {
  memset(features, 0, sizeof(*features));

#if SB_IS(32_BIT)
  features->architecture = kSbCPUFeaturesArchitectureArm;
#elif SB_IS(64_BIT)
  features->architecture = kSbCPUFeaturesArchitectureArm64;
#else
#error "Your platform must be either 32-bit or 64-bit."
#endif

  // Set the default value of the features to be invalid, then fill them in
  // if appropriate.
  SetGeneralFeaturesInvalid(features);
  SetArmFeaturesInvalid(features);
  SetX86FeaturesInvalid(features);

  ProcCpuInfo cpu_info;

  // Extract CPU implementor, variant, revision and part information, which
  // are all integers.
  features->arm.implementer = cpu_info.ExtractIntegerFeature("CPU implementer");
  features->arm.variant = cpu_info.ExtractIntegerFeature("CPU variant");
  features->arm.revision = cpu_info.ExtractIntegerFeature("CPU revision");
  features->arm.part = cpu_info.ExtractIntegerFeature("CPU part");

  // Extract CPU architecture generation from the "CPU Architecture" field.
  // Allocate 128 bytes for "CPU architecture", which is an integer field.
  char architecture_buffer[128] = {0};
  if (cpu_info.ExtractStringFeature(
          "CPU architecture", architecture_buffer,
          sizeof(architecture_buffer) / sizeof(architecture_buffer[0]))) {
    char* end;
    features->arm.architecture_generation =
        static_cast<int>(strtol(architecture_buffer, &end, 10));
    if (end == architecture_buffer) {
      // Kernels older than 3.18 report "CPU architecture: AArch64" on ARMv8.
      if (strcasecmp(architecture_buffer, "AArch64") == 0) {
        features->arm.architecture_generation = 8;
      } else {
        features->arm.architecture_generation = -1;
      }
    }

    // Unfortunately, it seems that certain ARMv6-based CPUs
    // report an incorrect architecture number of 7!
    //
    // See http://code.google.com/p/android/issues/detail?id=10812
    //
    // We try to correct this by looking at the 'elf_platform'
    // feature reported by the 'Processor' field or 'model name'
    // field in Linux v3.8, which is of the form of "(v7l)" for an
    // ARMv7-based CPU, and "(v6l)" for an ARMv6-one. For example,
    // the Raspberry Pi is one popular ARMv6 device that reports
    // architecture 7. The 'Processor' or 'model name' fields
    // also contain processor brand information. "model name" is used in
    // Linux 3.8 and later (3.7
    // and later for arm64) and is shown once per CPU. "Processor" is used in
    // earlier versions and is shown only once at the top of /proc/cpuinfo
    // regardless of the number CPUs.

    // Allocate 256 bytes for "Processor"/"model name" field.
    static char brand_buffer[256] = {0};
    if (!cpu_info.ExtractStringFeature(
            "Processor", brand_buffer,
            sizeof(brand_buffer) / sizeof(brand_buffer[0]))) {
      if (cpu_info.ExtractStringFeature(
              "model name", brand_buffer,
              sizeof(brand_buffer) / sizeof(brand_buffer[0]))) {
        features->brand = brand_buffer;
        if (features->arm.architecture_generation == 7 &&
            HasItemInList(features->brand, "(v6l)")) {
          features->arm.architecture_generation = 6;
        }
      }
    }
  }

#if SB_IS(32_BIT)
  // Get hwcap bitmask and extract the CPU feature flags from it.
  features->hwcap = ReadElfHwcaps(AT_HWCAP);
  if (features->hwcap == 0) {
    features->hwcap = ConstructHwcapFromCPUInfo(
        &cpu_info, features->arm.architecture_generation, AT_HWCAP);
  }

  features->arm.has_idiva = (features->hwcap & HWCAP_IDIVA) != 0;
  features->arm.has_neon = (features->hwcap & HWCAP_NEON) != 0;
  features->arm.has_vfp = (features->hwcap & HWCAP_VFP) != 0;
  features->arm.has_vfp3 = (features->hwcap & (HWCAP_VFPv3 | HWCAP_VFPv3D16 |
                                               HWCAP_VFPv4 | HWCAP_NEON)) != 0;
  features->arm.has_vfp3_d32 =
      (features->arm.has_vfp3 && ((features->hwcap & HWCAP_VFPv3D16) == 0 ||
                                  (features->hwcap & HWCAP_VFPD32) != 0));
  features->arm.has_vfp3_d32 =
      features->arm.has_vfp3_d32 || features->arm.has_neon;

  // Some old kernels will report vfp not vfpv3. Here we make an attempt
  // to detect vfpv3 by checking for vfp *and* neon, since neon is only
  // available on architectures with vfpv3. Checking neon on its own is
  // not enough as it is possible to have neon without vfp.
  if (features->arm.has_vfp && features->arm.has_neon) {
    features->arm.has_vfp3 = true;
  }

  // VFPv3 implies ARMv7, see ARM DDI 0406B, page A1-6.
  if (features->arm.architecture_generation < 7 && features->arm.has_vfp3) {
    features->arm.architecture_generation = 7;
  }

  // ARMv7 implies Thumb2.
  if (features->arm.architecture_generation >= 7) {
    features->arm.has_thumb2 = true;
  }

  // The earliest architecture with Thumb2 is ARMv6T2.
  if (features->arm.has_thumb2 && features->arm.architecture_generation < 6) {
    features->arm.architecture_generation = 6;
  }

  // We don't support any FPUs other than VFP.
  features->has_fpu = features->arm.has_vfp;

  // The following flags are always supported by ARMv8, as mandated by the ARM
  // Architecture Reference Manual.
  if (features->arm.architecture_generation >= 8) {
    features->arm.has_idiva = true;
    features->arm.has_neon = true;
    features->arm.has_thumb2 = true;
    features->arm.has_vfp = true;
    features->arm.has_vfp3 = true;
  }

  // Read hwcaps2 bitmask and extract the CPU feature flags from it.
  features->hwcap2 = ReadElfHwcaps(AT_HWCAP2);
  if (features->hwcap2 == 0) {
    features->hwcap2 = ConstructHwcapFromCPUInfo(
        &cpu_info, features->arm.architecture_generation, AT_HWCAP2);
  }

  features->arm.has_aes = (features->hwcap2 & HWCAP2_AES) != 0;
  features->arm.has_pmull = (features->hwcap2 & HWCAP2_PMULL) != 0;
  features->arm.has_sha1 = (features->hwcap2 & HWCAP2_SHA1) != 0;
  features->arm.has_sha2 = (features->hwcap2 & HWCAP2_SHA2) != 0;
  features->arm.has_crc32 = (features->hwcap2 & HWCAP2_CRC32) != 0;

#elif SB_IS(64_BIT)
  // Read hwcaps bitmask and extract the CPU feature flags from it.
  features->hwcap = ReadElfHwcaps(AT_HWCAP);

  features->arm.has_aes = (features->hwcap & HWCAP_AES) != 0;
  features->arm.has_pmull = (features->hwcap & HWCAP_PMULL) != 0;
  features->arm.has_sha1 = (features->hwcap & HWCAP_SHA1) != 0;
  features->arm.has_sha2 = (features->hwcap & HWCAP_SHA2) != 0;
  features->arm.has_crc32 = (features->hwcap & HWCAP_CRC32) != 0;

#endif  // SB_IS(32_BIT)

  // Get L1 ICACHE and DCACHE line size.
  features->icache_line_size = sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
  features->dcache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

  return true;
}

#elif SB_IS(ARCH_X86) || SB_IS(ARCH_X64)

bool SbCPUFeaturesGet_X86(SbCPUFeatures* features) {
  memset(features, 0, sizeof(*features));

#if SB_IS(32_BIT)
  features->architecture = kSbCPUFeaturesArchitectureX86;
#elif SB_IS(64_BIT)
  features->architecture = kSbCPUFeaturesArchitectureX86_64;
#else
#error "Your platform must be either 32-bit or 64-bit."
#endif

  // Set the default value of the features to be invalid, then fill them in
  // if appropriate.
  SetGeneralFeaturesInvalid(features);
  SetArmFeaturesInvalid(features);
  SetX86FeaturesInvalid(features);

  ProcCpuInfo cpu_info;

  // Extract brand and vendor information.
  // Allocate 256 bytes for "model name" field.
  static char brand_buffer[256] = {0};
  if (cpu_info.ExtractStringFeature(
          "model name", brand_buffer,
          sizeof(brand_buffer) / sizeof(brand_buffer[0]))) {
    features->brand = brand_buffer;
  }

  // Allocate 128 bytes for "vendor_id" field.
  static char vendor_buffer[128] = {0};
  if (cpu_info.ExtractStringFeature(
          "vendor_id", vendor_buffer,
          sizeof(vendor_buffer) / sizeof(vendor_buffer[0]))) {
    features->x86.vendor = vendor_buffer;
  }

  // Get L1 ICACHE and DCACHE line size.
  features->icache_line_size = sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
  features->dcache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

  // Hwcap bitmasks are not applicable to x86/x86_64. They'll remain 0.

  // Use cpuid instruction to get feature flags.
  int cpuid_info[4] = {0};

  // x86_cpuid with an InfoType argument of 0 returns the number of
  // valid Ids in cpuid_info[0].
  x86_cpuid(cpuid_info, 0);
  unsigned int num_ids = cpuid_info[0];

  // Interpret CPU feature information of CPUID level 1. EAX is returned
  // in cpuid_info[0], EBX in cpuid_info[1], ECX in cpuid_info[2],
  // EDX in cpuid_info[3].
  if (num_ids > 0) {
    x86_cpuid(cpuid_info, 1);
    features->x86.signature = cpuid_info[0];
    features->x86.stepping = cpuid_info[0] & 0xF;
    features->x86.model =
        ((cpuid_info[0] >> 4) & 0xF) + ((cpuid_info[0] >> 12) & 0xF0);
    features->x86.family = (cpuid_info[0] >> 8) & 0xF;
    features->x86.type = (cpuid_info[0] >> 12) & 0x3;
    features->x86.ext_model = (cpuid_info[0] >> 16) & 0xF;
    features->x86.ext_family = (cpuid_info[0] >> 20) & 0xFF;

    features->has_fpu = (cpuid_info[3] & (1 << 0)) != 0;

    features->x86.has_cmov = (cpuid_info[3] & (1 << 15)) != 0;
    features->x86.has_mmx = (cpuid_info[3] & (1 << 23)) != 0;
    features->x86.has_sse = (cpuid_info[3] & (1 << 25)) != 0;
    features->x86.has_sse2 = (cpuid_info[3] & (1 < 26)) != 0;
    features->x86.has_tsc = (cpuid_info[3] & (1 << 4)) != 0;

    features->x86.has_sse3 = (cpuid_info[2] & (1 << 0)) != 0;
#if SB_API_VERSION >= 12
    features->x86.has_pclmulqdq = (cpuid_info[2] & (1 << 1)) != 0;
#endif  // SB_API_VERSION >= 12
    features->x86.has_ssse3 = (cpuid_info[2] & (1 << 9)) != 0;
    features->x86.has_sse41 = (cpuid_info[2] & (1 << 19)) != 0;
    features->x86.has_sse42 = (cpuid_info[2] & (1 << 20)) != 0;
    features->x86.has_movbe = (cpuid_info[2] & (1 << 22)) != 0;
    features->x86.has_popcnt = (cpuid_info[2] & (1 << 23)) != 0;
    features->x86.has_osxsave = (cpuid_info[2] & (1 << 27)) != 0;
    features->x86.has_f16c = (cpuid_info[2] & (1 << 29)) != 0;
    features->x86.has_fma3 = (cpuid_info[2] & (1 << 12)) != 0;
    features->x86.has_aesni = (cpuid_info[2] & (1 << 25)) != 0;

    // AVX instructions will generate an illegal instruction exception unless
    //   a) they are supported by the CPU,
    //   b) XSAVE is supported by the CPU and
    //   c) XSAVE is enabled by the kernel.
    // See http://software.intel.com/en-us/blogs/2011/04/14/is-avx-enabled
    //
    // In addition, we have observed some crashes with the xgetbv instruction
    // even after following Intel's example code. (See crbug.com/375968.)
    // Because of that, we also test the XSAVE bit because its description in
    // the CPUID documentation suggests that it signals xgetbv support.
    features->x86.has_avx =
        (cpuid_info[2] & (1 << 28)) != 0 &&
        (cpuid_info[2] & (1 << 26)) != 0 /* XSAVE */ &&
        (cpuid_info[2] & (1 << 27)) != 0 /* OSXSAVE */ &&
        (x86_xgetbv(0) & 6) == 6 /* XSAVE enabled by kernel */;
  }

  // Interpret CPU feature information of CPUID level 7.
  if (num_ids >= 7) {
    x86_cpuid(cpuid_info, 7);
    features->x86.has_avx2 =
        features->x86.has_avx && (cpuid_info[1] & (1 << 5)) != 0;
    features->x86.has_avx512f =
        features->x86.has_avx && (cpuid_info[1] & (1 << 16)) != 0;
    features->x86.has_avx512dq =
        features->x86.has_avx && (cpuid_info[1] & (1 << 17)) != 0;
    features->x86.has_avx512ifma =
        features->x86.has_avx && (cpuid_info[1] & (1 << 21)) != 0;
    features->x86.has_avx512pf =
        features->x86.has_avx && (cpuid_info[1] & (1 << 26)) != 0;
    features->x86.has_avx512er =
        features->x86.has_avx && (cpuid_info[1] & (1 << 27)) != 0;
    features->x86.has_avx512cd =
        features->x86.has_avx && (cpuid_info[1] & (1 << 28)) != 0;
    features->x86.has_avx512bw =
        features->x86.has_avx && (cpuid_info[1] & (1 << 30)) != 0;
    features->x86.has_avx512vl =
        features->x86.has_avx && (cpuid_info[1] & (1 << 31)) != 0;
    features->x86.has_bmi1 = (cpuid_info[1] & (1 << 3)) != 0;
    features->x86.has_bmi2 = (cpuid_info[1] & (1 << 8)) != 0;
  }

  // Query extended IDs.
  x86_cpuid(cpuid_info, 0x80000000);
  unsigned int num_ext_ids = cpuid_info[0];

  // Interpret extended CPU feature information.
  if (num_ext_ids > 0x80000000) {
    // Interpret CPU feature information of CPUID level 0x80000001.
    x86_cpuid(cpuid_info, 0x80000001);
    features->x86.has_lzcnt = (cpuid_info[2] & (1 << 5)) != 0;
    // SAHF must be probed in long mode.
    features->x86.has_sahf = (cpuid_info[2] & (1 << 0)) != 0;
  }

  return true;
}
#endif

}  // namespace

// TODO: Only ARM/ARM64 and X86/X86_64 are currently implemented and tested
bool SbCPUFeaturesGet(SbCPUFeatures* features) {
#if SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)
  return SbCPUFeaturesGet_ARM(features);
#elif SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
  return SbCPUFeaturesGet_X86(features);
#else
  SB_NOTIMPLEMENTED();

  memset(features, 0, sizeof(*features));
  features->architecture = kSbCPUFeaturesArchitectureUnknown;

  SetGeneralFeaturesInvalid(features);
  SetArmFeaturesInvalid(features);
  SetX86FeaturesInvalid(features);

  return false;
#endif
}
