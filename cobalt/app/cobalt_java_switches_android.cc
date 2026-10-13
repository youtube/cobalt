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

#include "cobalt/app/cobalt_java_switches_android.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/features.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "cobalt/browser/constants/cobalt_java_switch_names.h"
#include "cobalt/common/features/features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/display/display_switches.h"

void ApplyJavaSwitches() {
  cobalt::ApplyJavaSwitches(base::CommandLine::ForCurrentProcess());
}

namespace cobalt {
namespace {

constexpr char kForce720pDeviceScaleFactor[] = "1.5";

// V8 engine flags defined in //v8/src/flags/flag-definitions.h and passed via
// `--js-flags` (blink::switches::kJavaScriptFlags).
constexpr char kV8MinorMSFlag[] = "--minor-ms";
constexpr char kV8MinorMSMinNewSpaceCapacityForConcurrentMarkingMbFlag[] =
    "--minor-ms-min-new-space-capacity-for-concurrent-marking-mb=0";
constexpr char kV8FlushBytecodeFlag[] = "--flush-bytecode";
constexpr char kV8BytecodeOldTimePrefix[] = "--bytecode-old-time=";
constexpr char kV8InitialOldSpaceSizePrefix[] = "--initial-old-space-size=";
constexpr char kV8NoSparkplugFlag[] = "--no-sparkplug";
constexpr char kV8MaxOldSpaceSizePrefix[] = "--max-old-space-size=";

base::flat_map<std::string, std::string> ParseSerializedJavaSwitches(
    std::string_view serialized) {
  base::flat_map<std::string, std::string> java_switches;
  for (std::string_view entry : base::SplitStringPiece(
           serialized, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (auto kv = base::SplitStringOnce(entry, '=')) {
      if (!kv->first.empty()) {
        java_switches[std::string(kv->first)] = std::string(kv->second);
      }
    } else {
      java_switches[std::string(entry)] = "";
    }
  }
  return java_switches;
}

std::string GetSanitizedNumericValue(
    const base::flat_map<std::string, std::string>& java_switches,
    std::string_view key) {
  auto it = java_switches.find(key);
  if (it == java_switches.end()) {
    return "";
  }
  std::string digits;
  for (char c : it->second) {
    if (base::IsAsciiDigit(c)) {
      digits.push_back(c);
    }
  }
  return digits;
}

void PrependCommaSeparatedSwitch(base::CommandLine* cmd_line,
                                 std::string_view switch_name,
                                 const std::vector<std::string>& items) {
  if (items.empty()) {
    return;
  }
  std::string joined = base::JoinString(items, ",");
  std::string existing = cmd_line->GetSwitchValueASCII(switch_name);
  if (!existing.empty()) {
    cmd_line->AppendSwitchASCII(switch_name,
                                base::StrCat({joined, ",", existing}));
  } else {
    cmd_line->AppendSwitchASCII(switch_name, joined);
  }
}

void AppendCommaSeparatedSwitch(base::CommandLine* cmd_line,
                                std::string_view switch_name,
                                const std::vector<std::string>& items) {
  if (items.empty()) {
    return;
  }
  std::string joined = base::JoinString(items, ",");
  std::string existing = cmd_line->GetSwitchValueASCII(switch_name);
  if (!existing.empty()) {
    cmd_line->AppendSwitchASCII(switch_name,
                                base::StrCat({existing, ",", joined}));
  } else {
    cmd_line->AppendSwitchASCII(switch_name, joined);
  }
}

}  // namespace

void ApplyJavaSwitches(base::CommandLine* cmd_line) {
  CHECK(cmd_line);

  const base::flat_map<std::string, std::string> java_switches =
      ParseSerializedJavaSwitches(
          cmd_line->GetSwitchValueASCII(kCobaltJavaSwitches));

  if (!java_switches.contains(kEnableQUIC)) {
    cmd_line->AppendSwitch(::switches::kDisableQuic);
  }

  // TODO(cobalt, b/563373348): Investigate performance impact on high-end
  // devices. Use Java switch due to IsLowEndDevice called before Finch is
  // initialized. We should migrate to Finch if high-end device benefits from
  // removing this low-end-device-mode flag.
  if (!java_switches.contains(kDisableLowEndDeviceMode)) {
    cmd_line->AppendSwitch(::switches::kEnableLowEndDeviceMode);
  }

  std::vector<std::string> js_flags;
  if (java_switches.contains(kUseMinorMSForMinorGC)) {
    js_flags.emplace_back(kV8MinorMSFlag);
    js_flags.emplace_back(
        kV8MinorMSMinNewSpaceCapacityForConcurrentMarkingMbFlag);
  }

  std::string old_time =
      GetSanitizedNumericValue(java_switches, kV8SetBytecodeOldTime);
  if (!old_time.empty()) {
    js_flags.emplace_back(kV8FlushBytecodeFlag);
    js_flags.push_back(base::StrCat({kV8BytecodeOldTimePrefix, old_time}));
  }

  std::string initial_old_space =
      GetSanitizedNumericValue(java_switches, kV8InitialOldSpaceSize);
  js_flags.push_back(
      base::StrCat({kV8InitialOldSpaceSizePrefix,
                    initial_old_space.empty() ? kDefaultInitialOldSpaceSize
                                              : initial_old_space}));

  if (java_switches.contains(kV8DisableSparkplug)) {
    js_flags.emplace_back(kV8NoSparkplugFlag);
  }

  std::string max_old_space =
      GetSanitizedNumericValue(java_switches, kV8MaxOldSpaceSize);
  js_flags.push_back(base::StrCat(
      {kV8MaxOldSpaceSizePrefix,
       max_old_space.empty() ? kDefaultMaxOldSpaceSize : max_old_space}));

  AppendCommaSeparatedSwitch(cmd_line, blink::switches::kJavaScriptFlags,
                             js_flags);

  std::string force_gpu_mem =
      GetSanitizedNumericValue(java_switches, kForceGpuMemAvailableMb);
  if (!force_gpu_mem.empty()) {
    cmd_line->AppendSwitchASCII(::switches::kForceGpuMemAvailableMb,
                                force_gpu_mem);
  } else if (!cmd_line->HasSwitch(::switches::kForceGpuMemAvailableMb)) {
#if !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_X86_64)
    cmd_line->AppendSwitchASCII(::switches::kForceGpuMemAvailableMb,
                                kDefaultForceGpuMemAvailableMb);
#endif
  }

  if (java_switches.contains(kDisableGpuMemoryBufferCompositorResources)) {
    cmd_line->AppendSwitch(
        ::switches::kDisableGpuMemoryBufferCompositorResources);
  }

  std::string limit =
      GetSanitizedNumericValue(java_switches, kGpuImageCacheLimitItems);
  if (!limit.empty()) {
    cmd_line->AppendSwitchASCII(::switches::kCCImageCacheLimitItems, limit);
  }

  std::string decode_limit =
      GetSanitizedNumericValue(java_switches, kLimitImageDecodeCacheSizeMb);
  if (!decode_limit.empty()) {
    cmd_line->AppendSwitchASCII(::switches::kCCImageCacheLimitMbs,
                                decode_limit);
  }

  std::string budget = GetSanitizedNumericValue(
      java_switches, kDecodedImageWorkingSetBudgetBytes);
  if (!budget.empty()) {
    cmd_line->AppendSwitchASCII(::switches::kDecodedImageWorkingSetBudgetBytes,
                                budget);
  }

  if (java_switches.contains(kEnableScalingClippedImages)) {
    cmd_line->AppendSwitch(::switches::kEnableClippedImageScaling);
  }

  std::vector<std::string> enabled_features;
  std::vector<std::string> disabled_features;

  std::vector<std::string> mojo_pipe_params;
  std::string subresource_size = GetSanitizedNumericValue(
      java_switches, kCobaltDynamicMojoPipeSubresourceSize);
  if (!subresource_size.empty()) {
    mojo_pipe_params.push_back(base::StrCat(
        {network::features::kCobaltDynamicMojoPipeSizingSubresourceSize.name,
         "/", subresource_size}));
  }

  std::string media_size =
      GetSanitizedNumericValue(java_switches, kCobaltDynamicMojoPipeMediaSize);
  if (!media_size.empty()) {
    mojo_pipe_params.push_back(base::StrCat(
        {network::features::kCobaltDynamicMojoPipeSizingMediaSize.name, "/",
         media_size}));
  }

  if (java_switches.contains(kEnableCobaltDynamicMojoPipeSizing) ||
      !mojo_pipe_params.empty()) {
    if (!mojo_pipe_params.empty()) {
      enabled_features.push_back(
          base::StrCat({network::features::kCobaltDynamicMojoPipeSizing.name,
                        ":", base::JoinString(mojo_pipe_params, "/")}));
    } else {
      enabled_features.emplace_back(
          network::features::kCobaltDynamicMojoPipeSizing.name);
    }
  }

  if (java_switches.contains(kEnableCobaltContentLengthAwareMojoPipeSizing)) {
    enabled_features.emplace_back(
        network::features::kCobaltContentLengthAwareMojoPipeSizing.name);
  }

  std::vector<std::string> interest_area_params;
  std::string interest_area_size =
      GetSanitizedNumericValue(java_switches, kInterestAreaSizeInPixels);
  if (!interest_area_size.empty()) {
    interest_area_params.push_back(base::StrCat(
        {::features::kInterestAreaSizeInPixels.name, "/", interest_area_size}));
  }

  std::string reclaim_delay =
      GetSanitizedNumericValue(java_switches, kReclaimDelayInSeconds);
  if (!reclaim_delay.empty()) {
    interest_area_params.push_back(base::StrCat(
        {::features::kReclaimDelayInSeconds.name, "/", reclaim_delay}));
  }

  if (!interest_area_params.empty()) {
    enabled_features.push_back(
        base::StrCat({::features::kSmallerInterestArea.name, ":",
                      base::JoinString(interest_area_params, "/")}));
  }

  if (java_switches.contains(kAvoidCCReuseResource)) {
    cmd_line->AppendSwitch(::switches::kAvoidCCReuseResource);
  }

  if (java_switches.contains(kCobaltBypassResourceLoadScheduler)) {
    enabled_features.emplace_back(
        blink::features::kCobaltBypassResourceLoadScheduler.name);
  }

  if (java_switches.contains(kCobaltBypassHTMLPreloadScanner)) {
    enabled_features.emplace_back(
        blink::features::kCobaltBypassHTMLPreloadScanner.name);
  }

  if (java_switches.contains(kEnableCobaltMmapFontCache)) {
    // SkWoff2FontCache_cobalt.cc is only compiled in hermetic builds.
    enabled_features.emplace_back("CobaltMmapFontCache");
  }

  if (java_switches.contains(kAreaBasedVideoBufferBudget)) {
    enabled_features.emplace_back(features::kAreaBasedVideoBufferBudget.name);
  }

  if (java_switches.contains(
          kAllowCriticalMemoryPressureHandlingInForeground)) {
    cmd_line->AppendSwitch(
        ::switches::kAllowCriticalMemoryPressureHandlingInForeground);
  }

  if (java_switches.contains(kEvictMemoryCacheOnCriticalMemoryPressure)) {
    enabled_features.emplace_back(
        blink::features::kEvictMemoryCacheOnCriticalMemoryPressure.name);
  }

  if (java_switches.contains(kDisableLessAggressiveParkableString)) {
    disabled_features.emplace_back(
        blink::features::kLessAggressiveParkableString.name);
  }

  if (java_switches.contains(kEnableModerateMemoryPressure)) {
    enabled_features.emplace_back(
        base::features::kCobaltEnableModerateMemoryPressure.name);
  }

  std::string cooldown =
      GetSanitizedNumericValue(java_switches, kMemoryPressureCooldownInSeconds);
  if (!cooldown.empty()) {
    enabled_features.push_back(
        base::StrCat({base::features::kCobaltMemoryPressureCooldown.name, ":",
                      base::features::kCobaltMemoryPressureCooldownSeconds.name,
                      "/", cooldown}));
  }

  // Prepend JavaSwitches features ahead of any default features already on
  // `cmd_line` (e.g. SmallerInterestArea from CommandLineOverrideHelper) so
  // parameterized JavaSwitches overrides take precedence in FeatureList's
  // first-wins registration.
  PrependCommaSeparatedSwitch(cmd_line, ::switches::kEnableFeatures,
                              enabled_features);
  PrependCommaSeparatedSwitch(cmd_line, ::switches::kDisableFeatures,
                              disabled_features);

  if (java_switches.contains(kUseStarboardLifecycle)) {
    cmd_line->AppendSwitch(kUseStarboardLifecycleSwitch);
  }

  if (java_switches.contains(kForce720pUiOn1GbDevices)) {
    cmd_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor,
                                kForce720pDeviceScaleFactor);
  } else if (!cmd_line->HasSwitch(::switches::kForceDeviceScaleFactor)) {
    cmd_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor,
                                kDefaultForceDeviceScaleFactor);
  }
}

}  // namespace cobalt
