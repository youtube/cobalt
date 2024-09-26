// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

namespace {

// Defines a report-level restriction type for screen captures.
const policy::DlpContentRestrictionSet kScreenCaptureReported{
    policy::DlpContentRestriction::kScreenshot,
    policy::DlpRulesManager::Level::kReport};
// Defines a warning-level restriction type for screen captures.
const policy::DlpContentRestrictionSet kScreenCaptureWarned{
    policy::DlpContentRestriction::kScreenshot,
    policy::DlpRulesManager::Level::kWarn};

constexpr char kSrcPattern[] = "example.com";
constexpr char kRuleName[] = "rule #1";
constexpr char kRuleId[] = "testid1";
const policy::DlpRulesManager::RuleMetadata kRuleMetadata(kRuleName, kRuleId);

// Returns the native window of the given `browser`.
aura::Window* GetBrowserWindow(Browser* browser) {
  return browser->window()->GetNativeWindow();
}

void SetupLoopToWaitForCaptureFileToBeSaved(base::RunLoop* loop) {
  ash::CaptureModeTestApi().SetOnCaptureFileSavedCallback(
      base::BindLambdaForTesting([loop](const base::FilePath& path) {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(base::PathExists(path));
        loop->Quit();
      }));
}

// Defines a waiter that waits for the DLP warning dialog to be added as a child
// of the system modal container window under the given `root`.
// A single instance of `DlpWarningDialogWaiter` can be used only once for
// waiting.
class DlpWarningDialogWaiter : public aura::WindowObserver {
 public:
  explicit DlpWarningDialogWaiter(aura::Window* root) {
    observation_.Observe(
        root->GetChildById(ash::kShellWindowId_SystemModalContainer));
    on_window_added_callback_ = loop_.QuitClosure();
  }
  DlpWarningDialogWaiter(const DlpWarningDialogWaiter&) = delete;
  DlpWarningDialogWaiter& operator=(const DlpWarningDialogWaiter&) = delete;
  ~DlpWarningDialogWaiter() override = default;

  void Wait() { loop_.Run(); }

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    if (on_window_added_callback_)
      std::move(on_window_added_callback_).Run();
  }

 private:
  base::ScopedObservation<aura::Window, aura::WindowObserver> observation_{
      this};
  base::RunLoop loop_;
  base::OnceClosure on_window_added_callback_;
};

// Starts a fullscreen video recording.
void StartVideoRecording() {
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  ASSERT_TRUE(test_api.IsSessionActive());
  test_api.PerformCapture();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());
}

// Marks the active web contents of the given `browser` as DLP restricted with a
// warning level.
void MarkActiveTabAsDlpWarnedForScreenCapture(Browser* browser) {
  auto* dlp_content_observer = policy::DlpContentObserver::Get();
  ASSERT_TRUE(dlp_content_observer);

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  dlp_content_observer->OnConfidentialityChanged(web_contents,
                                                 kScreenCaptureWarned);
}

// Waits for video record countdown to be finished.
void WaitForCountDownToFinish() {
  base::RunLoop run_loop;
  ash::CaptureModeTestApi().SetOnVideoRecordCountdownFinishedCallback(
      run_loop.QuitClosure());
  run_loop.Run();
}

// Stops the video recording and waits for the DLP warning dialog to be added.
void StopRecordingAndWaitForDlpWarningDialog(Browser* browser) {
  auto* root = GetBrowserWindow(browser)->GetRootWindow();
  ASSERT_TRUE(root);
  DlpWarningDialogWaiter waiter{root};
  ash::CaptureModeTestApi test_api;
  test_api.StopVideoRecording();
  waiter.Wait();
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
}

void SendKeyEvent(Browser* browser,
                  ui::KeyboardCode key_code,
                  int flags = ui::EF_NONE) {
  auto* browser_window = GetBrowserWindow(browser);
  ui::test::EventGenerator event_generator{browser_window->GetRootWindow(),
                                           browser_window};
  event_generator.PressAndReleaseKey(key_code, flags);
}

std::unique_ptr<KeyedService> SetDlpRulesManager(
    content::BrowserContext* context) {
  auto dlp_rules_manager =
      std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>();
  ON_CALL(*dlp_rules_manager, GetSourceUrlPattern)
      .WillByDefault(testing::DoAll(testing::SetArgPointee<3>(kRuleMetadata),
                                    testing::Return(kSrcPattern)));
  return dlp_rules_manager;
}

}  // namespace

