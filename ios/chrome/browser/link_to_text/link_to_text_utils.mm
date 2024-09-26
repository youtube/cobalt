// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_utils.h"

#import "base/time/time.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/chrome/browser/link_to_text/link_to_text_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using shared_highlighting::LinkGenerationError;

namespace link_to_text {

absl::optional<LinkGenerationOutcome> ParseStatus(
    absl::optional<double> status) {
  if (!status.has_value()) {
    return absl::nullopt;
  }

  int status_value = static_cast<int>(status.value());
  if (status_value < 0 ||
      status_value > static_cast<int>(LinkGenerationOutcome::kMaxValue)) {
    return absl::nullopt;
  }

  return static_cast<LinkGenerationOutcome>(status_value);
}

shared_highlighting::LinkGenerationError OutcomeToError(
    LinkGenerationOutcome outcome) {
  switch (outcome) {
    case LinkGenerationOutcome::kInvalidSelection:
      return LinkGenerationError::kIncorrectSelector;
    case LinkGenerationOutcome::kAmbiguous:
      return LinkGenerationError::kContextExhausted;
    case LinkGenerationOutcome::kTimeout:
      return LinkGenerationError::kTimeout;
    case LinkGenerationOutcome::kExecutionFailed:
      return LinkGenerationError::kUnknown;
    case LinkGenerationOutcome::kSuccess:
      // kSuccess is not supposed to happen, as it is not an error.
      NOTREACHED();
      return LinkGenerationError::kUnknown;
  }
}

BOOL IsLinkGenerationTimeout(base::TimeDelta latency) {
  return latency >= kLinkGenerationTimeout;
}

}  // namespace link_to_text
