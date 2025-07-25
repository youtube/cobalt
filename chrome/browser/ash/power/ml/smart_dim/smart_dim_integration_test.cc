// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/power/ml/user_activity_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace ash {
namespace {

// Waits for a Lacros window to open and become visible.
class LacrosWindowWaiter : public aura::EnvObserver,
                           public aura::WindowObserver {
 public:
  LacrosWindowWaiter() { aura::Env::GetInstance()->AddObserver(this); }

  ~LacrosWindowWaiter() override {
    aura::Env::GetInstance()->RemoveObserver(this);
    if (lacros_window_) {
      lacros_window_->RemoveObserver(this);
      lacros_window_ = nullptr;
    }
  }

  void Wait() { run_loop_.Run(); }

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {
    if (crosapi::browser_util::IsLacrosWindow(window)) {
      lacros_window_ = window;
      // Observe for the window becoming visible.
      lacros_window_->AddObserver(this);
    }
  }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (window == lacros_window_ && visible) {
      run_loop_.Quit();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    CHECK_EQ(window, lacros_window_);
    lacros_window_->RemoveObserver(this);
    lacros_window_ = nullptr;
  }

  raw_ptr<aura::Window> lacros_window_ = nullptr;
  base::RunLoop run_loop_;
};

class SmartDimIntegrationTest : public InteractiveAshTest {
 public:
  SmartDimIntegrationTest() {
    feature_list_.InitAndEnableFeature(features::kSmartDim);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SmartDimIntegrationTest, SmartDim) {
  base::HistogramTester histograms;

  // UserActivityController has the underlying implementation of the
  // MlDecisionProvider D-Bus API used by powerd to make screen dim decisions.
  // Request a screen dimming decision and wait for the ML service to return an
  // answer.
  base::RunLoop run_loop;
  power::ml::UserActivityController::Get()->ShouldDeferScreenDim(
      base::BindLambdaForTesting([&](bool defer) { run_loop.Quit(); }));
  run_loop.Run();

  // WorkerType 0 is built-in worker. This is emitted before chrome queries the
  // ML service.
  histograms.ExpectBucketCount("PowerML.SmartDimComponent.WorkerType", 0, 1);

  // Bucket 0 is ash. This is emitted before chrome queries the ML service.
  histograms.ExpectBucketCount("PowerML.SmartDimFeature.WebPageInfoSource", 0,
                               1);

  // Bucket 0 is success. This is emitted after the ML Service replies
  // to chrome.
  histograms.ExpectBucketCount("PowerML.SmartDimModel.Result", 0, 1);
}

class SmartDimLacrosIntegrationTest : public SmartDimIntegrationTest {
 public:
  SmartDimLacrosIntegrationTest() {
    feature_list_.InitAndEnableFeature(
        ash::standalone_browser::features::kLacrosOnly);
  }

  // InteractiveAshTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SmartDimIntegrationTest::SetUpCommandLine(command_line);
    SetUpCommandLineForLacros(command_line);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SmartDimLacrosIntegrationTest, SmartDim) {
  ASSERT_TRUE(crosapi::browser_util::IsLacrosEnabled());

  // The test opens a Lacros window, so ensure the Wayland server is running and
  // crosapi is ready.
  WaitForAshFullyStarted();
  ASSERT_TRUE(crosapi::CrosapiManager::Get());
  ASSERT_TRUE(crosapi::CrosapiManager::Get()->crosapi_ash());

  // Request a Lacros window to open and wait for it to become visible.
  LacrosWindowWaiter waiter;
  crosapi::BrowserManager::Get()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/false);
  waiter.Wait();
  ASSERT_TRUE(crosapi::BrowserManager::Get()->IsRunning());

  base::HistogramTester histograms;

  // Request a screen dimming decision and wait for the ML service to return an
  // answer.
  base::RunLoop run_loop;
  power::ml::UserActivityController::Get()->ShouldDeferScreenDim(
      base::BindLambdaForTesting([&](bool defer) { run_loop.Quit(); }));
  run_loop.Run();

  // WorkerType 0 is built-in worker.
  histograms.ExpectBucketCount("PowerML.SmartDimComponent.WorkerType", 0, 1);
  // Bucket 1 is Lacros.
  histograms.ExpectBucketCount("PowerML.SmartDimFeature.WebPageInfoSource", 1,
                               1);
  // Bucket 0 is success.
  histograms.ExpectBucketCount("PowerML.SmartDimModel.Result", 0, 1);
}

}  // namespace
}  // namespace ash
