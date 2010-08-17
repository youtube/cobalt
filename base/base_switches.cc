// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"

namespace switches {

// If the program includes chrome/common/debug_on_start.h, the process will
// start the JIT system-registered debugger on itself and will wait for 60
// seconds for the debugger to attach to itself. Then a break point will be hit.
const char kDebugOnStart[]                  = "debug-on-start";

// Disables the crash reporting.
const char kDisableBreakpad[]               = "disable-breakpad";

// Enable DCHECKs in release mode.
const char kEnableDCHECK[]                  = "enable-dcheck";

// Generates full memory crash dump.
const char kFullMemoryCrashReport[]         = "full-memory-crash-report";

// Suppresses all error dialogs when present.
const char kNoErrorDialogs[]                = "noerrdialogs";

// Disable win_util::MessageBox.  This is useful when running as part of
// scripts that do not have a user interface.
const char kNoMessageBox[]                  = "no-message-box";

// When running certain tests that spawn child processes, this switch indicates
// to the test framework that the current process is a child process.
const char kTestChildProcess[]              = "test-child-process";

// Will wait for 60 seconds for a debugger to come to attach to the process.
const char kWaitForDebugger[]               = "wait-for-debugger";

}  // namespace switches
