// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_switches.h"

namespace switches {

// Run Chromium in single process mode.
const char kSingleProcessChromeFlag[]   = "single-process";

// Run the tests and the launcher in the same process. Useful for debugging a
// specific test in a debugger.
const char kSingleProcessTestsFlag[]   = "single_process";

// Time (in milliseconds) that the tests should wait before timing out.
// TODO(phajdan.jr): Clean up the switch names.
const char kTestLargeTimeout[] = "test-large-timeout";
const char kTestTinyTimeout[] = "test-tiny-timeout";
const char kUiTestActionTimeout[] = "ui-test-action-timeout";
const char kUiTestActionMaxTimeout[] = "ui-test-action-max-timeout";

}  // namespace switches
