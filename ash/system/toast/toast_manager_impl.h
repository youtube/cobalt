// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_TOAST_MANAGER_IMPL_H_
#define ASH_SYSTEM_TOAST_TOAST_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/toast/toast_overlay.h"
#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"

namespace aura {
class Window;
}

namespace ash {

namespace eche_app {
class LaunchAppHelperTest;
}

namespace video_conference {
class VideoConferenceIntegrationTest;
}

// Class managing toast requests.
class ASH_EXPORT ToastManagerImpl : public ToastManager,
                                    public ToastOverlay::Delegate,
                                    public SessionObserver,
                                    public ShellObserver {
 public:
  ToastManagerImpl();

  ToastManagerImpl(const ToastManagerImpl&) = delete;
  ToastManagerImpl& operator=(const ToastManagerImpl&) = delete;

  ~ToastManagerImpl() override;

  // ToastManager overrides:
  void Show(ToastData data) override;
  void Cancel(const std::string& id) override;
  bool MaybeToggleA11yHighlightOnActiveToastDismissButton(
      const std::string& id) override;
  bool MaybeActivateHighlightedDismissButtonOnActiveToast(
      const std::string& id) override;
  bool IsRunning(const std::string& id) const override;

  // ToastOverlay::Delegate overrides:
  void OnClosed() override;
  void OnToastHoverStateChanged(bool is_hovering) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  const ToastData& GetCurrentToastDataForTesting() const;

 private:
  class PausableTimer;
  friend class AutoConnectNotifierTest;
  friend class BluetoothNotificationControllerTest;
  friend class DesksTestApi;
  friend class ToastManagerImplTest;
  friend class ClipboardHistoryControllerRefreshTest;
  friend class eche_app::LaunchAppHelperTest;
  friend class video_conference::VideoConferenceIntegrationTest;

  void ShowLatest();

  // Creates a new toast overlay to be displayed on the designated
  // `root_window`.
  void CreateToastOverlayForRoot(aura::Window* root_window);

  // Unshows all existing toast instances in `root_window_to_overlay_`.
  void CloseAllToastsWithAnimation();

  // Resets all existing toast instances in `root_window_to_overlay_`,
  // effectively closing them without animation.
  void CloseAllToastsWithoutAnimation();

  // Checks whether any values in `root_window_to_overlay_` are not empty.
  bool HasActiveToasts() const;

  ToastOverlay* GetCurrentOverlayForTesting(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows());

  int serial_for_testing() const { return serial_; }
  void ResetSerialForTesting() { serial_ = 0; }

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnRootWindowWillShutdown(aura::Window* root_window) override;

  // Data of the toast which is currently shown. Empty if no toast is visible.
  absl::optional<ToastData> current_toast_data_;

  // Used to destroy the currently running toast if its duration is not
  // infinite. Also allows us to persist the toast on hover by pausing this
  // timer when a toast instance is being hovered by the mouse.
  std::unique_ptr<PausableTimer> current_toast_expiration_timer_;

  int serial_ = 0;
  bool locked_;
  base::circular_deque<ToastData> queue_;

  // Tracks active toast overlays and their corresponding root windows.
  base::flat_map<aura::Window*, std::unique_ptr<ToastOverlay>>
      root_window_to_overlay_;

  ScopedSessionObserver scoped_session_observer_{this};
  base::WeakPtrFactory<ToastManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_TOAST_MANAGER_IMPL_H_
