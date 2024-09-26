// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_MEDIA_TRAY_H_
#define ASH_SYSTEM_MEDIA_MEDIA_TRAY_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/style/icon_button.h"
#include "ash/system/media/media_notification_provider_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/memory/raw_ptr.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace views {
class Button;
class ImageView;
class View;
class Widget;
}  // namespace views

namespace ash {

class Shelf;
class TrayBubbleWrapper;

class ASH_EXPORT MediaTray : public MediaNotificationProviderObserver,
                             public TrayBackgroundView,
                             public SessionObserver {
 public:
  // Register `prefs::kGlobalMediaControlsPinned`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns if global media controls is pinned to shelf.
  static bool IsPinnedToShelf();

  // Set `kGlobalMediaControlsPinned`.
  static void SetPinnedToShelf(bool pinned);

  // Pin button shown in `GlobalMediaControlsTitleView` and
  // `UnifiedMediaControlsDetailedView`.
  class PinButton : public IconButton {
   public:
    PinButton();
    PinButton(const PinButton&) = delete;
    PinButton& operator=(const PinButton&) = delete;
    ~PinButton() override = default;

   private:
    void ButtonPressed();
  };

  explicit MediaTray(Shelf* shelf);
  MediaTray(const MediaTray&) = delete;
  MediaTray& operator=(const MediaTray&) = delete;
  ~MediaTray() override;

  // MediaNotificationProviderObserver:
  void OnNotificationListChanged() override;
  void OnNotificationListViewSizeChanged() override;

  // TrayBackgroundView:
  std::u16string GetAccessibleNameForTray() override;
  void UpdateAfterLoginStatusChange() override;
  void HandleLocaleChange() override;
  views::Widget* GetBubbleWidget() const override;
  TrayBubbleView* GetBubbleView() override;
  void ShowBubble() override;
  void CloseBubble() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void AnchorUpdated() override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Show/hide media tray.
  void UpdateDisplayState();

  // If `item_id` is non-empty, the bubble contains just the media item
  // specified by the ID. If it's an empty string then all the items are shown.
  void ShowBubbleWithItem(const std::string& item_id);

  TrayBubbleWrapper* tray_bubble_wrapper_for_testing() { return bubble_.get(); }

  views::View* pin_button_for_testing() { return pin_button_; }

 private:
  friend class MediaTrayTest;

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;

  // Called when theme change, set colors for media notification view.
  void SetNotificationColorTheme();

  // Called when global media controls pin pref is changed.
  void OnGlobalMediaControlsPinPrefChanged();

  void ShowEmptyState();
  std::unique_ptr<TrayBubbleView> CreateTrayBubbleView();

  // Ptr to pin button in the dialog, owned by the view hierarchy.
  raw_ptr<views::Button, ExperimentalAsh> pin_button_ = nullptr;

  std::unique_ptr<TrayBubbleWrapper> bubble_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  raw_ptr<views::ImageView, ExperimentalAsh> icon_;

  raw_ptr<views::View, ExperimentalAsh> content_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> empty_state_view_ = nullptr;

  bool bubble_has_shown_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_MEDIA_TRAY_H_
