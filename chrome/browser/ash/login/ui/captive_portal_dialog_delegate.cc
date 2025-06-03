// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/captive_portal_dialog_delegate.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Cleans up the delegate for a WebContentsModalDialogManager on destruction, or
// on WebContents destruction, whichever comes first.
class CaptivePortalDialogDelegate::ModalDialogManagerCleanup
    : public content::WebContentsObserver {
 public:
  // This constructor automatically observes |web_contents| for its lifetime.
  explicit ModalDialogManagerCleanup(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ModalDialogManagerCleanup(const ModalDialogManagerCleanup&) = delete;
  ModalDialogManagerCleanup& operator=(const ModalDialogManagerCleanup&) =
      delete;
  ~ModalDialogManagerCleanup() override { ResetDelegate(); }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { ResetDelegate(); }

  void ResetDelegate() {
    if (!web_contents())
      return;
    web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
        ->SetDelegate(nullptr);
  }
};

CaptivePortalDialogDelegate::CaptivePortalDialogDelegate(
    views::WebDialogView* host_dialog_view)
    : host_view_(host_dialog_view),
      web_contents_(host_dialog_view->web_contents()) {
  DCHECK(web_contents_);

  set_dialog_modal_type(ui::ModalType::MODAL_TYPE_SYSTEM);
  set_show_dialog_title(false);

  view_ =
      new views::WebDialogView(ProfileHelper::GetSigninProfile(), this,
                               std::make_unique<ChromeWebContentsHandler>());
  view_->SetVisible(false);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = view_.get();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  ash_util::SetupWidgetInitParamsForContainer(
      &params, kShellWindowId_LockSystemModalContainer);

  // The ownership of the `Widget` we're allocating here is a bit unclear --
  // it's not deleted in this class, but we sometimes try to access the Widget
  // in this class after the widget has been deleted elsewhere. By using a weak
  // ptr to hold onto the widget, we can at least check whether it is still
  // alive before accessing it.
  widget_ = (new views::Widget)->GetWeakPtr();
  widget_->Init(std::move(params));
  widget_->SetBounds(display::Screen::GetScreen()
                         ->GetDisplayNearestWindow(widget_->GetNativeWindow())
                         .work_area());
  widget_->SetOpacity(0);

  // Set this as the web modal delegate so that captive portal dialog can
  // appear.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents_);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
      ->SetDelegate(this);
  modal_dialog_manager_cleanup_ =
      std::make_unique<ModalDialogManagerCleanup>(web_contents_);
}

CaptivePortalDialogDelegate::~CaptivePortalDialogDelegate() {
  for (auto& observer : modal_dialog_host_observer_list_)
    observer.OnHostDestroying();
}

void CaptivePortalDialogDelegate::Show() {
  widget_->Show();
}

void CaptivePortalDialogDelegate::Hide() {
  if (widget_) {
    widget_->Hide();
  }
}

void CaptivePortalDialogDelegate::Close() {
  if (widget_) {
    widget_->Close();
  }
}

void CaptivePortalDialogDelegate::GetDialogSize(gfx::Size* size) const {
  *size = display::Screen::GetScreen()
              ->GetDisplayNearestWindow(widget_->GetNativeWindow())
              .work_area()
              .size();
}

web_modal::WebContentsModalDialogHost*
CaptivePortalDialogDelegate::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView CaptivePortalDialogDelegate::GetHostView() const {
  if (widget_->IsVisible())
    return widget_->GetNativeWindow();
  else
    return host_view_->GetWidget()->GetNativeWindow();
}

gfx::Point CaptivePortalDialogDelegate::GetDialogPosition(
    const gfx::Size& size) {
  // Center the dialog in the screen.
  gfx::Size host_size = GetHostView()->GetBoundsInScreen().size();
  return gfx::Point(host_size.width() / 2 - size.width() / 2,
                    host_size.height() / 2 - size.height() / 2);
}

gfx::Size CaptivePortalDialogDelegate::GetMaximumDialogSize() {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(widget_->GetNativeWindow())
      .work_area()
      .size();
}

void CaptivePortalDialogDelegate::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.AddObserver(observer);
}

void CaptivePortalDialogDelegate::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.RemoveObserver(observer);
}

base::WeakPtr<CaptivePortalDialogDelegate>
CaptivePortalDialogDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
