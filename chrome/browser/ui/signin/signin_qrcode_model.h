// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_MODEL_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_MODEL_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// A tab-scoped model that caches the FIDO WebAuthn QR code generated during the
// Chrome sign-in flow. It acts as a bridge between the request delegate
// (WebAuthn) and the sign-in banner (UI).
class SigninQRCodeModel
    : public content::WebContentsUserData<SigninQRCodeModel> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a new QR code string is generated and set.
    virtual void OnQrCodeChanged(std::string_view qr_code_string) = 0;

    // Called when the active WebAuthn request is destroyed or aborted,
    // invalidating the cached QR code.
    virtual void OnQrCodeReset() = 0;

    virtual void OnModelDestroyed(SigninQRCodeModel* model) = 0;
  };

  explicit SigninQRCodeModel(content::WebContents* web_contents);
  SigninQRCodeModel(const SigninQRCodeModel&) = delete;
  SigninQRCodeModel& operator=(const SigninQRCodeModel&) = delete;
  ~SigninQRCodeModel() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets the generated QR code string and notifies all observers.
  void SetQrCode(std::string_view qr_string);

  // Clears the cached QR code string and notifies all observers.
  void Reset();

  // Returns the currently cached QR code string, if any.
  std::optional<std::string_view> qr_code_string() const {
    return qr_code_string_ ? std::optional<std::string_view>(*qr_code_string_)
                           : std::nullopt;
  }

 private:
  friend class content::WebContentsUserData<SigninQRCodeModel>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  std::optional<std::string> qr_code_string_;
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_MODEL_H_
