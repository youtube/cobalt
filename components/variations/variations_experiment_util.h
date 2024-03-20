// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_EXPERIMENT_UTIL_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_EXPERIMENT_UTIL_H_

#include <string>
#include "base/basictypes.h"
#include "base/strings/string_piece_forward.h"
namespace base {
class Time;
}

namespace variations {

extern const base::WStringPiece kExperimentLabelSeparator;

// Constructs a date string in the format understood by Google Update for the
// |current_time| plus one year.
std::u16string BuildExperimentDateString(const base::Time& current_time);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_EXPERIMENT_UTIL_H_
