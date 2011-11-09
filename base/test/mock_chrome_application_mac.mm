// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/mock_chrome_application_mac.h"

#include "base/logging.h"

@implementation MockCrApp
- (BOOL)isHandlingSendEvent {
  return NO;
}
@end

namespace mock_cr_app {

void RegisterMockCrApp() {
  NSApplication* app = [MockCrApp sharedApplication];

  // Would prefer ASSERT_TRUE() to provide better test failures, but
  // this class is used by remoting/ for a non-test use.
  DCHECK([app conformsToProtocol:@protocol(CrAppProtocol)]);
}

}  // namespace mock_cr_app
