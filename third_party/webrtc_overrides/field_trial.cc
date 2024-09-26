// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

// Define webrtc::field_trial::FindFullName to provide webrtc with a field trial
// implementation.
namespace webrtc {
namespace field_trial {

std::string FindFullName(absl::string_view trial_name) {
  return base::FieldTrialList::FindFullName(
      base::StringPiece(trial_name.data(), trial_name.length()));
}

}  // namespace field_trial
}  // namespace webrtc
