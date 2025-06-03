// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_WEB_DIALOG_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_WEB_DIALOG_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace ash {

// Launches web dialog during OOBE/Login with specified URL and title.
class LoginWebDialog : public ui::WebDialogDelegate {
 public:
  // If `parent_window` is null then the dialog is placed in the modal dialog
  // container on the primary display.
  LoginWebDialog(content::BrowserContext* browser_context,
                 gfx::NativeWindow parent_window,
                 const std::u16string& title,
                 const GURL& url);

  LoginWebDialog(const LoginWebDialog&) = delete;
  LoginWebDialog& operator=(const LoginWebDialog&) = delete;

  ~LoginWebDialog() override;

  void Show();

  // Overrides dialog title.
  void SetDialogTitle(const std::u16string& title);

  static content::WebContents* GetCurrentWebContents();

  // Returns `dialog_window_` instance for test, can be NULL if dialog is not
  // shown or closed.
  gfx::NativeWindow get_dialog_window_for_test() const {
    return dialog_window_;
  }

 protected:
  // ui::WebDialogDelegate implementation.
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetDialogSize(gfx::Size* size) const override;
  void GetMinimumDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogShown(content::WebUI* webui) override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleOpenURLFromTab(content::WebContents* source,
                            const content::OpenURLParams& params,
                            content::WebContents** out_new_contents) override;
  bool HandleShouldOverrideWebContentsCreation() override;
  std::vector<ui::Accelerator> GetAccelerators() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

 private:
  const raw_ptr<content::BrowserContext, ExperimentalAsh> browser_context_;
  gfx::NativeWindow parent_window_;
  gfx::NativeWindow dialog_window_;

  std::u16string title_;
  const GURL url_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_WEB_DIALOG_H_
