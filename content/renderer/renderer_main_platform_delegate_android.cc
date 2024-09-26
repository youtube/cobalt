// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include "base/android/build_info.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/seccomp_sandbox_status_android.h"
#include "sandbox/linux/seccomp-bpf-helpers/seccomp_starter_android.h"
#include "sandbox/sandbox_buildflags.h"

#if BUILDFLAG(USE_SECCOMP_BPF)
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"
#endif

namespace content {

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters) {}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  TRACE_EVENT0("startup", "RendererMainPlatformDelegate::EnableSandbox");
  auto* info = base::android::BuildInfo::GetInstance();
  sandbox::SeccompStarterAndroid starter(info->sdk_int());
  // The policy compiler is only available if USE_SECCOMP_BPF is enabled.
#if BUILDFLAG(USE_SECCOMP_BPF)
  sandbox::BaselinePolicyAndroid::RuntimeOptions options(
      starter.GetDefaultBaselineOptions());
  starter.set_policy(std::make_unique<sandbox::BaselinePolicyAndroid>(options));
#endif
  starter.StartSandbox();

  SetSeccompSandboxStatus(starter.status());
  UMA_HISTOGRAM_ENUMERATION("Android.SeccompStatus.RendererSandbox",
                            starter.status(),
                            sandbox::SeccompSandboxStatus::STATUS_MAX);

  return true;
}

}  // namespace content