// Testing class to test CrOS capture mode, which is a feature to take
// screenshots and record video.
class CaptureModeBrowserTest : public InProcessBrowserTest {
 public:
  CaptureModeBrowserTest() = default;
  ~CaptureModeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // Instantiate |DlpContentManagerTestHelper| after main thread has been
    // set up because |DlpReportingManager| needs a sequenced task runner handle
    // to set up the report queue.
    helper_ = std::make_unique<policy::DlpContentManagerTestHelper>();

    // TODO(https://crbug.com/1283065): Remove this.
    // Currently, setting the notifier explicitly is needed since otherwise, due
    // to a wrongly initialized notifier, calling the virtual
    // ShowDlpWarningDialog() method causes a crash.
    helper_->ResetWarnNotifierForTesting();

    SetupDlpReporting();
  }

  void TearDownOnMainThread() override { helper_.reset(); }

  void SetupDlpReporting() {
    SetupDlpRulesManager();
    // Set up mock report queue.
    SetReportQueueForReportingManager(
        helper_->GetReportingManager(), events_,
        base::SequencedTaskRunner::GetCurrentDefault());
  }

 protected:
  // Sets up mock rules manager.
  void SetupDlpRulesManager() {
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(), base::BindRepeating(&SetDlpRulesManager));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  }

  std::unique_ptr<policy::DlpContentManagerTestHelper> helper_;
  std::vector<DlpPolicyEvent> events_;
};

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest, ContextMenuStaysOpen) {
  // Right click the desktop to open a context menu.
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  const gfx::Point point_on_desktop(1, 1);
  ASSERT_FALSE(browser_window->bounds().Contains(point_on_desktop));

  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           point_on_desktop);
  event_generator.ClickRightButton();

  ash::ShellTestApi shell_test_api;
  ASSERT_TRUE(shell_test_api.IsContextMenuShown());

  ash::CaptureModeTestApi().StartForWindow(/*for_video=*/false);
  EXPECT_TRUE(shell_test_api.IsContextMenuShown());
}

// Checks that video capture emits exactly one DLP reporting event.
IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest, DlpReportingVideoCapture) {
  // Set DLP restriction.
  auto* dlp_content_observer = policy::DlpContentObserver::Get();
  ASSERT_TRUE(dlp_content_observer);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  dlp_content_observer->OnConfidentialityChanged(web_contents,
                                                 kScreenCaptureReported);

  ash::CaptureModeTestApi test_api;

  // Should emit the first reporting event.
  StartVideoRecording();
  ASSERT_TRUE(test_api.IsVideoRecordingInProgress());
  // Set up a waiter to wait for the file to be saved.
  {
    base::RunLoop loop;
    SetupLoopToWaitForCaptureFileToBeSaved(&loop);
    test_api.StopVideoRecording();
    loop.Run();
  }
  ASSERT_FALSE(test_api.IsVideoRecordingInProgress());

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kReport)));

  // Repeat, should emit the second reporting event.
  StartVideoRecording();
  ASSERT_TRUE(test_api.IsVideoRecordingInProgress());
  {
    base::RunLoop loop;
    SetupLoopToWaitForCaptureFileToBeSaved(&loop);
    test_api.StopVideoRecording();
    loop.Run();
  }
  ASSERT_FALSE(test_api.IsVideoRecordingInProgress());

  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kReport)));
}

// Tests DLP reporting without opening the capture bar.
IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpReportingDialogOnFullscreenScreenCaptureShortcut) {
  ASSERT_TRUE(browser());
  // Set DLP restriction.
  auto* dlp_content_observer = policy::DlpContentObserver::Get();
  ASSERT_TRUE(dlp_content_observer);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  dlp_content_observer->OnConfidentialityChanged(web_contents,
                                                 kScreenCaptureReported);

  // Set up a waiter to wait for the file to be saved.
  base::RunLoop loop;
  SetupLoopToWaitForCaptureFileToBeSaved(&loop);

  SendKeyEvent(browser(), ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);

  // Wait for the file to be saved.
  loop.Run();

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kReport)));
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnVideoEndDismissed) {
  ASSERT_TRUE(browser());
  StartVideoRecording();

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  // Video recording should not end as a result of adding a restriction of a
  // warning level type.
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());

  // Set up a waiter to wait for the file to be deleted.
  base::RunLoop loop;
  test_api.SetOnCaptureFileDeletedCallback(base::BindLambdaForTesting(
      [&loop](const base::FilePath& path, bool delete_successful) {
        EXPECT_TRUE(delete_successful);
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_FALSE(base::PathExists(path));
        loop.Quit();
      }));
  StopRecordingAndWaitForDlpWarningDialog(browser());

  // Dismiss the dialog by hitting the ESCAPE key and wait for the file to be
  // deleted.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  loop.Run();

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnVideoEndAccepted) {
  ASSERT_TRUE(browser());
  StartVideoRecording();

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  // Video recording should not end as a result of adding a restriction of a
  // warning level type.
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());

  // Set up a waiter to wait for the file to be saved.
  base::RunLoop loop;
  SetupLoopToWaitForCaptureFileToBeSaved(&loop);
  StopRecordingAndWaitForDlpWarningDialog(browser());

  // Accept the dialog by hitting the ENTER key and wait for the file to be
  // saved.
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  loop.Run();

  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(
      events_[1],
      policy::IsDlpPolicyEvent(policy::CreateDlpPolicyWarningProceededEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId)));
}

