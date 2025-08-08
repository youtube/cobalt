// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_UTIL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_UTIL_H_

#include <sstream>
#include <string>

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"

namespace performance_manager {

class Node;

// Converts a Mojo enum to a human-readable string.
template <typename T>
std::string MojoEnumToString(T value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

// Returns a human-friendly string value computed from |time_ticks|. That string
// represents the time delta between |time_ticks| and TimeTicks::Now() in the
// following format: "x hours, y minutes".
base::Value TimeDeltaFromNowToValue(base::TimeTicks time_ticks);

// Converts a string to a base::Value, where null strings go to a null value
// instead of an empty string.
base::Value MaybeNullStringToValue(base::StringPiece str);

base::Value PriorityAndReasonToValue(
    const execution_context_priority::PriorityAndReason& priority_and_reason);

// Returns `node`'s description as described by its primary data describer as
// a formatted string.
std::string DumpNodeDescription(const Node* node);

// Returns the output of all registered data describers for `node` as a
// formatted string.
std::string DumpRegisteredDescribers(const Node* node);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_UTIL_H_
