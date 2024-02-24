// Copyright 2024 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/launch.h"

#include "base/notreached.h"

namespace base {

void RaiseProcessToHighPriority() {
  // Impossible on iOS. Do nothing.
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  NOTIMPLEMENTED();
  return false;
}

bool GetAppOutputAndError(const CommandLine& cl, std::string* output) {
  NOTIMPLEMENTED();
  return false;
}

Process LaunchProcess(const CommandLine& cmdline,
                      const LaunchOptions& options) {
  NOTIMPLEMENTED();
  return Process();
}

}  // namespace base
