// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_LOG_UNITTEST_H_
#define NET_BASE_NET_LOG_UNITTEST_H_

#include <cstddef>
#include <vector>
#include "net/base/net_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Create a timestamp with internal value of |t| milliseconds from the epoch.
inline base::TimeTicks MakeTime(int t) {
  base::TimeTicks ticks;  // initialized to 0.
  ticks += base::TimeDelta::FromMilliseconds(t);
  return ticks;
}

inline ::testing::AssertionResult LogContainsEventHelper(
    const CapturingNetLog::EntryList& entries,
    int i,  // Negative indices are reverse indices.
    const base::TimeTicks& expected_time,
    bool check_time,
    NetLog::EventType expected_event,
    NetLog::EventPhase expected_phase) {
  // Negative indices are reverse indices.
  size_t j = (i < 0) ? entries.size() + i : i;
  if (j >= entries.size())
    return ::testing::AssertionFailure() << j << " is out of bounds.";
  const NetLog::Entry& entry = entries[j];
  if (entry.type != NetLog::Entry::TYPE_EVENT)
    return ::testing::AssertionFailure() << "Not a TYPE_EVENT entry";
  if (expected_event != entry.event.type) {
    return ::testing::AssertionFailure()
        << "Actual event: " << NetLog::EventTypeToString(entry.event.type)
        << ". Expected event: " << NetLog::EventTypeToString(expected_event)
        << ".";
  }
  if (expected_phase != entry.event.phase) {
    return ::testing::AssertionFailure()
        << "Actual phase: " << entry.event.phase
        << ". Expected phase: " << expected_phase << ".";
  }
  if (check_time) {
    if (expected_time != entry.time) {
      return ::testing::AssertionFailure()
          << "Actual time: " << entry.time.ToInternalValue()
          << ". Expected time: " << expected_time.ToInternalValue()
          << ".";
    }
  }
  return ::testing::AssertionSuccess();
}

inline ::testing::AssertionResult LogContainsEventAtTime(
    const CapturingNetLog::EntryList& log,
    int i,  // Negative indices are reverse indices.
    const base::TimeTicks& expected_time,
    NetLog::EventType expected_event,
    NetLog::EventPhase expected_phase) {
  return LogContainsEventHelper(log, i, expected_time, true,
                                expected_event, expected_phase);
}

// Version without timestamp.
inline ::testing::AssertionResult LogContainsEvent(
    const CapturingNetLog::EntryList& log,
    int i,  // Negative indices are reverse indices.
    NetLog::EventType expected_event,
    NetLog::EventPhase expected_phase) {
  return LogContainsEventHelper(log, i, base::TimeTicks(), false,
                                expected_event, expected_phase);
}

// Version for PHASE_BEGIN (and no timestamp).
inline ::testing::AssertionResult LogContainsBeginEvent(
    const CapturingNetLog::EntryList& log,
    int i,  // Negative indices are reverse indices.
    NetLog::EventType expected_event) {
  return LogContainsEvent(log, i, expected_event, NetLog::PHASE_BEGIN);
}

// Version for PHASE_END (and no timestamp).
inline ::testing::AssertionResult LogContainsEndEvent(
    const CapturingNetLog::EntryList& log,
    int i,  // Negative indices are reverse indices.
    NetLog::EventType expected_event) {
  return LogContainsEvent(log, i, expected_event, NetLog::PHASE_END);
}

inline ::testing::AssertionResult LogContainsEntryWithType(
    const CapturingNetLog::EntryList& entries,
    int i, // Negative indices are reverse indices.
    NetLog::Entry::Type type) {
  // Negative indices are reverse indices.
  size_t j = (i < 0) ? entries.size() + i : i;
  if (j >= entries.size())
    return ::testing::AssertionFailure() << j << " is out of bounds.";
  const NetLog::Entry& entry = entries[j];
  if (entry.type != type)
    return ::testing::AssertionFailure() << "Type does not match.";
  return ::testing::AssertionSuccess();
}


// Expect that the log contains an event, but don't care about where
// as long as the index where it is found is greater than min_index.
// Returns the position where the event was found.
inline size_t ExpectLogContainsSomewhere(
    const CapturingNetLog::EntryList& entries,
    size_t min_index,
    NetLog::EventType expected_event,
    NetLog::EventPhase expected_phase) {
  size_t i = 0;
  for (; i < entries.size(); ++i) {
    const NetLog::Entry& entry = entries[i];
    if (entry.type == NetLog::Entry::TYPE_EVENT &&
        entry.event.type == expected_event &&
        entry.event.phase == expected_phase)
      break;
  }
  EXPECT_LT(i, entries.size());
  EXPECT_GE(i, min_index);
  return i;
}

}  // namespace net

#endif  // NET_BASE_NET_LOG_UNITTEST_H_
