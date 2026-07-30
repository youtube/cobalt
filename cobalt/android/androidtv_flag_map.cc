// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/android/androidtv_flag_map.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/features.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "cobalt/android/androidtv_flags.h"
#include "cobalt/android/v8_switches.h"
#include "cobalt/browser/switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"

namespace cobalt {
namespace {

// Feature names without an exported C++ declaration.
// Defined in cobalt/renderer/rasterizer/skia/.../SkWoff2FontCache_cobalt.cc.
constexpr char kCobaltMmapFontCacheFeature[] = "CobaltMmapFontCache";
// Defined via STARBOARD_FEATURE in starboard/extension/feature_config.h.
constexpr char kAreaBasedVideoBufferBudgetFeature[] =
    "AreaBasedVideoBufferBudget";

// Experiment-controlled defaults, used when the experiment key is absent.
constexpr char kDefaultInitialOldSpaceSizeMb[] = "64";
constexpr char kDefaultMaxOldSpaceSizeMb[] = "512";
#if !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_X86_64)
constexpr char kDefaultForceGpuMemAvailableMb[] = "64";
#endif

std::string KeepDigits(std::string_view value) {
  std::string digits;
  for (char c : value) {
    if (base::IsAsciiDigit(c)) {
      digits.push_back(c);
    }
  }
  return digits;
}

// User-provided switches take precedence over experiment switches, so only
// append `name` if it is not already on the command line.
void AppendSwitchIfAbsent(base::CommandLine* cmd_line,
                          std::string_view name,
                          std::string_view value = {}) {
  if (cmd_line->HasSwitch(name)) {
    return;
  }
  if (value.empty()) {
    cmd_line->AppendSwitch(name);
  } else {
    cmd_line->AppendSwitchASCII(name, value);
  }
}

// Merges `items` into the comma-separated list held by `switch_name`.
// When `prepend` is true, `items` go before the existing value.
void MergeListSwitch(base::CommandLine* cmd_line,
                     std::string_view switch_name,
                     const std::vector<std::string>& items,
                     bool prepend) {
  if (items.empty()) {
    return;
  }
  std::string merged = base::JoinString(items, ",");
  const std::string existing = cmd_line->GetSwitchValueASCII(switch_name);
  if (!existing.empty()) {
    merged = prepend ? base::StrCat({merged, ",", existing})
                     : base::StrCat({existing, ",", merged});
  }
  cmd_line->AppendSwitchASCII(switch_name, merged);
}

}  // namespace

namespace flags = androidtv_flags;

AndroidTvFlagMap::AndroidTvFlagMap() = default;
AndroidTvFlagMap::AndroidTvFlagMap(
    base::flat_map<std::string, std::string> values)
    : values_(std::move(values)) {}
AndroidTvFlagMap::AndroidTvFlagMap(const AndroidTvFlagMap&) = default;
AndroidTvFlagMap& AndroidTvFlagMap::operator=(const AndroidTvFlagMap&) =
    default;
AndroidTvFlagMap::AndroidTvFlagMap(AndroidTvFlagMap&&) = default;
AndroidTvFlagMap& AndroidTvFlagMap::operator=(AndroidTvFlagMap&&) = default;
AndroidTvFlagMap::~AndroidTvFlagMap() = default;

// static
AndroidTvFlagMap AndroidTvFlagMap::FromKeyValues(
    const std::vector<std::string>& key_values) {
  std::vector<std::pair<std::string, std::string>> entries;
  for (size_t i = 0; i < key_values.size(); i += 2) {
    entries.emplace_back(key_values[i], i + 1 < key_values.size()
                                            ? key_values[i + 1]
                                            : std::string());
  }
  return AndroidTvFlagMap(
      base::flat_map<std::string, std::string>(std::move(entries)));
}

bool AndroidTvFlagMap::Has(std::string_view key) const {
  return values_.contains(key);
}

std::optional<std::string> AndroidTvFlagMap::GetNumeric(
    std::string_view key) const {
  const auto it = values_.find(key);
  if (it == values_.end()) {
    return std::nullopt;
  }
  std::string digits = KeepDigits(it->second);
  if (digits.empty()) {
    return std::nullopt;
  }
  return digits;
}

void ExpandAndroidTvFlags(const AndroidTvFlagMap& flag_map,
                          base::CommandLine* cmd_line) {
  CHECK(cmd_line);

  std::vector<std::string> enable_features;
  std::vector<std::string> disable_features;
  std::vector<std::string> js_flags;

  auto append_switch_if_set = [&](std::string_view key,
                                  std::string_view switch_name) {
    if (flag_map.Has(key)) {
      AppendSwitchIfAbsent(cmd_line, switch_name);
    }
  };
  auto append_numeric_switch = [&](std::string_view key,
                                   std::string_view switch_name) {
    if (const auto val = flag_map.GetNumeric(key)) {
      AppendSwitchIfAbsent(cmd_line, switch_name, *val);
    }
  };
  auto enable_feature_if_set = [&](std::string_view key,
                                   std::string_view feature_name) {
    if (flag_map.Has(key)) {
      enable_features.emplace_back(feature_name);
    }
  };
  auto add_numeric_param = [&](std::vector<std::string>& params,
                               std::string_view key,
                               std::string_view param_name) {
    if (const auto val = flag_map.GetNumeric(key)) {
      params.push_back(base::StrCat({param_name, "/", *val}));
    }
  };

  if (!flag_map.Has(flags::kEnableQuic)) {
    AppendSwitchIfAbsent(cmd_line, ::switches::kDisableQuic);
  }

  if (flag_map.Has(flags::kUseMinorMsForMinorGc)) {
    js_flags.push_back(v8_switches::kMinorMs);
    js_flags.push_back(base::StrCat(
        {v8_switches::kMinorMsMinNewSpaceCapacityForConcurrentMarkingMb, "0"}));
  }

  if (const auto old_time = flag_map.GetNumeric(flags::kV8SetBytecodeOldTime)) {
    js_flags.push_back(v8_switches::kFlushBytecode);
    js_flags.push_back(
        base::StrCat({v8_switches::kBytecodeOldTime, *old_time}));
  }

  js_flags.push_back(
      base::StrCat({v8_switches::kInitialOldSpaceSize,
                    flag_map.GetNumeric(flags::kV8InitialOldSpaceSize)
                        .value_or(kDefaultInitialOldSpaceSizeMb)}));

  if (flag_map.Has(flags::kV8DisableSparkplug)) {
    js_flags.push_back(v8_switches::kNoSparkplug);
  }

  js_flags.push_back(
      base::StrCat({v8_switches::kMaxOldSpaceSize,
                    flag_map.GetNumeric(flags::kV8MaxOldSpaceSize)
                        .value_or(kDefaultMaxOldSpaceSizeMb)}));

  if (const auto gpu_mem =
          flag_map.GetNumeric(flags::kForceGpuMemAvailableMb)) {
    AppendSwitchIfAbsent(cmd_line, ::switches::kForceGpuMemDiscardableLimitMb,
                         *gpu_mem);
  } else {
#if !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_X86_64)
    AppendSwitchIfAbsent(cmd_line, ::switches::kForceGpuMemDiscardableLimitMb,
                         kDefaultForceGpuMemAvailableMb);
#endif
  }

  append_switch_if_set(flags::kDisableGpuMemoryBufferCompositorResources,
                       ::switches::kDisableGpuMemoryBufferCompositorResources);
  append_numeric_switch(flags::kGpuImageCacheLimitItems,
                        ::switches::kCCImageCacheLimitItems);
  append_numeric_switch(flags::kLimitImageDecodeCacheSizeMb,
                        ::switches::kCCImageCacheLimitMbs);
  append_numeric_switch(flags::kDecodedImageWorkingSetBudgetBytes,
                        ::switches::kDecodedImageWorkingSetBudgetBytes);
  append_switch_if_set(flags::kEnableScalingClippedImages,
                       ::switches::kEnableClippedImageScaling);

  std::vector<std::string> mojo_pipe_params;
  add_numeric_param(
      mojo_pipe_params, flags::kCobaltDynamicMojoPipeSubresourceSize,
      network::features::kCobaltDynamicMojoPipeSizingSubresourceSize.name);
  add_numeric_param(
      mojo_pipe_params, flags::kCobaltDynamicMojoPipeMediaSize,
      network::features::kCobaltDynamicMojoPipeSizingMediaSize.name);
  if (flag_map.Has(flags::kEnableCobaltDynamicMojoPipeSizing) ||
      !mojo_pipe_params.empty()) {
    std::string feature = network::features::kCobaltDynamicMojoPipeSizing.name;
    if (!mojo_pipe_params.empty()) {
      base::StrAppend(&feature, {":", base::JoinString(mojo_pipe_params, "/")});
    }
    enable_features.push_back(std::move(feature));
  }

  enable_feature_if_set(
      flags::kEnableCobaltContentLengthAwareMojoPipeSizing,
      network::features::kCobaltContentLengthAwareMojoPipeSizing.name);

  std::vector<std::string> interest_area_params;
  add_numeric_param(interest_area_params, flags::kInterestAreaSizeInPixels,
                    ::features::kInterestAreaSizeInPixels.name);
  add_numeric_param(interest_area_params, flags::kReclaimDelayInSeconds,
                    ::features::kReclaimDelayInSeconds.name);
  if (!interest_area_params.empty()) {
    enable_features.push_back(
        base::StrCat({::features::kSmallerInterestArea.name, ":",
                      base::JoinString(interest_area_params, "/")}));
  }

  append_switch_if_set(flags::kAvoidCcReuseResource,
                       ::switches::kAvoidCCReuseResource);
  enable_feature_if_set(
      flags::kCobaltBypassResourceLoadScheduler,
      blink::features::kCobaltBypassResourceLoadScheduler.name);
  enable_feature_if_set(flags::kCobaltBypassHtmlPreloadScanner,
                        blink::features::kCobaltBypassHTMLPreloadScanner.name);
  enable_feature_if_set(flags::kEnableCobaltMmapFontCache,
                        kCobaltMmapFontCacheFeature);
  enable_feature_if_set(flags::kAreaBasedVideoBufferBudget,
                        kAreaBasedVideoBufferBudgetFeature);
  append_switch_if_set(
      flags::kAllowCriticalMemoryPressureHandlingInForeground,
      switches::kAllowCriticalMemoryPressureHandlingInForeground);
  enable_feature_if_set(
      flags::kEvictMemoryCacheOnCriticalMemoryPressure,
      blink::features::kEvictMemoryCacheOnCriticalMemoryPressure.name);

  if (flag_map.Has(flags::kDisableLessAggressiveParkableString)) {
    disable_features.push_back(
        blink::features::kLessAggressiveParkableString.name);
  }

  enable_feature_if_set(
      flags::kEnableModerateMemoryPressure,
      base::features::kCobaltEnableModerateMemoryPressure.name);
  if (const auto cooldown =
          flag_map.GetNumeric(flags::kMemoryPressureCooldownInSeconds)) {
    enable_features.push_back(
        base::StrCat({base::features::kCobaltMemoryPressureCooldown.name, ":",
                      base::features::kCobaltMemoryPressureCooldownSeconds.name,
                      "/", *cooldown}));
  }

  // Exposed as a command-line switch so C++ code and non-Activity Java
  // components (such as NetworkStatus) can query it.
  append_switch_if_set(flags::kUseStarboardLifecycle,
                       switches::kUseStarboardLifecycle);
  append_switch_if_set(flags::kEnableActivityLifecycleCoordination,
                       switches::kEnableActivityLifecycleCoordination);

  // base::FeatureList is first-wins, so experiment features go after existing
  // (metadata/user) entries and before defaults.
  MergeListSwitch(cmd_line, ::switches::kEnableFeatures, enable_features,
                  /*prepend=*/false);
  // Disabling always wins over enabling, so the position here does not matter.
  MergeListSwitch(cmd_line, ::switches::kDisableFeatures, disable_features,
                  /*prepend=*/false);
  // V8 is last-wins, so experiment flags go before the existing (user) flags.
  MergeListSwitch(cmd_line, blink::switches::kJavaScriptFlags, js_flags,
                  /*prepend=*/true);
}

}  // namespace cobalt
