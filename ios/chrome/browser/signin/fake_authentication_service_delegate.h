// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_FAKE_AUTHENTICATION_SERVICE_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIGNIN_FAKE_AUTHENTICATION_SERVICE_DELEGATE_H_

#import "ios/chrome/browser/signin/authentication_service_delegate.h"

// Fake AuthenticationServiceDelegate used by FakeAuthenticationService.
class FakeAuthenticationServiceDelegate : public AuthenticationServiceDelegate {
 public:
  FakeAuthenticationServiceDelegate();

  FakeAuthenticationServiceDelegate& operator=(
      const FakeAuthenticationServiceDelegate&) = delete;

  ~FakeAuthenticationServiceDelegate() override;

  // AuthenticationServiceDelegate implementation.
  void ClearBrowsingData(ProceduralBlock completion) override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_FAKE_AUTHENTICATION_SERVICE_DELEGATE_H_
