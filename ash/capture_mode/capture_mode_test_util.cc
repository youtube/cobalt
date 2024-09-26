// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_test_util.h"

#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/scoped_blocking_call.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/constants.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

// Dispatch the simulated virtual key event to the WindowEventDispatcher.
void DispatchVKEvent(ui::test::EventGenerator* event_generator,
                     bool is_press,
                     ui::KeyboardCode key_code,
                     int flags,
                     int source_device_id) {
  ui::EventType type = is_press ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED;
  ui::KeyEvent keyev(type, key_code, flags);

  keyev.SetProperties({{
      ui::kPropertyFromVK,
      std::vector<uint8_t>(ui::kPropertyFromVKSize),
  }});
  keyev.set_source_device_id(source_device_id);
  event_generator->Dispatch(&keyev);
}

}  // namespace

CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                           CaptureModeType type) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(source);
  controller->SetType(type);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  DCHECK(controller->IsActive());
  return controller;
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

void WaitForRecordingToStart() {
  auto* controller = CaptureModeController::Get();
  if (controller->is_recording_in_progress())
    return;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate);
  base::RunLoop run_loop;
  test_delegate->set_on_recording_started_callback(run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(controller->is_recording_in_progress());
}

void MoveMouseToAndUpdateCursorDisplay(
    const gfx::Point& point,
    ui::test::EventGenerator* event_generator) {
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestPoint(point));
  event_generator->MoveMouseTo(point);
}

void StartVideoRecordingImmediately() {
  CaptureModeController::Get()->StartVideoRecordingImmediatelyForTesting();
  WaitForRecordingToStart();
}

base::FilePath WaitForCaptureFileToBeSaved() {
  base::FilePath result;
  base::RunLoop run_loop;
  ash::CaptureModeTestApi().SetOnCaptureFileSavedCallback(
      base::BindLambdaForTesting([&](const base::FilePath& path) {
        result = path;
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

base::FilePath CreateCustomFolderInUserDownloadsPath(
    const std::string& custom_folder_name) {
  base::FilePath custom_folder = CaptureModeController::Get()
                                     ->delegate_for_testing()
                                     ->GetUserDefaultDownloadsFolder()
                                     .Append(custom_folder_name);
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool result = base::CreateDirectory(custom_folder);
  DCHECK(result);
  return custom_folder;
}

void SendKey(ui::KeyboardCode key_code,
             ui::test::EventGenerator* event_generator,
             int flags,
             int count) {
  for (int i = 0; i < count; ++i)
    event_generator->PressAndReleaseKey(key_code, flags);
}

void WaitForSeconds(int seconds) {
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Seconds(seconds));
  loop.Run();
}

void SwitchToTabletMode() {
  TabletModeControllerTestApi test_api;
  test_api.DetachAllMice();
  test_api.EnterTabletMode();
}

void TouchOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveTouch(view_center);
  event_generator->PressTouch();
  event_generator->ReleaseTouch();
}

void ClickOrTapView(const views::View* view,
                    const bool in_tablet_mode,
                    ui::test::EventGenerator* event_generator) {
  if (in_tablet_mode)
    TouchOnView(view, event_generator);
  else
    ClickOnView(view, event_generator);
}

CaptureModeBarView* GetCaptureModeBarView() {
  auto* session = CaptureModeController::Get()->capture_mode_session();
  DCHECK(session);
  return CaptureModeSessionTestApi(session).GetCaptureModeBarView();
}

IconButton* GetFullscreenToggleButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  return GetCaptureModeBarView()
      ->capture_source_view()
      ->fullscreen_toggle_button();
}

IconButton* GetRegionToggleButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  return GetCaptureModeBarView()->capture_source_view()->region_toggle_button();
}

UserNudgeController* GetUserNudgeController() {
  auto* session = CaptureModeController::Get()->capture_mode_session();
  DCHECK(session);
  return CaptureModeSessionTestApi(session).GetUserNudgeController();
}

bool IsLayerStackedRightBelow(ui::Layer* layer, ui::Layer* sibling) {
  DCHECK_EQ(layer->parent(), sibling->parent());
  const auto& children = layer->parent()->children();
  const int sibling_index =
      base::ranges::find(children, sibling) - children.begin();
  return sibling_index > 0 && children[sibling_index - 1] == layer;
}

void SetDeviceScaleFactor(float dsf) {
  auto* display_manager = Shell::Get()->display_manager();
  const auto display_id = display_manager->GetDisplayAt(0).id();
  display_manager->UpdateZoomFactor(display_id, dsf);
  auto* controller = CaptureModeController::Get();
  if (controller->is_recording_in_progress()) {
    CaptureModeTestApi().FlushRecordingServiceForTesting();
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());
    // Consume any pending video frame from before changing the DSF prior to
    // proceeding.
    test_delegate->RequestAndWaitForVideoFrame();
  }
}

