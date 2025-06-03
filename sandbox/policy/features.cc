// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "sandbox/features.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace sandbox::policy::features {

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
// Enables network service sandbox.
// (Only causes an effect when feature kNetworkServiceInProcess is disabled.)
BASE_FEATURE(kNetworkServiceSandbox,
             "NetworkServiceSandbox",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Enables a fine-grained seccomp-BPF syscall filter for the network service.
// Only has an effect if IsNetworkSandboxEnabled() returns true.
// If the network service sandbox is enabled and |kNetworkServiceSyscallFilter|
// is disabled, a seccomp-BPF filter will still be applied but it will not
// disallow any syscalls.
BASE_FEATURE(kNetworkServiceSyscallFilter,
             "NetworkServiceSyscallFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables a fine-grained file path allowlist for the network service.
// Only has an effect if IsNetworkSandboxEnabled() returns true.
// If the network service sandbox is enabled and |kNetworkServiceFileAllowlist|
// is disabled, a file path allowlist will still be applied, but the policy will
// allow everything.
BASE_FEATURE(kNetworkServiceFileAllowlist,
             "NetworkServiceFileAllowlist",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
// Experiment for Windows sandbox security mitigation,
// sandbox::MITIGATION_EXTENSION_POINT_DISABLE.
BASE_FEATURE(kWinSboxDisableExtensionPoints,
             "WinSboxDisableExtensionPoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables GPU AppContainer sandbox on Windows.
BASE_FEATURE(kGpuAppContainer,
             "GpuAppContainer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables GPU Low Privilege AppContainer when combined with kGpuAppContainer.
BASE_FEATURE(kGpuLPAC,
             "GpuLPAC",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Renderer AppContainer
BASE_FEATURE(kRendererAppContainer,
             "RendererAppContainer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables very high job memory limits for sandboxed renderer processes. This
// sets a limit of 1Tb, effectively removing the Job memory limits, except in
// egregious cases.
BASE_FEATURE(kWinSboxHighRendererJobMemoryLimits,
             "WinSboxHighRendererJobMemoryLimits",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Emergency "off switch" for closing the KsecDD handle in cryptbase.dll just
// before sandbox lockdown in renderers.
BASE_FEATURE(kWinSboxRendererCloseKsecDD,
             "WinSboxRendererCloseKsecDD",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, only warm up `bcryptprimitives!ProcessPrng` - if disabled warms
// up `advapi32!RtlGenRandom`.
BASE_FEATURE(kWinSboxWarmupProcessPrng,
             "WinSboxWarmupProcessPrng",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, launch the network service within an LPAC sandbox. If disabled,
// the network service will run inside an App Container.
BASE_FEATURE(kWinSboxNetworkServiceSandboxIsLPAC,
             "WinSboxNetworkServiceSandboxIsLPAC",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, always launch the renderer process with Code Integrity Guard
// enabled, regardless of the local policy configuration. If disabled, then
// policy is respected. This acts as an emergency "off switch" for the
// deprecation of the RendererCodeIntegrityEnabled policy.
BASE_FEATURE(kWinSboxForceRendererCodeIntegrity,
             "WinSboxForceRendererCodeIntegrity",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, modifies the child's PEB to stop further application of
// appcompat in the child. Does not affect the browser or unsandboxed
// processes.
BASE_FEATURE(kWinSboxZeroAppShim,
             "WinSboxZeroAppShim",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, applies the FSCTL syscall lockdown mitigation to all sandboxed
// processes, if supported by the OS.
BASE_FEATURE(kWinSboxFsctlLockdown,
             "WinSboxFsctlLockdown",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Controls whether the Spectre variant 2 mitigation is enabled. We use a USE
// flag on some Chrome OS boards to disable the mitigation by disabling this
// feature in exchange for system performance.
BASE_FEATURE(kSpectreVariant2Mitigation,
             "SpectreVariant2Mitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// An override for the Spectre variant 2 default behavior. Security sensitive
// users can enable this feature to ensure that the mitigation is always
// enabled.
BASE_FEATURE(kForceSpectreVariant2Mitigation,
             "ForceSpectreVariant2Mitigation",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Enabling the kNetworkServiceSandbox feature automatically enables Spectre
// variant 2 mitigations in the network service. This can lead to performance
// regressions, so enabling this feature will turn off the Spectre Variant 2
// mitigations.
//
// On ChromeOS Ash, this overrides the system-wide kSpectreVariant2Mitigation
// feature above, but not the user-controlled kForceSpectreVariant2Mitigation
// feature.
BASE_FEATURE(kForceDisableSpectreVariant2MitigationInNetworkService,
             "kForceDisableSpectreVariant2MitigationInNetworkService",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
// Enables caching compiled sandbox profiles. Only some profiles support this,
// as controlled by CanCacheSandboxPolicy().
BASE_FEATURE(kCacheMacSandboxProfiles,
             "CacheMacSandboxProfiles",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
// Enables the renderer on Android to use a separate seccomp policy.
BASE_FEATURE(kUseRendererProcessPolicy,
             "UseRendererProcessPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);
// When enabled, this features restricts a set of syscalls in
// BaselinePolicyAndroid that are used by RendererProcessPolicy.
BASE_FEATURE(kRestrictRendererPoliciesInBaseline,
             "RestrictRendererPoliciesInBaseline",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
bool IsNetworkSandboxSupported() {
  // Network service sandbox uses GetNetworkConnectivityHint which is only
  // supported on Windows 10 Build 19041 (20H1) so versions before that wouldn't
  // have a working network change notifier when running in the sandbox.
  // TODO(crbug.com/1450754): Move this to an API that works earlier than 20H1
  // and also works in the LPAC sandbox.
  static const bool supported =
      base::win::GetVersion() >= base::win::Version::WIN10_20H1;
  if (!supported) {
    return false;
  }

  // App container must be already supported on 20H1, but double check it here.
  CHECK(sandbox::features::IsAppContainerSandboxSupported());

  return true;
}
#endif  // BUILDFLAG(IS_WIN)

bool IsNetworkSandboxEnabled() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
  return true;
#else
#if BUILDFLAG(IS_WIN)
  if (!IsNetworkSandboxSupported()) {
    return false;
  }
#endif  // BUILDFLAG(IS_WIN)
  // Check feature status.
  return base::FeatureList::IsEnabled(kNetworkServiceSandbox);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
}

}  // namespace sandbox::policy::features
