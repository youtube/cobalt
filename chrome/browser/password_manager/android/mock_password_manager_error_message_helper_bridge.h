// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_MANAGER_ERROR_MESSAGE_HELPER_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_MANAGER_ERROR_MESSAGE_HELPER_BRIDGE_H_

#include <jni.h>

#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPasswordManagerErrorMessageHelperBridge
    : public PasswordManagerErrorMessageHelperBridge {
 public:
  MockPasswordManagerErrorMessageHelperBridge();
  ~MockPasswordManagerErrorMessageHelperBridge() override;

  MOCK_METHOD(void,
              StartUpdateAccountCredentialsFlow,
              (content::WebContents * web_contents),
              (override));

  MOCK_METHOD(bool, ShouldShowErrorUI, (), (override));
  MOCK_METHOD(void, SaveErrorUIShownTimestamp, (), (override));
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_MANAGER_ERROR_MESSAGE_HELPER_BRIDGE_H_
