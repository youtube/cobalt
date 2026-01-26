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

#include "cobalt/app/cobalt_main_delegate.h"

#include "base/process/current_process.h"
#include "base/threading/hang_watcher.h"
#include "base/trace_event/trace_log.h"
#include "cobalt/browser/cobalt_content_browser_client.h"
#include "cobalt/gpu/cobalt_content_gpu_client.h"
#include "cobalt/renderer/cobalt_content_renderer_client.h"
#include "components/memory_system/initializer.h"
#include "components/memory_system/parameters.h"
#include "content/common/content_constants_internal.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "cobalt/app/cobalt_crash_reporter_client.h"
#include "components/crash/core/app/crashpad.h"
#endif

namespace cobalt {

CobaltMainDelegate::CobaltMainDelegate() : content::ShellMainDelegate() {}

CobaltMainDelegate::~CobaltMainDelegate() {}

std::optional<int> CobaltMainDelegate::BasicStartupComplete() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableAggressiveDOMStorageFlushing);
  cl->AppendSwitch(switches::kDisableGpuShaderDiskCache);
  return content::ShellMainDelegate::BasicStartupComplete();
}

content::ContentBrowserClient*
CobaltMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<CobaltContentBrowserClient>();
  return browser_client_.get();
}

content::ContentGpuClient* CobaltMainDelegate::CreateContentGpuClient() {
  gpu_client_ = std::make_unique<CobaltContentGpuClient>();
  return gpu_client_.get();
}

content::ContentRendererClient*
CobaltMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<CobaltContentRendererClient>();
  return renderer_client_.get();
}

content::ContentUtilityClient*
CobaltMainDelegate::CreateContentUtilityClient() {
  utility_client_ = std::make_unique<CobaltContentUtilityClient>();
  return utility_client_.get();
}

std::optional<int> CobaltMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  content::RenderFrameHost::AllowInjectingJavaScript();

  if (!ShouldCreateFeatureList(invoked_in)) {
    // Apply field trial testing configuration since content did not.
    browser_client_->CreateFeatureListAndFieldTrials();
  }
  if (!ShouldInitializeMojo(invoked_in)) {
    content::InitializeMojoCore();
  }

  InitializeHangWatcher();

  const std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  // ShellMainDelegate has GWP-ASan as well as Profiling Client disabled.
  // Consequently, we provide no parameters for these two. The memory_system
  // includes the PoissonAllocationSampler dynamically only if the Profiling
  // Client is enabled. However, we are not sure if this is the only user of
  // PoissonAllocationSampler in the ContentShell. Therefore, enforce inclusion
  // at the moment.
  //
  // TODO(https://crbug.com/1411454): Clarify which users of
  // PoissonAllocationSampler we have in the ContentShell. Do we really need to
  // enforce it?
  memory_system::Initializer()
      .SetDispatcherParameters(memory_system::DispatcherParameters::
                                   PoissonAllocationSamplerInclusion::kEnforce,
                               memory_system::DispatcherParameters::
                                   AllocationTraceRecorderInclusion::kIgnore,
                               process_type)
      .Initialize(memory_system_);

  return std::nullopt;
}

std::variant<int, content::MainFunctionParams> CobaltMainDelegate::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  // For non-browser process, return and have the caller run the main loop.
  if (!process_type.empty()) {
    return std::move(main_function_params);
  }

  base::CurrentProcess::GetInstance().SetProcessType(
      base::CurrentProcessType::PROCESS_BROWSER);

  main_runner_ = content::BrowserMainRunner::Create();

  // In browser tests, the |main_function_params| contains a |ui_task| which
  // will execute the testing. The task will be executed synchronously inside
  // Initialize() so we don't depend on the BrowserMainRunner being Run().
  int initialize_exit_code =
      main_runner_->Initialize(std::move(main_function_params));
  DCHECK_LT(initialize_exit_code, 0)
      << "BrowserMainRunner::Initialize failed in ShellMainDelegate";

  // Return 0 as BrowserMain() should not be called after this, bounce up to
  // the system message loop for ContentShell, and we're already done thanks
  // to the |ui_task| for browser tests.
  return 0;
}

#if BUILDFLAG(IS_ANDROIDTV)
void CobaltMainDelegate::PreSandboxStartup() {
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  CobaltCrashReporterClient::Create();
  crash_reporter::InitializeCrashpad(process_type.empty(), process_type);

  // For now we just want a minidump persisted in the on-device crash database.
  crash_reporter::SetUploadConsent(false);

  return content::ShellMainDelegate::PreSandboxStartup();
}
#endif

void CobaltMainDelegate::Shutdown() {
  main_runner_->Shutdown();
}

void CobaltMainDelegate::InitializeHangWatcher() {
  const base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  // In single-process mode it's always kBrowserProcess.
  base::HangWatcher::ProcessType hang_watcher_process_type;
  if (process_type.empty()) {
    hang_watcher_process_type = base::HangWatcher::ProcessType::kBrowserProcess;
  } else if (process_type == switches::kGpuProcess) {
    hang_watcher_process_type = base::HangWatcher::ProcessType::kGPUProcess;
  } else if (process_type == switches::kRendererProcess) {
    hang_watcher_process_type =
        base::HangWatcher::ProcessType::kRendererProcess;
  } else if (process_type == switches::kUtilityProcess) {
    hang_watcher_process_type = base::HangWatcher::ProcessType::kUtilityProcess;
  } else {
    hang_watcher_process_type = base::HangWatcher::ProcessType::kUnknownProcess;
  }
  const bool emit_crashes = false;

  base::HangWatcher::InitializeOnMainThread(hang_watcher_process_type,
                                            emit_crashes);
}
}  // namespace cobalt
