// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_BUTTONS_H_
#define ASH_SYSTEM_UNIFIED_BUTTONS_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"

// This file carries the buttons that will be shared in both the revamped
// `QuickSettingsView` and the old `UnifiedSystemTrayView`.
//
// TODO(crbug/1370847) When the old`UnifiedSystemTrayView` is removed, these
// buttons should be moved to the cc files that use them.

namespace views {
class Button;
class ImageView;
class Label;
class View;
}  // namespace views

namespace ash {

class UnifiedSystemTrayController;

// A base class of the views showing device management state.
class ASH_EXPORT ManagedStateView : public views::Button {
 public:
  METADATA_HEADER(ManagedStateView);

  ManagedStateView(const ManagedStateView&) = delete;
  ManagedStateView& operator=(const ManagedStateView&) = delete;
  ~ManagedStateView() override = default;

 protected:
  ManagedStateView(PressedCallback callback,
                   int label_id,
                   const gfx::VectorIcon& icon);

  views::Label* label() { return label_; }

 private:
  friend class QuickSettingsHeaderTest;

  // views::Button:
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  void OnThemeChanged() override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Owned by views hierarchy.
  raw_ptr<views::Label, ExperimentalAsh> label_ = nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> image_ = nullptr;

  const raw_ref<const gfx::VectorIcon, ExperimentalAsh> icon_;
};

// A view that shows whether the device is enterprise managed or not. It updates
// by observing EnterpriseDomainModel.
class ASH_EXPORT EnterpriseManagedView : public ManagedStateView,
                                         public EnterpriseDomainObserver,
                                         public SessionObserver {
 public:
  METADATA_HEADER(EnterpriseManagedView);

  explicit EnterpriseManagedView(UnifiedSystemTrayController* controller);
  EnterpriseManagedView(const EnterpriseManagedView&) = delete;
  EnterpriseManagedView& operator=(const EnterpriseManagedView&) = delete;
  ~EnterpriseManagedView() override;

  // Adjusts the layout for a narrower appearance, using a shorter label for
  // the button.
  void SetNarrowLayout(bool narrow);

 private:
  // EnterpriseDomainObserver:
  void OnDeviceEnterpriseInfoChanged() override;
  void OnEnterpriseAccountDomainChanged() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus status) override;

  // Updates the view visibility and displayed string.
  void Update();

  // See SetNarrowLayout().
  bool narrow_layout_ = false;
};

// A view that shows whether the user is supervised or a child.
class ASH_EXPORT SupervisedUserView : public ManagedStateView {
 public:
  METADATA_HEADER(SupervisedUserView);

  SupervisedUserView();
  SupervisedUserView(const SupervisedUserView&) = delete;
  SupervisedUserView& operator=(const SupervisedUserView&) = delete;
  ~SupervisedUserView() override = default;
};

// The avatar button shows in the quick setting bubble.
class ASH_EXPORT UserAvatarButton : public views::Button {
 public:
  METADATA_HEADER(UserAvatarButton);

  explicit UserAvatarButton(PressedCallback callback);
  UserAvatarButton(const UserAvatarButton&) = delete;
  UserAvatarButton& operator=(const UserAvatarButton&) = delete;
  ~UserAvatarButton() override = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_BUTTONS_H_
