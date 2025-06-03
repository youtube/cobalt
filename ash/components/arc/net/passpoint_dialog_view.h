// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_PASSPOINT_DIALOG_VIEW_H_
#define ASH_COMPONENTS_ARC_NET_PASSPOINT_DIALOG_VIEW_H_

#include <memory>

#include "ash/components/arc/mojom/net.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace arc {

// Dialog to let user allow or disallow an app to provision Passpoint
// credentials.
class PasspointDialogView : public views::BoxLayoutView {
 public:
  // Callback to notify user's confirmation for allowing an app to install
  // Passpoint credentials. If user allow it, the callback is invoked with true
  // as its argument. Otherwise, with false.
  using PasspointDialogCallback =
      base::OnceCallback<void(mojom::PasspointApprovalResponsePtr)>;

  // TestApi is used only in tests to get internal views.
  class TestApi {
   public:
    explicit TestApi(PasspointDialogView* view) : view_(view) {}

    views::StyledLabel* body_text() const { return view_->body_text_; }
    views::MdTextButton* allow_button() const { return view_->allow_button_; }
    views::MdTextButton* dont_allow_button() const {
      return view_->dont_allow_button_;
    }

   private:
    const raw_ptr<PasspointDialogView, ExperimentalAsh> view_;
  };

  PasspointDialogView(mojom::PasspointApprovalRequestPtr request,
                      PasspointDialogCallback callback);
  PasspointDialogView(const PasspointDialogView&) = delete;
  PasspointDialogView& operator=(const PasspointDialogView&) = delete;
  ~PasspointDialogView() override;

  // Shows confirmation dialog for asking user to approve the app to install
  // Passpoint credentials.
  static void Show(aura::Window* parent,
                   mojom::PasspointApprovalRequestPtr request,
                   PasspointDialogCallback callback);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

 private:
  // Get width to be used by label. This is calculated by getting the dialog's
  // width and subtracting it with it's inside margin.
  int GetLabelWidth();

  std::unique_ptr<views::View> MakeBaseLabelView(bool is_expiring);
  std::unique_ptr<views::View> MakeSubscriptionLabelView(
      base::StringPiece friendly_name);
  std::unique_ptr<views::View> MakeContentsView(
      bool is_expiring,
      base::StringPiece friendly_name);
  std::unique_ptr<views::View> MakeButtonsView();

  void OnLearnMoreClicked();
  void OnButtonClicked(bool allow);

  std::u16string app_name_;
  PasspointDialogCallback callback_;

  // Added for testing.
  raw_ptr<views::StyledLabel, ExperimentalAsh> body_text_{nullptr};
  raw_ptr<views::StyledLabel, ExperimentalAsh> body_subscription_text_{nullptr};
  raw_ptr<views::MdTextButton, ExperimentalAsh> allow_button_{nullptr};
  raw_ptr<views::MdTextButton, ExperimentalAsh> dont_allow_button_{nullptr};

  base::WeakPtrFactory<PasspointDialogView> weak_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_NET_PASSPOINT_DIALOG_VIEW_H_
