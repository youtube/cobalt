// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_force_close_watcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/crostini/crostini_force_close_view.h"
#include "components/exo/shell_surface_base.h"
#include "ui/views/widget/widget.h"

namespace crostini {

namespace {

constexpr base::TimeDelta kDefaultForceCloseDelay = base::Seconds(5);
}

ForceCloseWatcher::Delegate::~Delegate() = default;

void ForceCloseWatcher::Watch(std::unique_ptr<Delegate> delegate) {
  new ForceCloseWatcher(std::move(delegate));
}

void ForceCloseWatcher::OnWidgetDestroying(views::Widget* widget) {
  delegate_->Hide();
  widget->RemoveObserver(this);
  delete this;
}

void ForceCloseWatcher::OnCloseRequested() {
  if (!show_dialog_timer_.has_value()) {
    show_dialog_timer_ = base::ElapsedTimer();
    return;
  }

  if (show_dialog_timer_->Elapsed() < force_close_delay_)
    return;

  delegate_->Prompt();
}

void ForceCloseWatcher::OverrideDelayForTesting(base::TimeDelta delay) {
  force_close_delay_ = delay;
}

ForceCloseWatcher::ForceCloseWatcher(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      force_close_delay_(kDefaultForceCloseDelay) {
  delegate_->GetClosableWidget()->AddObserver(this);
  delegate_->Watched(this);
}

ForceCloseWatcher::~ForceCloseWatcher() {
  CHECK(!IsInObserverList());
}

ShellSurfaceForceCloseDelegate::ShellSurfaceForceCloseDelegate(
    exo::ShellSurfaceBase* shell_surface,
    std::string app_name)
    : shell_surface_(shell_surface),
      app_name_(std::move(app_name)),
      weak_ptr_factory_(this) {}

void ShellSurfaceForceCloseDelegate::ForceClose() {
  // Post a task because the dialog needs to finish running its accept button
  // handling code. If we CloseNow() here it will destroy the dialog's parent
  // widget, destroying the dialog and causing a use-after-free.
  // https://crbug.com/1215247
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ShellSurfaceForceCloseDelegate::ForceCloseNow,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ShellSurfaceForceCloseDelegate::ForceCloseNow() {
  // This must use CloseNow() and not Close() because ShellSurfaceBase widgets
  // respond to Close() by asking the client app to close. In this case
  // the app is not responding, so it won't respond to the Wayland protocol
  // zxdg_toplevel_v6_send_close() message.
  GetClosableWidget()->CloseNow();
}

ShellSurfaceForceCloseDelegate::~ShellSurfaceForceCloseDelegate() {
  CHECK(!IsInObserverList());
}

views::Widget* ShellSurfaceForceCloseDelegate::GetClosableWidget() {
  DCHECK(shell_surface_->GetWidget());
  return shell_surface_->GetWidget();
}

void ShellSurfaceForceCloseDelegate::Watched(ForceCloseWatcher* watcher) {
  // It is safe to use base::Unretained here. The watcher's liefetime is tied to
  // the widget associated with this shell surface, and the widget's
  // pre_close_callback_ can not be called on a deleted widget, so the watcher
  // will also be alive.
  shell_surface_->set_pre_close_callback(base::BindRepeating(
      &ForceCloseWatcher::OnCloseRequested, base::Unretained(watcher)));
}

void ShellSurfaceForceCloseDelegate::Prompt() {
  if (current_dialog_)
    Hide();

  DCHECK(!current_dialog_);
  current_dialog_ = ShowCrostiniForceCloseDialog(
      app_name_, GetClosableWidget(),
      base::BindOnce(&ShellSurfaceForceCloseDelegate::ForceClose,
                     weak_ptr_factory_.GetWeakPtr()));
  current_dialog_->AddObserver(this);
}

void ShellSurfaceForceCloseDelegate::Hide() {
  if (current_dialog_) {
    current_dialog_->RemoveObserver(this);
    current_dialog_->Close();
    current_dialog_ = nullptr;
  }
}

void ShellSurfaceForceCloseDelegate::OnWidgetDestroying(views::Widget* widget) {
  if (current_dialog_) {
    current_dialog_->RemoveObserver(this);
    current_dialog_ = nullptr;
  }
}

}  //  namespace crostini
