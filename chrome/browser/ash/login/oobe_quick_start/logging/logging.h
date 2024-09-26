// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_LOGGING_LOGGING_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_LOGGING_LOGGING_H_

#include <sstream>

#include "base/logging.h"

namespace ash::quick_start {

// Use the QS_LOG() macro for all logging related to Quick Start.
#define QS_LOG(severity) \
  ScopedLogMessage(__FILE__, __LINE__, logging::LOG_##severity).stream()

// An intermediate object used by the QS_LOG macro, wrapping a
// logging::LogMessage instance. When this object is destroyed, the message will
// be logged with the standard logging system.
class ScopedLogMessage {
 public:
  ScopedLogMessage(const char* file, int line, logging::LogSeverity severity);
  ScopedLogMessage(const ScopedLogMessage&) = delete;
  ScopedLogMessage& operator=(const ScopedLogMessage&) = delete;
  ~ScopedLogMessage();

  std::ostream& stream() { return stream_; }

 private:
  bool ShouldEmitToStandardLog() const;

  const char* file_;
  int line_;
  logging::LogSeverity severity_;
  std::ostringstream stream_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_LOGGING_LOGGING_H_
