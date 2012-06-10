// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/capturing_net_log.h"

#include "base/logging.h"
#include "base/values.h"

namespace net {

CapturingNetLog::CapturedEntry::CapturedEntry(
    EventType type,
    const base::TimeTicks& time,
    Source source,
    EventPhase phase,
    scoped_ptr<DictionaryValue> params)
    : type(type),
      time(time),
      source(source),
      phase(phase),
      params(params.Pass()) {
}

CapturingNetLog::CapturedEntry::CapturedEntry(const CapturedEntry& entry) {
  *this = entry;
}

CapturingNetLog::CapturedEntry::~CapturedEntry() {}

CapturingNetLog::CapturedEntry&
CapturingNetLog::CapturedEntry::operator=(const CapturedEntry& entry) {
  type = entry.type;
  time = entry.time;
  source = entry.source;
  phase = entry.phase;
  params.reset(entry.params.get() ? entry.params->DeepCopy() : NULL);
  return *this;
}

bool CapturingNetLog::CapturedEntry::GetStringValue(
    const std::string& name,
    std::string* value) const {
  if (!params.get())
    return false;
  return params->GetString(name, value);
}

bool CapturingNetLog::CapturedEntry::GetIntegerValue(
    const std::string& name,
    int* value) const {
  if (!params.get())
    return false;
  return params->GetInteger(name, value);
}

bool CapturingNetLog::CapturedEntry::GetNetErrorCode(int* value) const {
  return GetIntegerValue("net_error", value);
}

CapturingNetLog::CapturingNetLog()
    : last_id_(0),
      log_level_(LOG_ALL_BUT_BYTES) {
}

CapturingNetLog::~CapturingNetLog() {}

void CapturingNetLog::GetEntries(CapturedEntryList* entry_list) const {
  base::AutoLock lock(lock_);
  *entry_list = captured_entries_;
}

void CapturingNetLog::Clear() {
  base::AutoLock lock(lock_);
  captured_entries_.clear();
}

void CapturingNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  base::AutoLock lock(lock_);
  log_level_ = log_level;
}

void CapturingNetLog::OnAddEntry(const net::NetLog::Entry& entry) {
  // Only BoundNetLogs without a NetLog should have an invalid source.
  DCHECK(entry.source().is_valid());

  // Using Dictionaries instead of Values makes checking values a little
  // simpler.
  DictionaryValue* param_dict = NULL;
  Value* param_value = entry.ParametersToValue();
  if (param_value && !param_value->GetAsDictionary(&param_dict))
    delete param_value;

  // Only need to acquire the lock when accessing class variables.
  base::AutoLock lock(lock_);
  captured_entries_.push_back(
      CapturedEntry(entry.type(),
                    base::TimeTicks::Now(),
                    entry.source(),
                    entry.phase(),
                    scoped_ptr<DictionaryValue>(param_dict)));
}

uint32 CapturingNetLog::NextID() {
  return base::subtle::NoBarrier_AtomicIncrement(&last_id_, 1);
}

NetLog::LogLevel CapturingNetLog::GetLogLevel() const {
  base::AutoLock lock(lock_);
  return log_level_;
}

void CapturingNetLog::AddThreadSafeObserver(
    NetLog::ThreadSafeObserver* observer,
    NetLog::LogLevel log_level) {
  NOTIMPLEMENTED() << "Not currently used by net unit tests.";
}

void CapturingNetLog::SetObserverLogLevel(ThreadSafeObserver* observer,
                                          LogLevel log_level) {
  NOTIMPLEMENTED() << "Not currently used by net unit tests.";
}

void CapturingNetLog::RemoveThreadSafeObserver(
    NetLog::ThreadSafeObserver* observer) {
  NOTIMPLEMENTED() << "Not currently used by net unit tests.";
}

CapturingBoundNetLog::CapturingBoundNetLog()
    : net_log_(BoundNetLog::Make(&capturing_net_log_,
                                 net::NetLog::SOURCE_NONE)) {
}

CapturingBoundNetLog::~CapturingBoundNetLog() {}

void CapturingBoundNetLog::GetEntries(
    CapturingNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_.GetEntries(entry_list);
}

void CapturingBoundNetLog::Clear() {
  capturing_net_log_.Clear();
}

void CapturingBoundNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  capturing_net_log_.SetLogLevel(log_level);
}

}  // namespace net
