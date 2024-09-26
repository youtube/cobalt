// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_KERBEROS_KERBEROS_IN_BROWSER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_KERBEROS_KERBEROS_IN_BROWSER_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"

namespace ash {

// A dialog window that is shown when Kerberos authentication in ChromeOS fails
// while trying to access a website.
// Extends from |SystemWebDialogDelegate| to create an always-on-top dialog.
class KerberosInBrowserDialog : public SystemWebDialogDelegate {
 public:
  KerberosInBrowserDialog(const KerberosInBrowserDialog&) = delete;
  KerberosInBrowserDialog& operator=(const KerberosInBrowserDialog&) = delete;

  static bool IsShown();

  // Displays the dialog.
  // |close_dialog_closure| will be called when the dialog is closed.
  static void Show(base::OnceClosure close_dialog_closure = base::DoNothing());

  // ui::SystemWebDialogDelegate overrides.
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

 protected:
  explicit KerberosInBrowserDialog(
      base::OnceClosure close_dialog_closure = base::DoNothing());
  ~KerberosInBrowserDialog() override;

  // ui::WebDialogDelegate overrides
  ui::ModalType GetDialogModalType() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowDialogTitle() const override;

 private:
  base::OnceClosure close_dialog_closure_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_KERBEROS_KERBEROS_IN_BROWSER_DIALOG_H_
