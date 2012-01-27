// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_logging.h"

#include <CoreServices/CoreServices.h>

#include <iomanip>

namespace logging {

OSStatusLogMessage::OSStatusLogMessage(const char* file_path,
                                       int line,
                                       LogSeverity severity,
                                       OSStatus status)
    : LogMessage(file_path, line, severity),
      status_(status) {
}

OSStatusLogMessage::~OSStatusLogMessage() {
  stream() << ": "
           << GetMacOSStatusErrorString(status_)
           << " ("
           << status_
           << ")";
}

}  // namespace logging
