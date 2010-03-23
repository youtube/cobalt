// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log.h"
#include "base/logging.h"

namespace net {

// static
const char* NetLog::EventTypeToString(EventType event) {
  switch (event) {
#define EVENT_TYPE(label) case TYPE_ ## label: return #label;
#include "net/base/net_log_event_type_list.h"
#undef EVENT_TYPE
  }
  return NULL;
}

// static
std::vector<NetLog::EventType> NetLog::GetAllEventTypes() {
  std::vector<NetLog::EventType> types;
#define EVENT_TYPE(label) types.push_back(TYPE_ ## label);
#include "net/base/net_log_event_type_list.h"
#undef EVENT_TYPE
  return types;
}

void BoundNetLog::AddEntry(const NetLog::Entry& entry) const {
  if (net_log_)
    net_log_->AddEntry(entry);
}

bool BoundNetLog::HasListener() const {
  if (net_log_)
    return net_log_->HasListener();
  return false;
}

void BoundNetLog::AddEvent(NetLog::EventType event_type) const {
  if (net_log_) {
    NetLog::Entry entry;
    entry.source = source_;
    entry.type = NetLog::Entry::TYPE_EVENT;
    entry.time = base::TimeTicks::Now();
    entry.event = NetLog::Event(event_type, NetLog::PHASE_NONE);
    AddEntry(entry);
  }
}

void BoundNetLog::BeginEvent(NetLog::EventType event_type) const {
  if (net_log_) {
    NetLog::Entry entry;
    entry.source = source_;
    entry.type = NetLog::Entry::TYPE_EVENT;
    entry.time = base::TimeTicks::Now();
    entry.event = NetLog::Event(event_type, NetLog::PHASE_BEGIN);
    AddEntry(entry);
  }
}

void BoundNetLog::BeginEventWithString(NetLog::EventType event_type,
                                       const std::string& string) const {
  NetLog::Entry entry;
  entry.source = source_;
  entry.type = NetLog::Entry::TYPE_EVENT;
  entry.time = base::TimeTicks::Now();
  entry.event = NetLog::Event(event_type, NetLog::PHASE_BEGIN);
  entry.string = string;
  AddEntry(entry);
}

void BoundNetLog::AddEventWithInteger(NetLog::EventType event_type,
                                      int integer) const {
  NetLog::Entry entry;
  entry.source = source_;
  entry.type = NetLog::Entry::TYPE_EVENT;
  entry.time = base::TimeTicks::Now();
  entry.event = NetLog::Event(event_type, NetLog::PHASE_NONE);
  entry.error_code = integer;
  AddEntry(entry);
}

void BoundNetLog::EndEvent(NetLog::EventType event_type) const {
  if (net_log_) {
    NetLog::Entry entry;
    entry.source = source_;
    entry.type = NetLog::Entry::TYPE_EVENT;
    entry.time = base::TimeTicks::Now();
    entry.event = NetLog::Event(event_type, NetLog::PHASE_END);
    AddEntry(entry);
  }
}

void BoundNetLog::AddStringLiteral(const char* literal) const {
  if (net_log_) {
    NetLog::Entry entry;
    entry.source = source_;
    entry.type = NetLog::Entry::TYPE_STRING_LITERAL;
    entry.time = base::TimeTicks::Now();
    entry.literal = literal;
    AddEntry(entry);
  }
}

void BoundNetLog::AddString(const std::string& string) const {
  if (net_log_) {
    NetLog::Entry entry;
    entry.source = source_;
    entry.type = NetLog::Entry::TYPE_STRING;
    entry.time = base::TimeTicks::Now();
    entry.string = string;
    AddEntry(entry);
  }
}

void BoundNetLog::AddErrorCode(int error) const {
  if (net_log_) {
    NetLog::Entry entry;
    entry.source = source_;
    entry.type = NetLog::Entry::TYPE_ERROR_CODE;
    entry.time = base::TimeTicks::Now();
    entry.error_code = error;
    AddEntry(entry);
  }
}

// static
BoundNetLog BoundNetLog::Make(NetLog* net_log,
                              NetLog::SourceType source_type) {
  if (!net_log)
    return BoundNetLog();

  NetLog::Source source(source_type, net_log->NextID());
  return BoundNetLog(source, net_log);
}

void CapturingNetLog::AddEntry(const Entry& entry) {
  if (entries_.size() + 1 < max_num_entries_)
    entries_.push_back(entry);
}

int CapturingNetLog::NextID() {
  return next_id_++;
}

void CapturingNetLog::Clear() {
  entries_.clear();
}

void CapturingBoundNetLog::Clear() {
  capturing_net_log_->Clear();
}

void CapturingBoundNetLog::AppendTo(const BoundNetLog& net_log) const {
  for (size_t i = 0; i < entries().size(); ++i) {
    NetLog::Entry entry = entries()[i];
    entry.source = net_log.source();
    net_log.AddEntry(entry);
  }
}

}  // namespace net
