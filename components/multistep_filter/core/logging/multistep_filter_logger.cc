// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_logger.h"

#include <string>
#include <utility>

namespace multistep_filter {

ScopedLogMessage::ScopedLogMessage(MultistepFilterLogRouter* logger,
                                   int64_t navigation_id,
                                   LogEventType type,
                                   std::string_view host)
    : logger_(logger), entry_(navigation_id, type, host) {}

ScopedLogMessage::~ScopedLogMessage() {
  if (logger_) {
    logger_->RouteLogMessage(std::move(entry_));
  }
}

}  // namespace multistep_filter
