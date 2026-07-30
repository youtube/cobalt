// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_SCOPED_INITIAL_COMMAND_LINE_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_SCOPED_INITIAL_COMMAND_LINE_H_

namespace base {
class CommandLine;
}

// Global function to get the initial browser command line.
// In production, this is defined in chrome_main.cc.
// For testing or utility binaries, a fallback/override is defined in
// scoped_initial_command_line.cc.
const base::CommandLine& GetInitialBrowserCommandLine();

namespace enterprise_reporting {

// RAII class to temporarily override the initial command line.
// The override is cleared automatically when this object goes out of scope.
class ScopedInitialCommandLine {
 public:
  explicit ScopedInitialCommandLine(const base::CommandLine* command_line);
  ~ScopedInitialCommandLine();

  ScopedInitialCommandLine(const ScopedInitialCommandLine&) = delete;
  ScopedInitialCommandLine& operator=(const ScopedInitialCommandLine&) = delete;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_SCOPED_INITIAL_COMMAND_LINE_H_
