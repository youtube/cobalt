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
    : last_id_(-1),
      max_num_entries_(max_num_entries),
      log_level_(LOG_ALL_BUT_BYTES) {
}

CapturingNetLog::~CapturingNetLog() {}

void CapturingNetLog::GetEntries(EntryList* entry_list) const {
  base::AutoLock lock(lock_);
  *entry_list = entries_;
}

void CapturingNetLog::Clear() {
  base::AutoLock lock(lock_);
  entries_.clear();
}

void CapturingNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  base::AutoLock lock(lock_);
  log_level_ = log_level;
}

void CapturingNetLog::AddEntry(EventType type,
                               const base::TimeTicks& time,
                               const Source& source,
                               EventPhase phase,
                               EventParameters* extra_parameters) {
  base::AutoLock lock(lock_);
  Entry entry(type, time, source, phase, extra_parameters);
  if (entries_.size() + 1 < max_num_entries_)
    entries_.push_back(entry);
}

uint32 CapturingNetLog::NextID() {
  return base::subtle::NoBarrier_AtomicIncrement(&last_id_, 1);
}

NetLog::LogLevel CapturingNetLog::GetLogLevel() const {
  base::AutoLock lock(lock_);
  return log_level_;
}

CapturingBoundNetLog::CapturingBoundNetLog(const NetLog::Source& source,
                                           CapturingNetLog* net_log)
    : source_(source), capturing_net_log_(net_log) {
}

CapturingBoundNetLog::CapturingBoundNetLog(size_t max_num_entries)
    : capturing_net_log_(new CapturingNetLog(max_num_entries)) {}

CapturingBoundNetLog::~CapturingBoundNetLog() {}

void CapturingBoundNetLog::GetEntries(
    CapturingNetLog::EntryList* entry_list) const {
  capturing_net_log_->GetEntries(entry_list);
}

void CapturingBoundNetLog::Clear() {
  capturing_net_log_->Clear();
}

void CapturingBoundNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  capturing_net_log_->SetLogLevel(log_level);
}

}  // namespace net
