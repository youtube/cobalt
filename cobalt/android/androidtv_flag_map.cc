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
#include "cobalt/browser/switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/display/display_switches.h"

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
constexpr char kDefaultDeviceScaleFactor[] = "1";
constexpr char k720pUiDeviceScaleFactor[] = "1.5";

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

void ExpandAndroidTvFlags(const AndroidTvFlagMap& flags,
                          const AndroidTvDeviceInfo& device,
                          base::CommandLine* cmd_line) {
  CHECK(cmd_line);

  std::vector<std::string> enable_features;
  std::vector<std::string> disable_features;
  std::vector<std::string> js_flags;

  if (!flags.Has(androidtv_flags::kEnableQuic)) {
    AppendSwitchIfAbsent(cmd_line, ::switches::kDisableQuic);
  }

  // Note: DisableLowEndDeviceMode is handled in Java (JavaSwitches.java),
  // because --enable-low-end-device-mode is read before this hook runs.

  if (flags.Has(androidtv_flags::kUseMinorMsForMinorGc)) {
    js_flags.push_back("--minor-ms");
    js_flags.push_back(
        "--minor-ms-min-new-space-capacity-for-concurrent-marking-mb=0");
  }

  if (const auto old_time =
          flags.GetNumeric(androidtv_flags::kV8SetBytecodeOldTime)) {
    js_flags.push_back("--flush-bytecode");
    js_flags.push_back(base::StrCat({"--bytecode-old-time=", *old_time}));
  }

  js_flags.push_back(
      base::StrCat({"--initial-old-space-size=",
                    flags.GetNumeric(androidtv_flags::kV8InitialOldSpaceSize)
                        .value_or(kDefaultInitialOldSpaceSizeMb)}));

  if (flags.Has(androidtv_flags::kV8DisableSparkplug)) {
    js_flags.push_back("--no-sparkplug");
  }

  js_flags.push_back(
      base::StrCat({"--max-old-space-size=",
                    flags.GetNumeric(androidtv_flags::kV8MaxOldSpaceSize)
                        .value_or(kDefaultMaxOldSpaceSizeMb)}));

  if (const auto gpu_mem =
          flags.GetNumeric(androidtv_flags::kForceGpuMemAvailableMb)) {
    AppendSwitchIfAbsent(cmd_line, ::switches::kForceGpuMemAvailableMb,
                         *gpu_mem);
  } else {
#if !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_X86_64)
    AppendSwitchIfAbsent(cmd_line, ::switches::kForceGpuMemAvailableMb,
                         kDefaultForceGpuMemAvailableMb);
#endif
  }

  if (flags.Has(androidtv_flags::kDisableGpuMemoryBufferCompositorResources)) {
    AppendSwitchIfAbsent(
        cmd_line, ::switches::kDisableGpuMemoryBufferCompositorResources);
  }

  if (const auto limit =
          flags.GetNumeric(androidtv_flags::kGpuImageCacheLimitItems)) {
    AppendSwitchIfAbsent(cmd_line, ::switches::kCCImageCacheLimitItems, *limit);
  }

  if (const auto limit_mb =
          flags.GetNumeric(androidtv_flags::kLimitImageDecodeCacheSizeMb)) {
    AppendSwitchIfAbsent(cmd_line, ::switches::kCCImageCacheLimitMbs,
                         *limit_mb);
  }

  if (const auto budget = flags.GetNumeric(
          androidtv_flags::kDecodedImageWorkingSetBudgetBytes)) {
    AppendSwitchIfAbsent(
        cmd_line, ::switches::kDecodedImageWorkingSetBudgetBytes, *budget);
  }

  if (flags.Has(androidtv_flags::kEnableScalingClippedImages)) {
    AppendSwitchIfAbsent(cmd_line, ::switches::kEnableClippedImageScaling);
  }

  std::vector<std::string> mojo_pipe_params;
  if (const auto size = flags.GetNumeric(
          androidtv_flags::kCobaltDynamicMojoPipeSubresourceSize)) {
    mojo_pipe_params.push_back(base::StrCat(
        {network::features::kCobaltDynamicMojoPipeSizingSubresourceSize.name,
         "/", *size}));
  }
  if (const auto size =
          flags.GetNumeric(androidtv_flags::kCobaltDynamicMojoPipeMediaSize)) {
    mojo_pipe_params.push_back(base::StrCat(
        {network::features::kCobaltDynamicMojoPipeSizingMediaSize.name, "/",
         *size}));
  }
  if (flags.Has(androidtv_flags::kEnableCobaltDynamicMojoPipeSizing) ||
      !mojo_pipe_params.empty()) {
    std::string feature = network::features::kCobaltDynamicMojoPipeSizing.name;
    if (!mojo_pipe_params.empty()) {
      base::StrAppend(&feature, {":", base::JoinString(mojo_pipe_params, "/")});
    }
    enable_features.push_back(std::move(feature));
  }

  if (flags.Has(
          androidtv_flags::kEnableCobaltContentLengthAwareMojoPipeSizing)) {
    enable_features.push_back(
        network::features::kCobaltContentLengthAwareMojoPipeSizing.name);
  }

  std::vector<std::string> interest_area_params;
  if (const auto size =
          flags.GetNumeric(androidtv_flags::kInterestAreaSizeInPixels)) {
    interest_area_params.push_back(
        base::StrCat({::features::kInterestAreaSizeInPixels.name, "/", *size}));
  }
  if (const auto delay =
          flags.GetNumeric(androidtv_flags::kReclaimDelayInSeconds)) {
    interest_area_params.push_back(
        base::StrCat({::features::kReclaimDelayInSeconds.name, "/", *delay}));
  }
  if (!interest_area_params.empty()) {
    enable_features.push_back(
        base::StrCat({::features::kSmallerInterestArea.name, ":",
                      base::JoinString(interest_area_params, "/")}));
  }

  if (flags.Has(androidtv_flags::kAvoidCcReuseResource)) {
    AppendSwitchIfAbsent(cmd_line, ::switches::kAvoidCCReuseResource);
  }

  if (flags.Has(androidtv_flags::kCobaltBypassResourceLoadScheduler)) {
    enable_features.push_back(
        blink::features::kCobaltBypassResourceLoadScheduler.name);
  }

  if (flags.Has(androidtv_flags::kCobaltBypassHtmlPreloadScanner)) {
    enable_features.push_back(
        blink::features::kCobaltBypassHTMLPreloadScanner.name);
  }

  if (flags.Has(androidtv_flags::kEnableCobaltMmapFontCache)) {
    enable_features.push_back(kCobaltMmapFontCacheFeature);
  }

  if (flags.Has(androidtv_flags::kAreaBasedVideoBufferBudget)) {
    enable_features.push_back(kAreaBasedVideoBufferBudgetFeature);
  }

  if (flags.Has(
          androidtv_flags::kAllowCriticalMemoryPressureHandlingInForeground)) {
    AppendSwitchIfAbsent(
        cmd_line, switches::kAllowCriticalMemoryPressureHandlingInForeground);
  }

  if (flags.Has(androidtv_flags::kEvictMemoryCacheOnCriticalMemoryPressure)) {
    enable_features.push_back(
        blink::features::kEvictMemoryCacheOnCriticalMemoryPressure.name);
  }

  if (flags.Has(androidtv_flags::kDisableLessAggressiveParkableString)) {
    disable_features.push_back(
        blink::features::kLessAggressiveParkableString.name);
  }

  if (flags.Has(androidtv_flags::kEnableModerateMemoryPressure)) {
    enable_features.push_back(
        base::features::kCobaltEnableModerateMemoryPressure.name);
  }
  if (const auto cooldown =
          flags.GetNumeric(androidtv_flags::kMemoryPressureCooldownInSeconds)) {
    enable_features.push_back(
        base::StrCat({base::features::kCobaltMemoryPressureCooldown.name, ":",
                      base::features::kCobaltMemoryPressureCooldownSeconds.name,
                      "/", *cooldown}));
  }

  // Exposed as a command-line switch so C++ code and non-Activity Java
  // components (such as NetworkStatus) can query it.
  if (flags.Has(androidtv_flags::kUseStarboardLifecycle)) {
    AppendSwitchIfAbsent(cmd_line, switches::kUseStarboardLifecycle);
  }

  if (flags.Has(androidtv_flags::kEnableActivityLifecycleCoordination)) {
    AppendSwitchIfAbsent(cmd_line,
                         switches::kEnableActivityLifecycleCoordination);
  }

  // 720p UI only applies to 1 GB devices with a 1080p+ display.
  const bool force_720p_ui =
      flags.Has(androidtv_flags::kForce720pUiOn1GbDevices) &&
      device.is_1gb_device && device.is_display_at_least_1080p;
  AppendSwitchIfAbsent(
      cmd_line, ::switches::kForceDeviceScaleFactor,
      force_720p_ui ? k720pUiDeviceScaleFactor : kDefaultDeviceScaleFactor);

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
