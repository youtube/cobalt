// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_qrcode_model.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/observer_list.h"
#include "content/public/browser/web_contents.h"

SigninQRCodeModel::SigninQRCodeModel(content::WebContents* web_contents)
    : content::WebContentsUserData<SigninQRCodeModel>(*web_contents) {}

SigninQRCodeModel::~SigninQRCodeModel() {
  for (auto& observer : observers_) {
    observer.OnModelDestroyed(this);
  }
}

void SigninQRCodeModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SigninQRCodeModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SigninQRCodeModel::SetQrCode(std::string_view qr_string) {
  qr_code_string_ = std::string(qr_string);
  for (auto& observer : observers_) {
    observer.OnQrCodeChanged(qr_string);
  }
}

void SigninQRCodeModel::Reset() {
  qr_code_string_.reset();
  for (auto& observer : observers_) {
    observer.OnQrCodeReset();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SigninQRCodeModel);
