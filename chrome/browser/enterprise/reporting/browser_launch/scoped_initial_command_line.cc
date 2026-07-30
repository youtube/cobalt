// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_launch/scoped_initial_command_line.h"
#include "base/command_line.h"

namespace {
const base::CommandLine* g_initial_command_line_override = nullptr;
}

namespace enterprise_reporting {

ScopedInitialCommandLine::ScopedInitialCommandLine(
    const base::CommandLine* command_line) {
  g_initial_command_line_override = command_line;
}

ScopedInitialCommandLine::~ScopedInitialCommandLine() {
  g_initial_command_line_override = nullptr;
}

}  // namespace enterprise_reporting

// Fallback link-time definition of GetInitialBrowserCommandLine().
// In production Chrome, the actual implementation is in chrome_main.cc,
// which captures the command line before Chromium modifies it.
// However, many build targets (like unit/browser tests, and utility binaries
// such as `dump_colors`) link //chrome/browser but do not link chrome_main.cc.
// This fallback implementation prevents link errors for those targets,
// allowing them to use ScopedInitialCommandLine to temporarily override
// the command line. For other targets that link chrome_main.cc, they will use
// the real function that returns the command line captured at browser startup.
const base::CommandLine& GetInitialBrowserCommandLine() {
  if (g_initial_command_line_override) {
    return *g_initial_command_line_override;
  }
  return *base::CommandLine::ForCurrentProcess();
}