views::Widget* EnableAndGetAutoClickBubbleWidget() {
  auto* autoclick_controller = Shell::Get()->autoclick_controller();
  autoclick_controller->SetEnabled(true, /*show_confirmation_dialog=*/false);
  Shell::Get()
      ->accessibility_controller()
      ->GetFeature(A11yFeatureType::kAutoclick)
      .SetEnabled(true);

  views::Widget* autoclick_bubble_widget =
      autoclick_controller->GetMenuBubbleControllerForTesting()
          ->GetBubbleWidgetForTesting();
  EXPECT_TRUE(autoclick_bubble_widget->IsVisible());
  return autoclick_bubble_widget;
}

void PressKeyOnVK(ui::test::EventGenerator* event_generator,
                  ui::KeyboardCode key_code,
                  int flags,
                  int source_device_id) {
  DispatchVKEvent(event_generator, /*is_press=*/true, key_code, flags,
                  source_device_id);
}

void ReleaseKeyOnVK(ui::test::EventGenerator* event_generator,
                    ui::KeyboardCode key_code,
                    int flags,
                    int source_device_id) {
  DispatchVKEvent(event_generator, /*is_press=*/false, key_code, flags,
                  source_device_id);
}

void PressAndReleaseKeyOnVK(ui::test::EventGenerator* event_generator,
                            ui::KeyboardCode key_code,
                            int flags,
                            int source_device_id) {
  PressKeyOnVK(event_generator, key_code, flags, source_device_id);
  ReleaseKeyOnVK(event_generator, key_code, flags, source_device_id);
}

gfx::Image ReadAndDecodeImageFile(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // No need to read the image file, if the path doesn't exist.
  if (!base::PathExists(image_path)) {
    return gfx::Image();
  }

  std::string image_data;
  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read PNG file from disk.";
    return gfx::Image();
  }

  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
      base::MakeRefCounted<base::RefCountedString>(std::move(image_data)));

  if (image.IsEmpty()) {
    LOG(ERROR) << "Failed to decode PNG file.";
  }

  return image;
}

// -----------------------------------------------------------------------------
// ProjectorCaptureModeIntegrationHelper:

ProjectorCaptureModeIntegrationHelper::ProjectorCaptureModeIntegrationHelper() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kProjector},
      /*disabled_features=*/{});
}

void ProjectorCaptureModeIntegrationHelper::SetUp() {
  auto* projector_controller = ProjectorController::Get();
  projector_controller->SetClient(&projector_client_);
  ON_CALL(projector_client_, StopSpeechRecognition)
      .WillByDefault(testing::Invoke([]() {
        ProjectorController::Get()->OnSpeechRecognitionStopped(
            /*forced=*/false);
      }));

  // Simulate the availability of speech recognition.
  SpeechRecognitionAvailability availability;
  availability.on_device_availability =
      OnDeviceRecognitionAvailability::kAvailable;
  ON_CALL(projector_client_, GetSpeechRecognitionAvailability)
      .WillByDefault(testing::Return(availability));
  EXPECT_CALL(projector_client_, IsDriveFsMounted())
      .WillRepeatedly(testing::Return(true));
}

bool ProjectorCaptureModeIntegrationHelper::CanStartProjectorSession() const {
  return ProjectorController::Get()->GetNewScreencastPrecondition().state !=
         NewScreencastPreconditionState::kDisabled;
}

void ProjectorCaptureModeIntegrationHelper::StartProjectorModeSession() {
  auto* projector_session = ProjectorSession::Get();
  EXPECT_FALSE(projector_session->is_active());
  auto* projector_controller = ProjectorController::Get();
  EXPECT_CALL(projector_client_, MinimizeProjectorApp());
  projector_controller->StartProjectorSession("projector_data");
  EXPECT_TRUE(projector_session->is_active());
  auto* controller = CaptureModeController::Get();
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
}

// -----------------------------------------------------------------------------
// ViewVisibilityChangeWaiter:

ViewVisibilityChangeWaiter ::ViewVisibilityChangeWaiter(views::View* view)
    : view_(view) {
  view_->AddObserver(this);
}

ViewVisibilityChangeWaiter::~ViewVisibilityChangeWaiter() {
  view_->RemoveObserver(this);
}

void ViewVisibilityChangeWaiter::Wait() {
  wait_loop_.Run();
}

void ViewVisibilityChangeWaiter::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  wait_loop_.Quit();
}

}  // namespace ash
