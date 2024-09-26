// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/gwp_asan.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <tuple>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_alloc_support.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_math.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "components/gwp_asan/client/sampling_malloc_shims.h"
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "components/gwp_asan/client/sampling_partitionalloc_shims.h"
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

namespace gwp_asan {

namespace internal {
namespace {

constexpr int kDefaultMaxAllocations = 70;
constexpr int kDefaultMaxMetadata = 255;

#if defined(ARCH_CPU_64_BITS)
constexpr int kDefaultTotalPages = 2048;
#else
// Use much less virtual memory on 32-bit builds (where OOMing due to lack of
// address space is a concern.)
constexpr int kDefaultTotalPages = kDefaultMaxMetadata * 2;
#endif

// The allocation sampling frequency is calculated using the formula:
// multiplier * range**rand
// where rand is a random real number in the range [0,1).
constexpr int kDefaultAllocationSamplingMultiplier = 1000;
constexpr int kDefaultAllocationSamplingRange = 16;

constexpr double kDefaultProcessSamplingProbability = 0.015;
// The multiplier to increase the ProcessSamplingProbability in scenarios where
// we want to perform additional testing (e.g., on canary/dev builds).
constexpr int kDefaultProcessSamplingBoost2 = 10;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
constexpr base::FeatureState kDefaultEnabled = base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kDefaultEnabled =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

[[maybe_unused]] constexpr bool kDefaultEnableLightweightDetector = false;
constexpr int kDefaultMaxLightweightMetadata = 255;

BASE_FEATURE(kGwpAsanMalloc, "GwpAsanMalloc", kDefaultEnabled);
BASE_FEATURE(kGwpAsanPartitionAlloc, "GwpAsanPartitionAlloc", kDefaultEnabled);

// Returns whether this process should be sampled to enable GWP-ASan.
bool SampleProcess(const base::Feature& feature, bool boost_sampling) {
  double process_sampling_probability =
      GetFieldTrialParamByFeatureAsDouble(feature, "ProcessSamplingProbability",
                                          kDefaultProcessSamplingProbability);
  if (process_sampling_probability < 0.0 ||
      process_sampling_probability > 1.0) {
    DLOG(ERROR) << "GWP-ASan ProcessSamplingProbability is out-of-range: "
                << process_sampling_probability;
    return false;
  }

  int process_sampling_boost = GetFieldTrialParamByFeatureAsInt(
      feature, "ProcessSamplingBoost2", kDefaultProcessSamplingBoost2);
  if (process_sampling_boost < 1) {
    DLOG(ERROR) << "GWP-ASan ProcessSampling multiplier is out-of-range: "
                << process_sampling_boost;
    return false;
  }

  base::CheckedNumeric<double> sampling_prob_mult =
      process_sampling_probability;
  if (boost_sampling)
    sampling_prob_mult *= process_sampling_boost;
  if (!sampling_prob_mult.IsValid()) {
    DLOG(ERROR) << "GWP-ASan multiplier caused out-of-range multiply: "
                << process_sampling_boost;
    return false;
  }

  process_sampling_probability = sampling_prob_mult.ValueOrDie();
  return (base::RandDouble() < process_sampling_probability);
}

// Returns the allocation sampling frequency, or 0 on error.
size_t AllocationSamplingFrequency(const base::Feature& feature) {
  int multiplier =
      GetFieldTrialParamByFeatureAsInt(feature, "AllocationSamplingMultiplier",
                                       kDefaultAllocationSamplingMultiplier);
  if (multiplier < 1) {
    DLOG(ERROR) << "GWP-ASan AllocationSamplingMultiplier is out-of-range: "
                << multiplier;
    return 0;
  }

  int range = GetFieldTrialParamByFeatureAsInt(
      feature, "AllocationSamplingRange", kDefaultAllocationSamplingRange);
  if (range < 1) {
    DLOG(ERROR) << "GWP-ASan AllocationSamplingRange is out-of-range: "
                << range;
    return 0;
  }

  base::CheckedNumeric<size_t> frequency = multiplier;
  frequency *= std::pow(range, base::RandDouble());
  if (!frequency.IsValid()) {
    DLOG(ERROR) << "Out-of-range multiply " << multiplier << " " << range;
    return 0;
  }

  return frequency.ValueOrDie();
}

}  // namespace

// Exported for testing.
GWP_ASAN_EXPORT absl::optional<AllocatorSettings> GetAllocatorSettings(
    const base::Feature& feature,
    bool boost_sampling,
    const char* process_type) {
  if (!base::FeatureList::IsEnabled(feature))
    return absl::nullopt;

  static_assert(
      AllocatorState::kMaxRequestedSlots <= std::numeric_limits<int>::max(),
      "kMaxRequestedSlots out of range");
  constexpr int kMaxRequestedSlots =
      static_cast<int>(AllocatorState::kMaxRequestedSlots);

  static_assert(AllocatorState::kMaxMetadata <= std::numeric_limits<int>::max(),
                "kMaxMetadata out of range");
  constexpr int kMaxMetadata = static_cast<int>(AllocatorState::kMaxMetadata);

  static_assert(AllocatorState::kMaxLightweightMetadata <=
                    std::numeric_limits<int>::max(),
                "kMaxMetadata out of range");
  constexpr int kMaxLightweightMetadata =
      static_cast<int>(AllocatorState::kMaxLightweightMetadata);

  int total_pages = GetFieldTrialParamByFeatureAsInt(feature, "TotalPages",
                                                     kDefaultTotalPages);
  if (total_pages < 1 || total_pages > kMaxRequestedSlots) {
    DLOG(ERROR) << "GWP-ASan TotalPages is out-of-range: " << total_pages;
    return absl::nullopt;
  }

  int max_metadata = GetFieldTrialParamByFeatureAsInt(feature, "MaxMetadata",
                                                      kDefaultMaxMetadata);
  if (max_metadata < 1 || max_metadata > std::min(total_pages, kMaxMetadata)) {
    DLOG(ERROR) << "GWP-ASan MaxMetadata is out-of-range: " << max_metadata
                << " with TotalPages = " << total_pages;
    return absl::nullopt;
  }

  int max_allocations = GetFieldTrialParamByFeatureAsInt(
      feature, "MaxAllocations", kDefaultMaxAllocations);
  if (max_allocations < 1 || max_allocations > max_metadata) {
    DLOG(ERROR) << "GWP-ASan MaxAllocations is out-of-range: "
                << max_allocations << " with MaxMetadata = " << max_metadata;
    return absl::nullopt;
  }

  LightweightDetector::State lightweight_detector_state =
// The detector is not used on 32-bit systems because pointers there aren't big
// enough to safely store metadata IDs.
#if defined(ARCH_CPU_64_BITS)
      // At the moment, BRP is the only client of the detector, and this extra
      // check allows us to reduce the memory usage in BRP-disabled processes.
      (base::allocator::PartitionAllocSupport::GetBrpConfiguration(process_type)
           .enable_brp &&
       GetFieldTrialParamByFeatureAsBool(feature, "EnableLightweightDetector",
                                         kDefaultEnableLightweightDetector))
          ? LightweightDetector::State::kEnabled
          :
#endif  // defined(ARCH_CPU_64_BITS)
          LightweightDetector::State::kDisabled;

  int max_lightweight_metadata = 0;
  if (lightweight_detector_state == LightweightDetector::State::kEnabled) {
    max_lightweight_metadata = GetFieldTrialParamByFeatureAsInt(
        feature, "MaxLightweightMetadata", kDefaultMaxLightweightMetadata);
    if (max_lightweight_metadata < 1 ||
        max_lightweight_metadata > kMaxLightweightMetadata) {
      DLOG(ERROR) << "GWP-ASan MaxLightweightMetadata is out-of-range: "
                  << max_lightweight_metadata;
      return absl::nullopt;
    }
  }

  size_t alloc_sampling_freq = AllocationSamplingFrequency(feature);
  if (!alloc_sampling_freq)
    return absl::nullopt;

  if (!SampleProcess(feature, boost_sampling))
    return absl::nullopt;

  return AllocatorSettings{static_cast<size_t>(max_allocations),
                           static_cast<size_t>(max_metadata),
                           static_cast<size_t>(total_pages),
                           alloc_sampling_freq,
                           lightweight_detector_state,
                           static_cast<size_t>(max_lightweight_metadata)};
}

}  // namespace internal

void EnableForMalloc(bool boost_sampling, const char* process_type) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  static bool init_once = [&]() -> bool {
    auto settings = internal::GetAllocatorSettings(
        internal::kGwpAsanMalloc, boost_sampling, process_type);
    if (!settings)
      return false;

    internal::InstallMallocHooks(
        settings->max_allocated_pages, settings->num_metadata,
        settings->total_pages, settings->sampling_frequency, base::DoNothing());
    return true;
  }();
  std::ignore = init_once;
#else
  std::ignore = internal::kGwpAsanMalloc;
  DLOG(WARNING) << "base::allocator shims are unavailable for GWP-ASan.";
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
}

void EnableForPartitionAlloc(bool boost_sampling, const char* process_type) {
#if BUILDFLAG(USE_PARTITION_ALLOC)
  static bool init_once = [&]() -> bool {
    auto settings = internal::GetAllocatorSettings(
        internal::kGwpAsanPartitionAlloc, boost_sampling, process_type);
    if (!settings)
      return false;

    internal::InstallPartitionAllocHooks(
        settings->max_allocated_pages, settings->num_metadata,
        settings->total_pages, settings->sampling_frequency, base::DoNothing(),
        settings->lightweight_detector_state,
        settings->num_lightweight_metadata);
    return true;
  }();
  std::ignore = init_once;
#else
  std::ignore = internal::kGwpAsanPartitionAlloc;
  DLOG(WARNING) << "PartitionAlloc hooks are unavailable for GWP-ASan.";
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)
}

}  // namespace gwp_asan
