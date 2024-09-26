// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_H_

#include <memory>

#include "ash/components/arc/compat_mode/resize_util.h"
#include "base/cancelable_callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Rect;
struct VectorIcon;
}  // namespace gfx

namespace views {
class BubbleDialogDelegateView;
class ImageView;
class Label;
}  // namespace views

namespace arc {

class ArcResizeLockPrefDelegate;

class ResizeToggleMenu : public views::WidgetObserver,
                         public aura::WindowObserver {
 public:
  class MenuButtonView : public views::Button {
   public:
    MenuButtonView(PressedCallback callback,
                   const gfx::VectorIcon& icon,
                   int title_string_id);
    MenuButtonView(const MenuButtonView&) = delete;
    MenuButtonView& operator=(const MenuButtonView&) = delete;
    ~MenuButtonView() override;

    void SetSelected(bool is_selected);

   private:
    // views::View:
    void OnThemeChanged() override;
    gfx::Size CalculatePreferredSize() const override;

    void UpdateColors();
    void UpdateState();

    // Owned by views hierarchy.
    views::ImageView* icon_view_{nullptr};
    views::Label* title_{nullptr};

    const raw_ref<const gfx::VectorIcon, ExperimentalAsh> icon_;
    bool is_selected_{false};
  };

  ResizeToggleMenu(views::Widget* widget,
                   ArcResizeLockPrefDelegate* pref_delegate);
  ResizeToggleMenu(const ResizeToggleMenu&) = delete;
  ResizeToggleMenu& operator=(const ResizeToggleMenu&) = delete;
  ~ResizeToggleMenu() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  bool IsBubbleShown() const;

 private:
  friend class ResizeToggleMenuTest;

  void UpdateSelectedButton();

  void ApplyResizeCompatMode(ResizeCompatMode mode);

  gfx::Rect GetAnchorRect() const;

  base::WeakPtr<views::BubbleDialogDelegateView> bubble_view_;

  std::unique_ptr<views::BubbleDialogDelegateView> MakeBubbleDelegateView(
      views::Widget* parent,
      gfx::Rect anchor_rect,
      base::RepeatingCallback<void(ResizeCompatMode)> command_handler);

  void CloseBubble();

  raw_ptr<views::Widget, ExperimentalAsh> widget_;

  raw_ptr<ArcResizeLockPrefDelegate, ExperimentalAsh> pref_delegate_;

  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  base::CancelableOnceClosure auto_close_closure_;

  raw_ptr<views::Widget, ExperimentalAsh> bubble_widget_{nullptr};

  // Store only for testing.
  raw_ptr<MenuButtonView, ExperimentalAsh> phone_button_{nullptr};
  raw_ptr<MenuButtonView, ExperimentalAsh> tablet_button_{nullptr};
  raw_ptr<MenuButtonView, ExperimentalAsh> resizable_button_{nullptr};

  base::WeakPtrFactory<ResizeToggleMenu> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_H_
