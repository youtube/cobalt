// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/local_authentication_request_widget.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

LocalAuthenticationRequestWidget* g_instance = nullptr;

}  // namespace

// static
void LocalAuthenticationRequestWidget::Show(
    OnLocalAuthenticationCompleted on_local_authentication_completed,
    const std::u16string& title,
    const std::u16string& description,
    LocalAuthenticationRequestView::Delegate* delegate,
    std::unique_ptr<UserContext> user_context) {
  CHECK(!g_instance);

  g_instance = new LocalAuthenticationRequestWidget(
      std::move(on_local_authentication_completed), title, description,
      delegate, std::move(user_context));
}

// static
LocalAuthenticationRequestWidget* LocalAuthenticationRequestWidget::Get() {
  return g_instance;
}

// static
void LocalAuthenticationRequestWidget::UpdateState(
    LocalAuthenticationRequestViewState state,
    const std::u16string& title,
    const std::u16string& description) {
  CHECK_EQ(g_instance, this);
  GetView()->UpdateState(state, title, description);
}

void LocalAuthenticationRequestWidget::SetInputEnabled(bool enabled) {
  CHECK_EQ(g_instance, this);
  GetView()->SetInputEnabled(enabled);
}

void LocalAuthenticationRequestWidget::ClearInput() {
  CHECK_EQ(g_instance, this);
  GetView()->ClearInput();
}

void LocalAuthenticationRequestWidget::Close(bool success) {
  CHECK_EQ(g_instance, this);
  LocalAuthenticationRequestWidget* instance = g_instance;
  g_instance = nullptr;
  std::move(on_local_authentication_completed_).Run(success);
  widget_->Close();
  delete instance;
}

LocalAuthenticationRequestWidget::LocalAuthenticationRequestWidget(
    OnLocalAuthenticationCompleted on_local_authentication_completed,
    const std::u16string& title,
    const std::u16string& description,
    LocalAuthenticationRequestView::Delegate* delegate,
    std::unique_ptr<UserContext> user_context)
    : on_local_authentication_completed_(
          std::move(on_local_authentication_completed)) {
  views::Widget::InitParams widget_params;
  // Using window frameless to be able to get focus on the view input fields,
  // which does not work with popup type.
  widget_params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_params.opacity =
      views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget_params.accept_events = true;

  ShellWindowId parent_window_id =
      Shell::Get()->session_controller()->GetSessionState() ==
              session_manager::SessionState::ACTIVE
          ? kShellWindowId_SystemModalContainer
          : kShellWindowId_LockSystemModalContainer;
  widget_params.parent =
      Shell::GetPrimaryRootWindow()->GetChildById(parent_window_id);
  widget_params.delegate = new LocalAuthenticationRequestView(
      base::BindOnce(&LocalAuthenticationRequestWidget::Close,
                     weak_factory_.GetWeakPtr()),
      title, description, delegate, std::move(user_context));

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(widget_params));

  Show();
}

LocalAuthenticationRequestWidget::~LocalAuthenticationRequestWidget() = default;

void LocalAuthenticationRequestWidget::Show() {
  CHECK(widget_);
  widget_->Show();
}

// static
LocalAuthenticationRequestView*
LocalAuthenticationRequestWidget::GetViewForTesting() {
  LocalAuthenticationRequestWidget* widget =
      LocalAuthenticationRequestWidget::Get();
  return widget ? widget->GetView() : nullptr;
}

LocalAuthenticationRequestView* LocalAuthenticationRequestWidget::GetView() {
  return static_cast<LocalAuthenticationRequestView*>(
      widget_->widget_delegate());
}

}  // namespace ash
