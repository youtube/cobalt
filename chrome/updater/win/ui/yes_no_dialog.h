// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_YES_NO_DIALOG_H_
#define CHROME_UPDATER_WIN_UI_YES_NO_DIALOG_H_

#include <windows.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/win/ui/owner_draw_controls.h"
#include "chrome/updater/win/ui/resources/resources.grh"

namespace updater::ui {

class YesNoDialog : public CAxDialogImpl<YesNoDialog>,
                    public OwnerDrawTitleBar,
                    public CustomDlgColors,
                    public WTL::CMessageFilter {
  using Base = CAxDialogImpl<YesNoDialog>;

 public:
  static constexpr int IDD = IDD_YES_NO;

  YesNoDialog(WTL::CMessageLoop* message_loop, HWND parent);
  YesNoDialog(const YesNoDialog&) = delete;
  YesNoDialog& operator=(const YesNoDialog&) = delete;
  ~YesNoDialog() override;

  HRESULT Initialize(const std::wstring& yes_no_title,
                     const std::wstring& yes_no_text);
  HRESULT Show();

  bool yes_clicked() const { return yes_clicked_; }

  // Overrides for CMessageFilter.
  BOOL PreTranslateMessage(MSG* msg) override;

  BEGIN_MSG_MAP(YesNoDialog)
    COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedButton)
    COMMAND_ID_HANDLER(IDCANCEL, OnClickedButton)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_NCDESTROY, OnNCDestroy)
    CHAIN_MSG_MAP(Base)
    CHAIN_MSG_MAP(OwnerDrawTitleBar)
    CHAIN_MSG_MAP(CustomDlgColors)
  END_MSG_MAP()

 private:
  // Message and command handlers.
  LRESULT OnClickedButton(WORD notify_code,
                          WORD id,
                          HWND wnd_ctl,
                          BOOL& handled);
  LRESULT OnClose(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnNCDestroy(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);

  raw_ptr<WTL::CMessageLoop> message_loop_;
  HWND parent_;
  bool yes_clicked_;

  // Handle to large icon to show when ALT-TAB.
  base::win::ScopedGDIObject<HICON> hicon_;

  WTL::CFont default_font_;
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_YES_NO_DIALOG_H_
