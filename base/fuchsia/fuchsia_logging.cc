// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/fuchsia_logging.h"

#include <zircon/status.h>

#include <iomanip>

#include "base/location.h"
#include "base/process/process.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"

namespace logging {

ZxLogMessage::ZxLogMessage(const char* file_path,
                           int line,
                           LogSeverity severity,
                           zx_status_t zx_status)
    : LogMessage(file_path, line, severity), zx_status_(zx_status) {}

ZxLogMessage::~ZxLogMessage() {
  // zx_status_t error values are negative, so log the numeric version as
  // decimal rather than hex. This is also useful to match zircon/errors.h for
  // grepping.
  stream() << ": " << zx_status_get_string(zx_status_) << " (" << zx_status_
           << ")";
}

}  // namespace logging

namespace base {

namespace internal {

std::string FidlConnectionErrorMessage(const base::StringPiece& protocol_name,
                                       const base::StringPiece& status_string) {
  return base::StringPrintf("Failed to connect to %s: %s", protocol_name.data(),
                            status_string.data());
}

std::string FidlMethodResultErrorMessage(
    const base::StringPiece& formatted_error,
    const base::StringPiece& method_name) {
  return base::StringPrintf("Error calling %s: %s", method_name.data(),
                            formatted_error.data());
}

}  // namespace internal

fit::function<void(zx_status_t)> LogFidlErrorAndExitProcess(
    const Location& from_here,
    StringPiece protocol_name) {
  return [from_here, protocol_name](zx_status_t status) {
    {
      logging::ZxLogMessage(from_here.file_name(), from_here.line_number(),
                            logging::LOGGING_ERROR, status)
              .stream()
          << protocol_name << " disconnected unexpectedly, exiting";
    }
    base::Process::TerminateCurrentProcessImmediately(1);
  };
}

std::string FidlMethodResultErrorMessage(
    const fit::result<fidl::OneWayError>& result,
    const base::StringPiece& method_name) {
  CHECK(result.is_error());
  return internal::FidlMethodResultErrorMessage(
      result.error_value().FormatDescription(), method_name);
}

fit::function<void(fidl::UnbindInfo)> FidlBindingClosureWarningLogger(
    base::StringPiece protocol_name) {
  return [protocol_name](fidl::UnbindInfo info) {
    ZX_LOG(WARNING, info.status()) << protocol_name << " unbound";
  };
}

}  // namespace base