// A regression test for https://crbug.com/1350711 in which a session is started
// quickly after clicking the sign out button.
IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       SimulateStartingSessionAfterSignOut) {
  ash::Shell::Get()->session_controller()->RequestSignOut();
  ash::CaptureModeTestApi().StartForFullscreen(false);
}

// Parametrize capture mode browser tests to check both making screenshots and
// video capture. This is particularly important for DLP which handles reporting
// of user activity differently for screenshots and video capture.
class CaptureModeParamBrowserTest : public CaptureModeBrowserTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  CaptureModeParamBrowserTest() : for_video_(GetParam()) {}
  ~CaptureModeParamBrowserTest() override = default;

 protected:
  const bool for_video_;
};

INSTANTIATE_TEST_SUITE_P(CaptureModeParamBrowserTest,
                         CaptureModeParamBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(CaptureModeParamBrowserTest,
                       DlpWarningDialogOnSessionInitDismissed) {
  ASSERT_TRUE(browser());
  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  test_api.StartForFullscreen(for_video_);
  // A capture mode session doesn't start immediately. The controller should be
  // in a pending state waiting for a reply from the DLP manager.
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());

  // Dismiss the dialog by hitting the ESCAPE key. The session should be aborted
  // and the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
}

IN_PROC_BROWSER_TEST_P(CaptureModeParamBrowserTest,
                       DlpWarningDialogOnSessionInitAccepted) {
  ASSERT_TRUE(browser());
  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  test_api.StartForFullscreen(for_video_);
  // A capture mode session doesn't start immediately. The controller should be
  // in a pending state waiting for a reply from the DLP manager.
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());

  // Accept the dialog by hitting the ENTER key. The session should start and
  // the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  // Don't send warning proceeded event as the video capture didn't start.

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
}

IN_PROC_BROWSER_TEST_P(CaptureModeParamBrowserTest,
                       DlpWarningDialogOnPerformingCaptureDismissed) {
  ASSERT_TRUE(browser());
  // Start the session before a window becomes restricted.
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
  test_api.StartForFullscreen(for_video_);
  ASSERT_TRUE(test_api.IsSessionActive());

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());

  // Attempt performing the capture now, it won't be performed immediately,
  // rather the dialog will show instead.
  test_api.PerformCapture();

  // The session should remain active but in a pending state.
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Dismiss the dialog by hitting the ESCAPE key. The session should be aborted
  // and the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnPerformingScreenCaptureAccepted) {
  ASSERT_TRUE(browser());
  // Start the session before a window becomes restricted.
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
  test_api.StartForFullscreen(/*for_video=*/false);
  ASSERT_TRUE(test_api.IsSessionActive());

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());

  // Attempt performing the capture now, it won't be performed immediately,
  // rather the dialog will show instead.
  test_api.PerformCapture();

  // The session should remain active but in a pending state.
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Accept the dialog by hitting the ENTER key. The session should end, and the
  // screenshot should be taken.
  base::RunLoop loop;
  SetupLoopToWaitForCaptureFileToBeSaved(&loop);
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
  loop.Run();

  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(
      events_[1],
      policy::IsDlpPolicyEvent(policy::CreateDlpPolicyWarningProceededEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId)));
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnPerformingVideoCaptureAccepted) {
  ASSERT_TRUE(browser());
  SetupDlpReporting();

  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  ASSERT_TRUE(test_api.IsSessionActive());

  auto* root = GetBrowserWindow(browser())->GetRootWindow();
  ASSERT_TRUE(root);
  DlpWarningDialogWaiter waiter{root};

  // Mark the window as restricted and perform video capture.
  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  test_api.PerformCapture(/*skip_count_down=*/false);
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Wait for the dialog to show before the countdown starts.
  waiter.Wait();
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Accept the dialog by hitting the ENTER key, and expect countdown to start.
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  EXPECT_TRUE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
  EXPECT_FALSE(test_api.IsSessionWaitingForDlpConfirmation());

  // Start the video recording.
  WaitForCountDownToFinish();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());
  EXPECT_FALSE(test_api.IsSessionActive());

  // Stop recording and wait for file to be saved successfully.
  base::RunLoop loop;
  SetupLoopToWaitForCaptureFileToBeSaved(&loop);
  test_api.StopVideoRecording();
  loop.Run();

  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(
      events_[1],
      policy::IsDlpPolicyEvent(policy::CreateDlpPolicyWarningProceededEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId)));
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnCountdownEndDismissed) {
  ASSERT_TRUE(browser());
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  ASSERT_TRUE(test_api.IsSessionActive());
  test_api.PerformCapture(/*skip_count_down=*/false);
  EXPECT_TRUE(test_api.IsInCountDownAnimation());

  // While countdown is in progress, mark the window as restricted, and wait for
  // the dialog to show when the countdown ends.
  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  auto* root = GetBrowserWindow(browser())->GetRootWindow();
  DlpWarningDialogWaiter(root).Wait();
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Dismiss the dialog by hitting the ESCAPE key and expect that recording
  // doesn't start.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnCountdownEndAccepted) {
  ASSERT_TRUE(browser());
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  ASSERT_TRUE(test_api.IsSessionActive());
  test_api.PerformCapture(/*skip_count_down=*/false);
  EXPECT_TRUE(test_api.IsInCountDownAnimation());

  // While countdown is in progress, mark the window as restricted, and wait for
  // the dialog to show when the countdown ends.
  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  auto* root = GetBrowserWindow(browser())->GetRootWindow();
  DlpWarningDialogWaiter(root).Wait();
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Accept the dialog by hitting the ENTER key, and expect recording to start.
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  test_api.FlushRecordingServiceForTesting();
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  // Stop recording and wait for file to be saved successfully.
  base::RunLoop loop;
  SetupLoopToWaitForCaptureFileToBeSaved(&loop);
  test_api.StopVideoRecording();
  loop.Run();

  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(
      events_[1],
      policy::IsDlpPolicyEvent(policy::CreateDlpPolicyWarningProceededEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId)));
}

