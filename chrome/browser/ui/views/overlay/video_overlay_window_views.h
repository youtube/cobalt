// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_VIDEO_OVERLAY_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_VIDEO_OVERLAY_WINDOW_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ui/frame/highlight_border_overlay.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace viz {
class FrameSinkId;
}  // namespace viz

class BackToTabLabelButton;
class CloseImageButton;
class HangUpButton;
class PlaybackImageButton;
class ResizeHandleButton;
class SimpleOverlayWindowImageButton;
class SkipAdLabelButton;
class ToggleMicrophoneButton;
class ToggleCameraButton;

// The Chrome desktop implementation of VideoOverlayWindow. This will only be
// implemented in views, which will support all desktop platforms.
class VideoOverlayWindowViews : public content::VideoOverlayWindow,
                                public views::Widget,
                                public display::DisplayObserver {
 public:
  static std::unique_ptr<VideoOverlayWindowViews> Create(
      content::VideoPictureInPictureWindowController* controller);

  VideoOverlayWindowViews(const VideoOverlayWindowViews&) = delete;
  VideoOverlayWindowViews& operator=(const VideoOverlayWindowViews&) = delete;

  ~VideoOverlayWindowViews() override;

  enum class WindowQuadrant { kBottomLeft, kBottomRight, kTopLeft, kTopRight };

  // VideoOverlayWindow:
  void Close() override;
  void ShowInactive() override;
  void Hide() override;
  gfx::Rect GetBounds() override;
  void UpdateNaturalSize(const gfx::Size& natural_size) override;
  void SetPlaybackState(PlaybackState playback_state) override;
  void SetPlayPauseButtonVisibility(bool is_visible) override;
  void SetSkipAdButtonVisibility(bool is_visible) override;
  void SetNextTrackButtonVisibility(bool is_visible) override;
  void SetPreviousTrackButtonVisibility(bool is_visible) override;
  void SetMicrophoneMuted(bool muted) override;
  void SetCameraState(bool turned_on) override;
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override;
  void SetToggleCameraButtonVisibility(bool is_visible) override;
  void SetHangUpButtonVisibility(bool is_visible) override;
  void SetPreviousSlideButtonVisibility(bool is_visible) override;
  void SetNextSlideButtonVisibility(bool is_visible) override;
  void SetSurfaceId(const viz::SurfaceId& surface_id) override;

  // views::Widget
  bool IsActive() const override;
  bool IsVisible() const override;
  void OnNativeFocus() override;
  void OnNativeBlur() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnNativeWidgetMove() override;
  void OnNativeWidgetDestroying() override;
  void OnNativeWidgetDestroyed() override;
  void OnNativeWidgetAddedToCompositor() override;
  void OnNativeWidgetRemovingFromCompositor() override;
  void OnNativeWidgetSizeChanged(const gfx::Size& new_size) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // display::DisplayObserver
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  bool ControlsHitTestContainsPoint(const gfx::Point& point);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Gets the proper hit test component when the hit point is on the resize
  // handle in order to force a drag-to-resize.
  int GetResizeHTComponent() const;

  gfx::Rect GetResizeHandleControlsBounds();

  // Updates the bounds of |resize_handle_view_| based on what |quadrant| the
  // PIP window is in.
  void UpdateResizeHandleBounds(WindowQuadrant quadrant);
#endif

  // Called when the bounds of the controls should be updated.
  void OnUpdateControlsBounds();

  content::PictureInPictureWindowController* GetController() const;
  views::View* GetWindowBackgroundView() const;
  views::View* GetControlsContainerView() const;

  gfx::Size& GetNaturalSize();

  bool OnGestureEventHandledOrIgnored(ui::GestureEvent* event);

  // Returns true if the controls (e.g. close button, play/pause button) are
  // visible.
  bool AreControlsVisible() const;

  // Updates the controls view::Views to reflect |is_visible|.
  void UpdateControlsVisibility(bool is_visible);

  // Gets the bounds of the controls.
  gfx::Rect GetBackToTabControlsBounds();
  gfx::Rect GetSkipAdControlsBounds();
  gfx::Rect GetCloseControlsBounds();
  gfx::Rect GetPlayPauseControlsBounds();
  gfx::Rect GetNextTrackControlsBounds();
  gfx::Rect GetPreviousTrackControlsBounds();
  gfx::Rect GetToggleMicrophoneButtonBounds();
  gfx::Rect GetToggleCameraButtonBounds();
  gfx::Rect GetHangUpButtonBounds();
  gfx::Rect GetPreviousSlideControlsBounds();
  gfx::Rect GetNextSlideControlsBounds();

  PlaybackImageButton* play_pause_controls_view_for_testing() const;
  SimpleOverlayWindowImageButton* next_track_controls_view_for_testing() const;
  SimpleOverlayWindowImageButton* previous_track_controls_view_for_testing()
      const;
  SkipAdLabelButton* skip_ad_controls_view_for_testing() const;
  ToggleMicrophoneButton* toggle_microphone_button_for_testing() const;
  ToggleCameraButton* toggle_camera_button_for_testing() const;
  HangUpButton* hang_up_button_for_testing() const;
  SimpleOverlayWindowImageButton* next_slide_controls_view_for_testing() const;
  SimpleOverlayWindowImageButton* previous_slide_controls_view_for_testing()
      const;
  CloseImageButton* close_button_for_testing() const;
  gfx::Point close_image_position_for_testing() const;
  gfx::Point resize_handle_position_for_testing() const;
  PlaybackState playback_state_for_testing() const;
  ui::Layer* video_layer_for_testing() const;

  void ForceControlsVisibleForTesting(bool visible);

  // Determines whether a layout of the window controls has been scheduled but
  // is not done yet.
  bool IsLayoutPendingForTesting() const;

  void set_minimum_size_for_testing(const gfx::Size& min_size) {
    min_size_ = min_size;
  }

 protected:
  explicit VideoOverlayWindowViews(
      content::VideoPictureInPictureWindowController* controller);

 private:
  // Return the work area for the nearest display the widget is on.
  gfx::Rect GetWorkAreaForWindow() const;

  // Determine the intended bounds of |this|. This should be called when there
  // is reason for the bounds to change, such as switching primary displays or
  // playing a new video (i.e. different aspect ratio).
  gfx::Rect CalculateAndUpdateWindowBounds();

  // Set up the views::Views that will be shown on the window.
  void SetUpViews();

  // Finish initialization by performing the steps that require the root View.
  void OnRootViewReady();

  // Updates the bounds of the controls.
  void UpdateControlsBounds();

  // Update the max size of the widget based on |work_area| and window size.
  void UpdateMaxSize(const gfx::Rect& work_area);

  // Update the bounds of the layers on the window. This may introduce
  // letterboxing.
  void UpdateLayerBoundsWithLetterboxing(gfx::Size window_size);

  // Toggles the play/pause control through the |controller_| and updates the
  // |play_pause_controls_view_| toggled state to reflect the current playing
  // state.
  void TogglePlayPause();

  // Returns the current frame sink id for the surface displayed in the
  // |video_view_|. If |video_view_| is not currently displaying a surface then
  // returns nullptr.
  const viz::FrameSinkId* GetCurrentFrameSinkId() const;

  // Unregisters the current frame sink id for the surface displayed in the
  // |video_view_| from its parent frame sink if the frame sink hierarchy has
  // been registered before.
  void MaybeUnregisterFrameSinkHierarchy();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class OverlayWindowControl {
    kBackToTab = 0,
    kMuteDeprecated,
    kSkipAd,
    kClose,
    kPlayPause,
    kNextTrack,
    kPreviousTrack,
    kToggleMicrophone,
    kToggleCamera,
    kHangUp,
    kPreviousSlide,
    kNextSlide,
    kMaxValue = kNextSlide
  };
  void RecordButtonPressed(OverlayWindowControl);
  void RecordTapGesture(OverlayWindowControl);

  // Not owned; |controller_| owns |this|.
  raw_ptr<content::VideoPictureInPictureWindowController> controller_;

  // Whether or not the play/pause button will be shown.
  bool show_play_pause_button_ = false;

  // Temporary storage for child Views. Used during the time between
  // construction and initialization, when the views::View pointer members must
  // already be initialized, but there is no root view to add them to yet.
  std::vector<std::unique_ptr<views::View>> view_holder_;

  // Whether or not the window has been shown before. This is used to determine
  // sizing and placement. This is different from checking whether the window
  // components has been initialized.
  bool has_been_shown_ = false;

  // The upper and lower bounds of |current_size_|. These are determined by the
  // size of the primary display work area when Picture-in-Picture is initiated.
  // TODO(apacible): Update these bounds when the display the window is on
  // changes. http://crbug.com/819673
  gfx::Size min_size_;
  gfx::Size max_size_;

  // The natural size of the video to show. This is used to compute sizing and
  // ensuring factors such as aspect ratio is maintained.
  gfx::Size natural_size_;

  // Automatically hides the controls a few seconds after user tap gesture.
  base::RetainingOneShotTimer hide_controls_timer_;

  // Timer used to update controls bounds.
  std::unique_ptr<base::OneShotTimer> update_controls_bounds_timer_;

  // If set, controls will always either be shown or hidden, instead of showing
  // and hiding automatically. Only used for testing via
  // ForceControlsVisibleForTesting().
  absl::optional<bool> force_controls_visible_;

  // Views to be shown. The views are first temporarily owned by view_holder_,
  // then passed to this widget's ContentsView which takes ownership.
  raw_ptr<views::View> window_background_view_ = nullptr;
  raw_ptr<views::View> video_view_ = nullptr;
  raw_ptr<views::View> controls_scrim_view_ = nullptr;
  raw_ptr<views::View> controls_container_view_ = nullptr;
  raw_ptr<CloseImageButton> close_controls_view_ = nullptr;
  raw_ptr<BackToTabLabelButton> back_to_tab_label_button_ = nullptr;
  raw_ptr<SimpleOverlayWindowImageButton> previous_track_controls_view_ =
      nullptr;
  raw_ptr<PlaybackImageButton> play_pause_controls_view_ = nullptr;
  raw_ptr<SimpleOverlayWindowImageButton> next_track_controls_view_ = nullptr;
  raw_ptr<SkipAdLabelButton> skip_ad_controls_view_ = nullptr;
  raw_ptr<ResizeHandleButton> resize_handle_view_ = nullptr;
  raw_ptr<ToggleMicrophoneButton> toggle_microphone_button_ = nullptr;
  raw_ptr<ToggleCameraButton> toggle_camera_button_ = nullptr;
  raw_ptr<HangUpButton> hang_up_button_ = nullptr;
  raw_ptr<SimpleOverlayWindowImageButton> previous_slide_controls_view_ =
      nullptr;
  raw_ptr<SimpleOverlayWindowImageButton> next_slide_controls_view_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Generates a nine patch layer painted with a highlight border for ChromeOS
  // ASH only, not including LaCrOS. Highlight border for chrome pip window in
  // LaCrOS will be added when `ash::NonClientFrameViewAsh` is created.
  std::unique_ptr<HighlightBorderOverlay> highlight_border_overlay_;
#endif

  // Current playback state on the video in Picture-in-Picture window. It is
  // used to toggle play/pause/replay button.
  PlaybackState playback_state_for_testing_ = kEndOfVideo;

  // Whether or not the skip ad button will be shown. This is the
  // case when Media Session "skipad" action is handled by the website.
  bool show_skip_ad_button_ = false;

  // Whether or not the next track button will be shown. This is the
  // case when Media Session "nexttrack" action is handled by the website.
  bool show_next_track_button_ = false;

  // Whether or not the previous track button will be shown. This is the
  // case when Media Session "previoustrack" action is handled by the website.
  bool show_previous_track_button_ = false;

  // Whether or not the toggle microphone button will be shown. This is the case
  // when Media Session "togglemicrophone" action is handled by the website.
  bool show_toggle_microphone_button_ = false;

  // Whether or not the toggle camera button will be shown. This is the case
  // when Media Session "togglecamera" action is handled by the website.
  bool show_toggle_camera_button_ = false;

  // Whether or not the hang up button will be shown. This is the case when
  // Media Session "hangup" action is handled by the website.
  bool show_hang_up_button_ = false;

  // Whether or not the previous slide button will be shown. This is the
  // case when Media Session "previousslide" action is handled by the website.
  bool show_previous_slide_button_ = false;

  // Whether or not the next slide button will be shown. This is the
  // case when Media Session "nextslide" action is handled by the website.
  bool show_next_slide_button_ = false;

  // Whether or not the current frame sink for the surface displayed in the
  // |video_view_| is registered as the child of the overlay window frame sink.
  bool has_registered_frame_sink_hierarchy_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_VIDEO_OVERLAY_WINDOW_VIEWS_H_
