// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/loader_app_metrics.h"

#include "starboard/extension/loader_app_metrics.h"

namespace starboard {
namespace shared {
namespace starboard {

namespace {

// Thread safety isn't needed for these global variables since the extension's
// interface specifies that all accesses and mutations must be from the same
// thread.

static CrashpadInstallationStatus g_crashpad_installation_status;

static bool g_elf_library_stored_compressed;

static int64_t g_elf_load_duration_us = -1;

static int64_t g_elf_decompression_duration_us = -1;

static int64_t g_max_sampled_cpu_bytes_during_elf_load = -1;

static SlotSelectionStatus g_slot_selection_status;

void SetCrashpadInstallationStatus(CrashpadInstallationStatus status) {
  g_crashpad_installation_status = status;
}

CrashpadInstallationStatus GetCrashpadInstallationStatus() {
  return g_crashpad_installation_status;
}

void SetElfLibraryStoredCompressed(bool compressed) {
  g_elf_library_stored_compressed = compressed;
}

bool GetElfLibraryStoredCompressed(void) {
  return g_elf_library_stored_compressed;
}

void SetElfLoadDurationMicroseconds(int64_t microseconds) {
  g_elf_load_duration_us = microseconds;
}

int64_t GetElfLoadDurationMicroseconds() {
  return g_elf_load_duration_us;
}

void SetElfDecompressionDurationMicroseconds(int64_t microseconds) {
  g_elf_decompression_duration_us = microseconds;
}

int64_t GetElfDecompressionDurationMicroseconds() {
  return g_elf_decompression_duration_us;
}

void RecordUsedCpuBytesDuringElfLoad(int64_t bytes) {
  if (bytes > g_max_sampled_cpu_bytes_during_elf_load) {
    g_max_sampled_cpu_bytes_during_elf_load = bytes;
  }
}

int64_t GetMaxSampledUsedCpuBytesDuringElfLoad() {
  return g_max_sampled_cpu_bytes_during_elf_load;
}

void SetSlotSelectionStatus(SlotSelectionStatus status) {
  g_slot_selection_status = status;
}

SlotSelectionStatus GetSlotSelectionStatus() {
  return g_slot_selection_status;
}

const StarboardExtensionLoaderAppMetricsApi kLoaderAppMetricsApi = {
    kStarboardExtensionLoaderAppMetricsName,
    3,
    &SetCrashpadInstallationStatus,
    &GetCrashpadInstallationStatus,
    &SetElfLibraryStoredCompressed,
    &GetElfLibraryStoredCompressed,
    &SetElfLoadDurationMicroseconds,
    &GetElfLoadDurationMicroseconds,
    &SetElfDecompressionDurationMicroseconds,
    &GetElfDecompressionDurationMicroseconds,
    &RecordUsedCpuBytesDuringElfLoad,
    &GetMaxSampledUsedCpuBytesDuringElfLoad,
    &SetSlotSelectionStatus,
    &GetSlotSelectionStatus};

}  // namespace

const void* GetLoaderAppMetricsApi() {
  return &kLoaderAppMetricsApi;
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
