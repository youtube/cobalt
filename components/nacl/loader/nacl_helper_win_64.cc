// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_helper_win_64.h"

#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/timer/hi_res_timer_manager.h"
#include "base/win/process_startup_helper.h"
#include "components/nacl/broker/nacl_broker_listener.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/loader/nacl_listener.h"
#include "components/nacl/loader/nacl_main_platform_delegate.h"
#include "content/public/app/sandbox_helper_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "mojo/core/embedder/embedder.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/win/src/sandbox_types.h"

extern int NaClMain(content::MainFunctionParams);

namespace {
// main() routine for the NaCl broker process.
// This is necessary for supporting NaCl in Chrome on Win64.
int NaClBrokerMain(content::MainFunctionParams parameters) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::PlatformThread::SetName("CrNaClBrokerMain");

  mojo::core::InitFeatures();
  mojo::core::Init();

  base::PowerMonitor::Initialize(
      std::make_unique<base::PowerMonitorDeviceSource>());
  base::HighResolutionTimerManager hi_res_timer_manager;

  NaClBrokerListener listener;
  listener.Listen();

  return 0;
}

}  // namespace

namespace nacl {

int NaClWin64Main() {
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  content::InitializeSandboxInfo(&sandbox_info);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  base::FieldTrialList field_trial_list;
  base::FieldTrialList::CreateTrialsFromCommandLine(*command_line,
                                                    /*unused_fd_key=*/0);

  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrialList::CreateFeaturesFromCommandLine(*command_line,
                                                      feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  // Copy what ContentMain() does.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(*command_line);
  // Route stdio to parent console (if any) or create one.
  if (command_line->HasSwitch(switches::kEnableLogging))
    base::RouteStdioToConsole(true);

  // Initialize the sandbox for this process.
  bool sandbox_initialized_ok = sandbox::policy::Sandbox::Initialize(
      sandbox::policy::SandboxTypeFromCommandLine(*command_line),
      &sandbox_info);

  // Die if the sandbox can't be enabled.
  CHECK(sandbox_initialized_ok) << "Error initializing sandbox for "
                                << process_type;
  content::MainFunctionParams main_params(command_line);
  main_params.sandbox_info = &sandbox_info;

  if (process_type == switches::kNaClLoaderProcess)
    return NaClMain(std::move(main_params));

  if (process_type == switches::kNaClBrokerProcess)
    return NaClBrokerMain(std::move(main_params));

  CHECK(false) << "Unknown NaCl 64 process.";
  return -1;
}

}  // namespace nacl
