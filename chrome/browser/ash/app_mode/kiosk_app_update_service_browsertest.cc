// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_update_service.h"

#include <memory>
#include <string>

#include "ash/constants/ash_paths.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/system/automatic_reboot_manager.h"
#include "chrome/browser/ash/system/automatic_reboot_manager_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TestFuture;

namespace ash {

namespace {

// Maximum time for AutomaticRebootManager initialization to complete.
constexpr base::TimeDelta kAutomaticRebootManagerInitTimeout =
    base::Seconds(60);

// Blocks until `manager` is initialized and then sets `success_out` to true and
// runs `quit_closure`. If initialization does not occur within `timeout`, sets
// `success_out` to false and runs `quit_closure`.
void WaitForAutomaticRebootManagerInit(system::AutomaticRebootManager* manager,
                                       const base::TimeDelta& timeout,
                                       base::OnceClosure quit_closure,
                                       bool* success_out) {
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
  *success_out = manager->WaitForInitForTesting(timeout);
  std::move(quit_closure).Run();
}

}  // namespace

class KioskAppUpdateServiceTest
    : public extensions::PlatformAppBrowserTest,
      public system::AutomaticRebootManagerObserver {
 public:
  KioskAppUpdateServiceTest() = default;
  KioskAppUpdateServiceTest(const KioskAppUpdateServiceTest&) = delete;
  KioskAppUpdateServiceTest& operator=(const KioskAppUpdateServiceTest&) =
      delete;
  ~KioskAppUpdateServiceTest() override = default;

  // extensions::PlatformAppBrowserTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    extensions::PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    const base::FilePath& temp_dir = temp_dir_.GetPath();

    const base::TimeDelta uptime = base::Hours(3);
    const std::string uptime_seconds =
        base::NumberToString(uptime.InSecondsF());
    const base::FilePath uptime_file = temp_dir.Append("uptime");
    ASSERT_TRUE(base::WriteFile(uptime_file, uptime_seconds));
    uptime_file_override_ = std::make_unique<base::ScopedPathOverride>(
        FILE_UPTIME, uptime_file, /*is_absolute=*/false, /*create=*/false);
  }

  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    app_ = LoadExtension(
        test_data_dir_.AppendASCII("api_test/runtime/on_restart_required"));

    // Fake app mode command line.
    base::CommandLine* command = base::CommandLine::ForCurrentProcess();
    command->AppendSwitch(switches::kForceAppMode);
    command->AppendSwitchASCII(switches::kAppId, app_->id());

    automatic_reboot_manager_ =
        g_browser_process->platform_part()->automatic_reboot_manager();

    // Wait for `automatic_reboot_manager_` to finish initializing.
    bool initialized = false;
    base::RunLoop run_loop;
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(&WaitForAutomaticRebootManagerInit,
                       base::Unretained(automatic_reboot_manager_),
                       kAutomaticRebootManagerInitTimeout,
                       run_loop.QuitClosure(), base::Unretained(&initialized)));
    run_loop.Run();
    ASSERT_TRUE(initialized)
        << "AutomaticRebootManager not initialized in "
        << kAutomaticRebootManagerInitTimeout.InSeconds() << " seconds";

    automatic_reboot_manager_->AddObserver(this);
  }

  // system::AutomaticRebootManagerObserver:
  void OnRebootRequested(
      system::AutomaticRebootManagerObserver::Reason) override {
    if (test_waiter_) {
      test_waiter_->SetValue();
    }
  }

  void WillDestroyAutomaticRebootManager() override {
    automatic_reboot_manager_->RemoveObserver(this);
  }

  void CreateKioskAppUpdateService() {
    EXPECT_FALSE(update_service_);
    update_service_ = KioskAppUpdateServiceFactory::GetForProfile(profile());
    update_service_->Init(app_->id());
  }

  void FireAppUpdateAvailable() { update_service_->OnAppUpdateAvailable(app_); }

  void FireUpdatedNeedReboot() {
    update_engine::StatusResult status;
    status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
    test_waiter_ = std::make_unique<TestFuture<void>>();
    automatic_reboot_manager_->UpdateStatusChanged(status);
    EXPECT_TRUE(test_waiter_->Wait());
  }

  void RequestPeriodicReboot() {
    test_waiter_ = std::make_unique<TestFuture<void>>();
    g_browser_process->local_state()->SetInteger(prefs::kUptimeLimit,
                                                 base::Hours(2).InSeconds());
    EXPECT_TRUE(test_waiter_->Wait());
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> uptime_file_override_;
  raw_ptr<const extensions::Extension, ExperimentalAsh> app_ =
      nullptr;  // Not owned.
  raw_ptr<KioskAppUpdateService, DanglingUntriaged | ExperimentalAsh>
      update_service_ = nullptr;  // Not owned.
  raw_ptr<system::AutomaticRebootManager, DanglingUntriaged | ExperimentalAsh>
      automatic_reboot_manager_ = nullptr;  // Not owned.
  std::unique_ptr<TestFuture<void>> test_waiter_;
};

// Verifies that the app is notified a reboot is required when an app update
// becomes available.
IN_PROC_BROWSER_TEST_F(KioskAppUpdateServiceTest, AppUpdate) {
  base::HistogramTester histogram;

  CreateKioskAppUpdateService();

  ExtensionTestMessageListener listener("app_update");
  FireAppUpdateAvailable();
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  histogram.ExpectUniqueSample(kKioskPrimaryAppInSessionUpdateHistogram,
                               /*sample=*/1,
                               /*expected_bucket_count=*/1);
}

// Verifies that the app is notified a reboot is required when an OS update is
// applied while Chrome is running and the policy to reboot after update is
// enabled.
IN_PROC_BROWSER_TEST_F(KioskAppUpdateServiceTest, OsUpdate) {
  CreateKioskAppUpdateService();

  g_browser_process->local_state()->SetBoolean(prefs::kRebootAfterUpdate, true);
  ExtensionTestMessageListener listener("os_update");
  FireUpdatedNeedReboot();
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Verifies that the app is notified a reboot is required when a periodic reboot
// is requested while Chrome is running.
IN_PROC_BROWSER_TEST_F(KioskAppUpdateServiceTest, Periodic) {
  CreateKioskAppUpdateService();

  ExtensionTestMessageListener listener("periodic");
  RequestPeriodicReboot();
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Verifies that the app is notified a reboot is required when an OS update was
// applied before Chrome was started and the policy to reboot after update is
// enabled.
IN_PROC_BROWSER_TEST_F(KioskAppUpdateServiceTest, StartAfterOsUpdate) {
  g_browser_process->local_state()->SetBoolean(prefs::kRebootAfterUpdate, true);
  FireUpdatedNeedReboot();

  ExtensionTestMessageListener listener("os_update");
  CreateKioskAppUpdateService();
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Verifies that the app is notified a reboot is required when a periodic reboot
// was requested before Chrome was started.
IN_PROC_BROWSER_TEST_F(KioskAppUpdateServiceTest, StartAfterPeriodic) {
  RequestPeriodicReboot();

  ExtensionTestMessageListener listener("periodic");
  CreateKioskAppUpdateService();
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

}  // namespace ash