IN_PROC_BROWSER_TEST_F(
    CaptureModeBrowserTest,
    DlpWarningDialogOnCaptureScreenshotsOfAllDisplaysDismissed) {
  ASSERT_TRUE(browser());
  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  // The screenshots should not be taken immediately through the keyboard
  // shortcut. The controller should be in a pending state waiting for a reply
  // from the DLP manager.
  SendKeyEvent(browser(), ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(test_api.IsPendingDlpCheck());

  // Dismiss the dialog by hitting the ESCAPE key. The screenshot should be
  // aborted and the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
}

IN_PROC_BROWSER_TEST_F(
    CaptureModeBrowserTest,
    DlpWarningDialogOnFullscreenScreenCaptureShortcutAccepted) {
  ASSERT_TRUE(browser());
  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  // The screenshots should not be taken immediately through the keyboard
  // shortcut. The controller should be in a pending state waiting for a reply
  // from the DLP manager.
  SendKeyEvent(browser(), ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(test_api.IsPendingDlpCheck());

  // Set up a waiter to wait for the file to be saved.
  base::RunLoop loop;
  test_api.SetOnCaptureFileSavedCallback(
      base::BindLambdaForTesting([&loop](const base::FilePath& path) {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(base::PathExists(path));
        loop.Quit();
      }));

  // Accept the dialog by hitting the ENTER key. The screenshot should be taken
  // and the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  // Wait for the file to be saved.
  loop.Run();

  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[0],
      policy::IsDlpPolicyEvent(CreateDlpPolicyEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId, policy::DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(
      events_[1],
      policy::IsDlpPolicyEvent(policy::CreateDlpPolicyWarningProceededEvent(
          kSrcPattern, policy::DlpRulesManager::Restriction::kScreenshot,
          kRuleName, kRuleId)));
}

class CaptureModeSettingsBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  CaptureModeSettingsBrowserTest() = default;
  ~CaptureModeSettingsBrowserTest() override = default;

  // extensions::ExtensionBrowserTest:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    CHECK(profile());
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the capture mode folder selection dialog window gets parented
// correctly when a browser window is available.
IN_PROC_BROWSER_TEST_F(CaptureModeSettingsBrowserTest,
                       FolderSelectionDialogParentedCorrectly) {
  ASSERT_TRUE(browser());
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/false);
  test_api.SimulateOpeningFolderSelectionDialog();
  auto* dialog_window = test_api.GetFolderSelectionDialogWindow();
  ASSERT_TRUE(dialog_window);
  auto* transient_root = wm::GetTransientRoot(dialog_window);
  ASSERT_TRUE(transient_root);
  EXPECT_EQ(transient_root->GetId(),
            ash::kShellWindowId_CaptureModeFolderSelectionDialogOwner);
  EXPECT_NE(transient_root, browser()->window()->GetNativeWindow());
}

IN_PROC_BROWSER_TEST_F(CaptureModeSettingsBrowserTest,
                       AudioCaptureDisabledByPolicy) {
  ash::CaptureModeTestApi test_api;
  test_api.SetAudioRecordingEnabled(true);
  EXPECT_TRUE(test_api.GetAudioRecordingEnabled());

  auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAudioCaptureAllowed, false);
  EXPECT_FALSE(test_api.GetAudioRecordingEnabled());
  prefs->SetBoolean(prefs::kAudioCaptureAllowed, true);
  EXPECT_TRUE(test_api.GetAudioRecordingEnabled());
}

// This test fixture tests the chromeos-linux path of camera video frames coming
// from the actual video_capture service using a fake camera device. It can only
// test the `kSharedMemory` buffer type. The `kGpuMemoryBuffer` type path cannot
// be tested here, as the `GpuMemoryBufferTracker` instance on chromeos attempts
// creating a `GpuMemoryBuffer` with the usage
// `VEA_READ_CAMERA_AND_CPU_READ_WRITE` which is not supported in a
// chromeos-linux environment. This path however is tested in ash_unittests.
class CaptureModeCameraBrowserTests : public InProcessBrowserTest {
 public:
  CaptureModeCameraBrowserTests() = default;
  ~CaptureModeCameraBrowserTests() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This command-line switch adds a single fake camera.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

  void SetUpOnMainThread() override {
    ash::CaptureModeTestApi test_api;
    ASSERT_EQ(1u, test_api.GetNumberOfAvailableCameras());
    test_api.SelectCameraAtIndex(0);
  }

  void WaitForAndVerifyRenderedVideoFrame() {
    constexpr int kFramesToRender = 15;
    for (int i = 0; i < kFramesToRender; ++i) {
      base::RunLoop loop;
      ash::CaptureModeTestApi().SetOnCameraVideoFrameRendered(
          base::BindLambdaForTesting(
              [&loop](scoped_refptr<media::VideoFrame> frame) {
                ASSERT_TRUE(frame);
                loop.Quit();
              }));
      loop.Run();
    }
  }
};

IN_PROC_BROWSER_TEST_F(CaptureModeCameraBrowserTests, VerifyFrames) {
  ash::CaptureModeTestApi().StartForFullscreen(/*for_video=*/true);
  WaitForAndVerifyRenderedVideoFrame();
}

class CaptureModeProjectorBrowserTests : public CaptureModeCameraBrowserTests {
 public:
  CaptureModeProjectorBrowserTests() {
    scoped_feature_list_.InitWithFeatures({ash::features::kProjector}, {});
  }

  ~CaptureModeProjectorBrowserTests() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    CaptureModeCameraBrowserTests::SetUpOnMainThread();
    auto* profile = browser()->profile();
    ash::SystemWebAppManager::GetForTest(profile)
        ->InstallSystemAppsForTesting();

    ui_test_utils::BrowserChangeObserver browser_opened(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    ash::ProjectorClient::Get()->OpenProjectorApp();
    browser_opened.Wait();

    Browser* app_browser =
        FindSystemWebAppBrowser(profile, ash::SystemWebAppType::PROJECTOR);
    ASSERT_TRUE(app_browser);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CaptureModeCameraBrowserTests::SetUpCommandLine(command_line);
    command_line->AppendSwitch("--projector-extended-features-disabled");
  }

  void StartProjectorModeSession() {
    auto* projector_session = ash::ProjectorSession::Get();
    EXPECT_FALSE(projector_session->is_active());
    ash::ProjectorController::Get()->StartProjectorSession("projector_data");
    EXPECT_TRUE(projector_session->is_active());
    EXPECT_TRUE(ash::CaptureModeTestApi().IsSessionActive());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the crash reported in https://crbug.com/1368903 is not happening.
IN_PROC_BROWSER_TEST_F(CaptureModeProjectorBrowserTests,
                       NoCrashWhenExitingSessionInWindowRecording) {
  StartProjectorModeSession();
  ash::CaptureModeTestApi test_api;
  ASSERT_TRUE(test_api.GetCameraPreviewWidget());
  test_api.SetCaptureModeSource(ash::CaptureModeSource::kWindow);
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  EXPECT_FALSE(test_api.IsSessionActive());
}
