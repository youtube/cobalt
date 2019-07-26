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

#include "starboard/cpu_features.h"

#include <dlfcn.h>         // dlsym, dlclose, dlopen
#include <linux/auxvec.h>  // AT_HWCAP
#include <stdio.h>         // fopen, fclose
#include <string.h>
#include <strings.h>  // strcasecmp
#include <sys/auxv.h>
#include <unistd.h>  // sysconf()
#include <memory>

#include <cstdlib>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/cpu_features.h"

#if SB_API_VERSION >= 11

namespace {

// See <asm/hwcap.h> kernel header.
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

// see <uapi/asm/hwcap.h> kernel header
#define HWCAP2_AES (1 << 0)
#define HWCAP2_PMULL (1 << 1)
#define HWCAP2_SHA1 (1 << 2)
#define HWCAP2_SHA2 (1 << 3)
#define HWCAP2_CRC32 (1 << 4)

// Preset hwcap for Armv8
#define HWCAP_SET_FOR_ARMV8 \
  (HWCAP_VFP | HWCAP_NEON | HWCAP_VFPv3 | HWCAP_VFPv4 | HWCAP_IDIV)

using starboard::shared::SetX86FeaturesInvalid;
using starboard::shared::SetArmFeaturesInvalid;
using starboard::shared::SetGeneralFeaturesInvalid;

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

  // Raspi is a 32-bit ARM platform.
  features->architecture = kSbCPUFeaturesArchitectureArm;

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
    // also contain processor brand information.

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
  features->arm.has_sha1 = (features->hwcap2 & HWCAP2_PMULL) != 0;
  features->arm.has_sha2 = (features->hwcap2 & HWCAP2_SHA2) != 0;
  features->arm.has_crc32 = (features->hwcap2 & HWCAP2_CRC32) != 0;

  // Get L1 ICACHE and DCACHE line size.
  features->icache_line_size = sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
  features->dcache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

  return true;
}

}  // namespace

// TODO: Only ARM is currently implemented and tested
bool SbCPUFeaturesGet(SbCPUFeatures* features) {
#if SB_IS(ARCH_ARM)
  return SbCPUFeaturesGet_ARM(features);
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

#endif  // SB_API_VERSION >= 11
