// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/windows_services/service_program/process_wrl_module.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TracingServiceTestSuite : public base::TestSuite {
 public:
  TracingServiceTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {
    ExtendTestTimeouts();
  }

 protected:
  void Initialize() override {
    // Tests may use mojo.
    mojo::core::Init(mojo::core::Configuration{.is_broker_process = true});
    test_io_thread_.Start();
    ipc_support_.emplace(test_io_thread_.task_runner(),
                         mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

    // Tests may need a WRL module.
    CreateWrlModule();

    base::TestSuite::Initialize();
  }

 private:
  // Tests run in this binary may need 120s in order to reach a platform timeout
  // for COM calls. Add switches to the process command line to extend the
  // values that would cause tests to be terminated prematurely.
  static void ExtendTestTimeouts() {
    auto& command_line = *base::CommandLine::ForCurrentProcess();
    constexpr auto kTimeoutMs = base::Seconds(130).InMilliseconds();
    const auto timeout_string = base::NumberToString(kTimeoutMs);

    // Controls how long the test launcher will allow a child process to run.
    if (!command_line.HasSwitch(switches::kTestLauncherTimeout)) {
      command_line.AppendSwitchASCII(switches::kTestLauncherTimeout,
                                     timeout_string);
    }

    // Controls how long a RunLoop may run tasks in TaskEnvironment.
    if (!command_line.HasSwitch(switches::kUiTestActionTimeout)) {
      command_line.AppendSwitchASCII(switches::kUiTestActionTimeout,
                                     timeout_string);
    }

    // Controls how long TaskEnvironment will allow a single task to run.
    if (!command_line.HasSwitch(switches::kUiTestActionMaxTimeout)) {
      command_line.AppendSwitchASCII(switches::kUiTestActionMaxTimeout,
                                     timeout_string);
    }
  }

  install_static::ScopedInstallDetails install_details_;
  base::TestIOThread test_io_thread_{base::TestIOThread::kManualStart};
  std::optional<mojo::core::ScopedIPCSupport> ipc_support_;
};

}  // namespace

int main(int argc, char** argv) {
  base::PlatformThread::SetName("MainThread");

  TracingServiceTestSuite test_suite(argc, argv);

  // Some tests in this binary mutate system-wide state (e.g., to install a
  // Windows service), so run all tests serially to avoid races between tests.
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
