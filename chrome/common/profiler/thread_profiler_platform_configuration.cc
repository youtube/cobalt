// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "build/build_config.h"
#include "chrome/common/profiler/process_type.h"

namespace {

// The default configuration to use in the absence of special circumstances on a
// specific platform.
class DefaultPlatformConfiguration
    : public ThreadProfilerPlatformConfiguration {
 public:
  explicit DefaultPlatformConfiguration(bool browser_test_mode_enabled);

  RelativePopulations GetEnableRates(
      absl::optional<version_info::Channel> release_channel) const override;

  double GetChildProcessEnableFraction(
      metrics::CallStackProfileParams::Process process) const override;

  bool IsEnabledForThread(
      metrics::CallStackProfileParams::Process process,
      metrics::CallStackProfileParams::Thread thread) const override;

 protected:
  bool IsSupportedForChannel(
      absl::optional<version_info::Channel> release_channel) const override;

  bool browser_test_mode_enabled() const { return browser_test_mode_enabled_; }

 private:
  const bool browser_test_mode_enabled_;
};

DefaultPlatformConfiguration::DefaultPlatformConfiguration(
    bool browser_test_mode_enabled)
    : browser_test_mode_enabled_(browser_test_mode_enabled) {}

ThreadProfilerPlatformConfiguration::RelativePopulations
DefaultPlatformConfiguration::GetEnableRates(
    absl::optional<version_info::Channel> release_channel) const {
  CHECK(IsSupportedForChannel(release_channel));

  if (!release_channel) {
    // This is a local/CQ build.
    return RelativePopulations{100, 0};
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (browser_test_mode_enabled()) {
    // This is a browser test or maybe a tast test that called
    // chrome.EnableStackSampledMetrics().
    return RelativePopulations{100, 0};
  }
#endif

  CHECK(*release_channel == version_info::Channel::CANARY ||
        *release_channel == version_info::Channel::DEV);

  return RelativePopulations{80, 20};
}

double DefaultPlatformConfiguration::GetChildProcessEnableFraction(
    metrics::CallStackProfileParams::Process process) const {
  DCHECK_NE(metrics::CallStackProfileParams::Process::kBrowser, process);

  // Profile all supported processes in browser test mode.
  if (browser_test_mode_enabled()) {
    return 1.0;
  }

  switch (process) {
    case metrics::CallStackProfileParams::Process::kGpu:
    case metrics::CallStackProfileParams::Process::kNetworkService:
      return 1.0;

    case metrics::CallStackProfileParams::Process::kRenderer:
      // Run the profiler in 20% of the processes to collect roughly as many
      // profiles for renderer processes as browser processes.
      return 0.2;

    default:
      return 0.0;
  }
}

bool DefaultPlatformConfiguration::IsEnabledForThread(
    metrics::CallStackProfileParams::Process process,
    metrics::CallStackProfileParams::Thread thread) const {
  // Enable for all supported threads.
  return true;
}

bool DefaultPlatformConfiguration::IsSupportedForChannel(
    absl::optional<version_info::Channel> release_channel) const {
  // The profiler is always supported for local builds and the CQ.
  if (!release_channel)
    return true;

#if BUILDFLAG(IS_CHROMEOS)
  if (browser_test_mode_enabled()) {
    // This is a browser test or maybe a tast test that called
    // chrome.EnableStackSampledMetrics().
    return true;
  }
#endif

  // Canary and dev are the only channels currently supported in release
  // builds.
  return *release_channel == version_info::Channel::CANARY ||
         *release_channel == version_info::Channel::DEV;
}

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
// The configuration to use for the Android platform. Applies to ARM32 which is
// the only Android architecture currently supported by StackSamplingProfiler.
// Defined in terms of DefaultPlatformConfiguration where Android does not
// differ from the default case.
class AndroidPlatformConfiguration : public DefaultPlatformConfiguration {
 public:
  explicit AndroidPlatformConfiguration(bool browser_test_mode_enabled);

  RelativePopulations GetEnableRates(
      absl::optional<version_info::Channel> release_channel) const override;

  double GetChildProcessEnableFraction(
      metrics::CallStackProfileParams::Process process) const override;
};

AndroidPlatformConfiguration::AndroidPlatformConfiguration(
    bool browser_test_mode_enabled)
    : DefaultPlatformConfiguration(browser_test_mode_enabled) {}

ThreadProfilerPlatformConfiguration::RelativePopulations
AndroidPlatformConfiguration::GetEnableRates(
    absl::optional<version_info::Channel> release_channel) const {
  // Always enable profiling in local/CQ builds or browser test mode.
  if (!release_channel.has_value() || browser_test_mode_enabled()) {
    return RelativePopulations{100, 0};
  }

  DCHECK(*release_channel == version_info::Channel::CANARY ||
         *release_channel == version_info::Channel::DEV);
  // TODO(crbug.com/1430519): Change the relative population to {80, 20} after
  // the DAU shift has been mitigated.
  // Set all the population in experiment group. Because we have 3 experiment
  // groups, set population of experiment group to 99, the largest integer
  // < 100 and divisible by 3.
  return RelativePopulations{1, 99, /* add_periodic_only_group=*/true};
}

double AndroidPlatformConfiguration::GetChildProcessEnableFraction(
    metrics::CallStackProfileParams::Process process) const {
  if (process == metrics::CallStackProfileParams::Process::kRenderer) {
    // There are empirically, on average, 1.3 renderer processes per browser
    // process. This samples the renderer process at roughly the same
    // frequency overall as the browser process.
    // http://uma/p/chrome/timeline_v2?sid=39bc30a43a01d045204d0add05ad120a
    return browser_test_mode_enabled() ? 1.0 : 0.75;
  }
  return DefaultPlatformConfiguration::GetChildProcessEnableFraction(process);
}
#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)

}  // namespace

// static
std::unique_ptr<ThreadProfilerPlatformConfiguration>
ThreadProfilerPlatformConfiguration::Create(bool browser_test_mode_enabled) {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
  using PlatformConfiguration = AndroidPlatformConfiguration;
#else
  using PlatformConfiguration = DefaultPlatformConfiguration;
#endif
  return std::make_unique<PlatformConfiguration>(browser_test_mode_enabled);
}

bool ThreadProfilerPlatformConfiguration::IsSupported(
    absl::optional<version_info::Channel> release_channel) const {
// `ThreadProfiler` is currently not supported on ARM64, even if
// `base::StackSamplingProfiler` may support it.
//
// TODO(crbug.com/1392158): Remove this conditional.
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64)
  return false;
#else
  return base::StackSamplingProfiler::IsSupportedForCurrentPlatform() &&
         IsSupportedForChannel(release_channel);
#endif
}
