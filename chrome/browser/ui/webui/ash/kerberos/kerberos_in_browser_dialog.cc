// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/kerberos/kerberos_in_browser_dialog.h"

#include <algorithm>
#include <string>

#include "ash/public/cpp/window_backdrop.h"
#include "base/check_op.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {
KerberosInBrowserDialog* g_dialog = nullptr;

constexpr int kKerberosInBrowserDialogWidth = 370;
constexpr int kKerberosInBrowserDialogHeight = 155;
}  // namespace

// static
bool KerberosInBrowserDialog::IsShown() {
  return g_dialog != nullptr;
}

void KerberosInBrowserDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;
}

KerberosInBrowserDialog::KerberosInBrowserDialog(
    base::OnceClosure close_dialog_closure)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIKerberosInBrowserURL),
                              /*title=*/std::u16string()),
      close_dialog_closure_(std::move(close_dialog_closure)) {
  DCHECK(!g_dialog);
  g_dialog = this;
}

KerberosInBrowserDialog::~KerberosInBrowserDialog() {
  if (close_dialog_closure_) {
    std::move(close_dialog_closure_).Run();
  }

  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

ui::ModalType KerberosInBrowserDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

void KerberosInBrowserDialog::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(dialog_window());

  size->SetSize(
      std::min(kKerberosInBrowserDialogWidth, display.work_area().width()),
      std::min(kKerberosInBrowserDialogHeight, display.work_area().height()));
}

bool KerberosInBrowserDialog::ShouldShowCloseButton() const {
  return false;
}

bool KerberosInBrowserDialog::ShouldShowDialogTitle() const {
  return false;
}

// static
void KerberosInBrowserDialog::Show(base::OnceClosure close_dialog_closure) {
  if (g_dialog) {
    g_dialog->dialog_window()->Focus();
    return;
  }

  // Will be deleted by `SystemWebDialogDelegate::OnDialogClosed`.
  g_dialog = new KerberosInBrowserDialog(std::move(close_dialog_closure));
  g_dialog->ShowSystemDialog();

  // TODO(b/252374529): Remove/update this after the dialog behavior on
  // ChromeOS is defined.
  WindowBackdrop::Get(g_dialog->dialog_window())
      ->SetBackdropType(WindowBackdrop::BackdropType::kSemiOpaque);
}

}  // namespace ash
