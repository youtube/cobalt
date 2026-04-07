// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_PATTERNS_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_PATTERNS_H_

#include <string>

#include "base/strings/string_util.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

inline constexpr char kLibChrobaltPattern[] = "libchrobalt.so";
inline constexpr char kPartitionAllocPattern[] = "partition_alloc";
inline constexpr char kV8Pattern[] = "v8";
inline constexpr char kScudoPattern[] = "scudo";
inline constexpr char kHeapPattern[] = "[heap]";
inline constexpr char kSoExtension[] = ".so";
inline constexpr char kApkExtension[] = ".apk";
inline constexpr char kDexExtension[] = ".dex";
inline constexpr char kTtfExtension[] = ".ttf";
inline constexpr char kTtcExtension[] = ".ttc";
inline constexpr char kFontsPath[] = "fonts/";
inline constexpr char kAshmemPath[] = "/dev/ashmem/";
inline constexpr char kMemfdJitPattern[] = "memfd:jit";
inline constexpr char kArtExtension[] = ".art";
inline constexpr char kOatExtension[] = ".oat";
inline constexpr char kVdexExtension[] = ".vdex";
inline constexpr char kOdexExtension[] = ".odex";
inline constexpr char kJarExtension[] = ".jar";
inline constexpr char kHybExtension[] = ".hyb";
inline constexpr char kDalvikPrefix[] = "dalvik-";
inline constexpr char kStackAndTlsPattern[] = "stack_and_tls";
inline constexpr char kStackPattern[] = "[stack]";

inline mojom::CobaltMemoryCategory GetMemoryCategoryForMappedFile(
    const std::string& mapped_file) {
  if (mapped_file.find(kLibChrobaltPattern) != std::string::npos) {
    return mojom::CobaltMemoryCategory::kLibChrobalt;
  }
  if (mapped_file.find(kPartitionAllocPattern) != std::string::npos) {
    return mojom::CobaltMemoryCategory::kPartitionAlloc;
  }
  if (mapped_file.find(kV8Pattern) != std::string::npos) {
    return mojom::CobaltMemoryCategory::kV8;
  }
  if (mapped_file.find(kScudoPattern) != std::string::npos ||
      mapped_file.find(kHeapPattern) != std::string::npos) {
    return mojom::CobaltMemoryCategory::kMalloc;
  }
  if (base::EndsWith(mapped_file, kTtfExtension, base::CompareCase::SENSITIVE) ||
      base::EndsWith(mapped_file, kTtcExtension, base::CompareCase::SENSITIVE) ||
      mapped_file.find(kFontsPath) != std::string::npos) {
    return mojom::CobaltMemoryCategory::kFonts;
  }
  if (mapped_file.find(kAshmemPath) != std::string::npos ||
      mapped_file.find(kMemfdJitPattern) != std::string::npos) {
    return mojom::CobaltMemoryCategory::kAshmemJit;
  }
  if (base::EndsWith(mapped_file, kArtExtension, base::CompareCase::SENSITIVE) ||
      base::EndsWith(mapped_file, kOatExtension, base::CompareCase::SENSITIVE) ||
      base::EndsWith(mapped_file, kVdexExtension, base::CompareCase::SENSITIVE) ||
      base::EndsWith(mapped_file, kOdexExtension, base::CompareCase::SENSITIVE) ||
      base::EndsWith(mapped_file, kJarExtension, base::CompareCase::SENSITIVE) ||
      base::EndsWith(mapped_file, kHybExtension, base::CompareCase::SENSITIVE) ||
      base::StartsWith(mapped_file, kDalvikPrefix,
                       base::CompareCase::SENSITIVE)) {
    return mojom::CobaltMemoryCategory::kAndroidRuntime;
  }
  if (mapped_file.find(kStackAndTlsPattern) != std::string::npos ||
      mapped_file.find(kStackPattern) != std::string::npos) {
    return mojom::CobaltMemoryCategory::kStacks;
  }
  if (base::EndsWith(mapped_file, kSoExtension, base::CompareCase::SENSITIVE) ||
      base::EndsWith(mapped_file, kApkExtension, base::CompareCase::SENSITIVE) ||
      base::EndsWith(mapped_file, kDexExtension,
                     base::CompareCase::SENSITIVE)) {
    return mojom::CobaltMemoryCategory::kCodeOther;
  }
  return mojom::CobaltMemoryCategory::kUnknown;
}

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_PATTERNS_H_
