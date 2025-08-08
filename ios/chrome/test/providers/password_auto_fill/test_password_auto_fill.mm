// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/password_auto_fill/password_auto_fill_api.h"

namespace ios {
namespace provider {

BOOL SupportShortenedInstructionForPasswordAutoFill() {
  return NO;
}

void PasswordsInOtherAppsOpensSettings() {
  // Test implementation does nothing.
}

}  // namespace provider
}  // namespace ios
