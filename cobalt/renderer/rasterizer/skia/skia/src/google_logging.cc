// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides integration with Google-style "base/logging.h" assertions
// for Skia SkASSERT. If you don't want this, you can link with another file
// that provides integration with the logging of your choice.

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkTypes.h"

void SkDebugf_FileLine(const char* file, int line, bool fatal,
                       const char* format, ...) {
  va_list ap;
  va_start(ap, format);

  std::string message;
  base::StringAppendV(&message, format, ap);
  va_end(ap);

  if (fatal) {
    logging::LogMessage(file, line, logging::LOG_FATAL).stream() << message;
  } else if (DLOG_IS_ON(INFO)) {
    logging::LogMessage(file, line, logging::LOG_INFO).stream() << message;
  }
}
