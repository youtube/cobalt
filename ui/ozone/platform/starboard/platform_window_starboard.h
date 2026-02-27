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

#ifndef UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_

#include "base/functional/callback.h"
#include "starboard/window.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/ozone/platform/starboard/platform_event_observer_starboard.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ui {

class PlatformWindowStarboard : public PlatformWindow,
                                public PlatformEventDispatcher,
                                public PlatformEventObserverStarboard {
 public:
  PlatformWindowStarboard(PlatformWindowDelegate* delegate,
                          const gfx::Rect& bounds);

  PlatformWindowStarboard(const PlatformWindowStarboard&) = delete;
  PlatformWindowStarboard& operator=(const PlatformWindowStarboard&) = delete;

  ~PlatformWindowStarboard() override;

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInDIP() const override;
  void SetTitle(const std::u16string& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  bool HasCapture() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetCursor(scoped_refptr<PlatformCursor> cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInDIP() const override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;

  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  bool ShouldUseNativeFrame() const override;
  void SetUseNativeFrame(bool use_native_frame) override;

  using WindowCreatedCallback = base::RepeatingCallback<void(SbWindow)>;
  static void SetWindowCreatedCallback(WindowCreatedCallback cb);

  // ui::PlatformEventObserverStarboard interface.
  void ProcessWindowSizeChangedEvent(int width, int height) override;
  void ProcessFocusEvent(bool is_focused) override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInPixels() const override;

 protected:
  PlatformWindowDelegate* delegate() { return delegate_; }

 private:
  enum class ActivationState {
    kUnknown,
    kActive,
    kInactive,
  };

  SbWindow sb_window_;
  bool use_native_frame_ = false;
  gfx::Rect bounds_;
  raw_ptr<PlatformWindowDelegate> delegate_ = nullptr;
  ui::PlatformWindowState window_state_ = ui::PlatformWindowState::kUnknown;
  ActivationState activation_state_ = ActivationState::kUnknown;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_
