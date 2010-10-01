// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/capturing_net_log.h"

namespace net {

CapturingNetLog::Entry::Entry(EventType type,
                              const base::TimeTicks& time,
                              Source source,
                              EventPhase phase,
                              EventParameters* extra_parameters)
    : type(type), time(time), source(source), phase(phase),
      extra_parameters(extra_parameters) {
}

CapturingNetLog::Entry::~Entry() {}

CapturingNetLog::CapturingNetLog(size_t max_num_entries)
    : next_id_(0), max_num_entries_(max_num_entries) {
}

CapturingNetLog::~CapturingNetLog() {}

void CapturingNetLog::AddEntry(EventType type,
                               const base::TimeTicks& time,
                               const Source& source,
                               EventPhase phase,
                               EventParameters* extra_parameters) {
  Entry entry(type, time, source, phase, extra_parameters);
  if (entries_.size() + 1 < max_num_entries_)
    entries_.push_back(entry);
}

uint32 CapturingNetLog::NextID() {
  return next_id_++;
}

void CapturingNetLog::Clear() {
  entries_.clear();
}

CapturingBoundNetLog::CapturingBoundNetLog(const NetLog::Source& source,
                                           CapturingNetLog* net_log)
    : source_(source), capturing_net_log_(net_log) {
}

CapturingBoundNetLog::CapturingBoundNetLog(size_t max_num_entries)
    : capturing_net_log_(new CapturingNetLog(max_num_entries)) {}

CapturingBoundNetLog::~CapturingBoundNetLog() {}

void CapturingBoundNetLog::Clear() {
  capturing_net_log_->Clear();
}

void CapturingBoundNetLog::AppendTo(const BoundNetLog& net_log) const {
  for (size_t i = 0; i < entries().size(); ++i) {
    const CapturingNetLog::Entry& entry = entries()[i];
    net_log.AddEntryWithTime(entry.type, entry.time, entry.phase,
                             entry.extra_parameters);
  }
}

}  // namespace net
