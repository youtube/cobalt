// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kHtmlFile[] = "/chromium_logo_page.html";
}  // namespace

// Page state test cases for the web shell.
@interface PageLoadTestCase : WebShellTestCase
@end

@implementation PageLoadTestCase

// Tests that a simple page loads successfully.
- (void)testPageLoad {
  const GURL pageURL = self.testServer->GetURL(kHtmlFile);

  [ShellEarlGrey loadURL:pageURL];
  [ShellEarlGrey waitForWebStateContainingText:
                     @"Page with some text and the chromium logo image."];
}

@end
