// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_SHEET_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

class AuthenticatorQRViewCentered;

class AuthenticatorQRSheetView : public AuthenticatorRequestSheetView {
 public:
  explicit AuthenticatorQRSheetView(
      std::unique_ptr<AuthenticatorQRSheetModel> model);

  AuthenticatorQRSheetView(const AuthenticatorQRSheetView&) = delete;
  AuthenticatorQRSheetView& operator=(const AuthenticatorQRSheetView&) = delete;

  ~AuthenticatorQRSheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;

  raw_ptr<AuthenticatorQRViewCentered> qr_view_ = nullptr;
  const std::string qr_string_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_SHEET_VIEW_H_
