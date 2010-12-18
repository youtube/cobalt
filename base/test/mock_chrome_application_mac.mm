// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/mock_chrome_application_mac.h"

@implementation MockCrApp
- (BOOL)isHandlingSendEvent {
  return NO;
}
@end

namespace mock_cr_app {

void RegisterMockCrApp() {
  [MockCrApp sharedApplication];
}

}  // namespace mock_cr_app
