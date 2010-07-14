// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"

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

void BoundNetLog::AddEntry(
    NetLog::EventType type,
    NetLog::EventPhase phase,
    const scoped_refptr<NetLog::EventParameters>& params) const {
  if (net_log_) {
    net_log_->AddEntry(type, base::TimeTicks::Now(), source_, phase, params);
  }
}

void BoundNetLog::AddEntryWithTime(
    NetLog::EventType type,
    const base::TimeTicks& time,
    NetLog::EventPhase phase,
    const scoped_refptr<NetLog::EventParameters>& params) const {
  if (net_log_) {
    net_log_->AddEntry(type, time, source_, phase, params);
  }
}

bool BoundNetLog::HasListener() const {
  if (net_log_)
    return net_log_->HasListener();
  return false;
}

void BoundNetLog::AddEvent(
    NetLog::EventType event_type,
    const scoped_refptr<NetLog::EventParameters>& params) const {
  AddEntry(event_type, NetLog::PHASE_NONE, params);
}

void BoundNetLog::BeginEvent(
    NetLog::EventType event_type,
    const scoped_refptr<NetLog::EventParameters>& params) const {
  AddEntry(event_type, NetLog::PHASE_BEGIN, params);
}

void BoundNetLog::EndEvent(
    NetLog::EventType event_type,
    const scoped_refptr<NetLog::EventParameters>& params) const {
  AddEntry(event_type, NetLog::PHASE_END, params);
}

// static
BoundNetLog BoundNetLog::Make(NetLog* net_log,
                              NetLog::SourceType source_type) {
  if (!net_log)
    return BoundNetLog();

  NetLog::Source source(source_type, net_log->NextID());
  return BoundNetLog(source, net_log);
}

NetLogStringParameter::NetLogStringParameter(const char* name,
                                             const std::string& value)
    : name_(name), value_(value) {
}

Value* NetLogIntegerParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(ASCIIToWide(name_), value_);
  return dict;
}

Value* NetLogStringParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(ASCIIToWide(name_), value_);
  return dict;
}

Value* NetLogSourceParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  DictionaryValue* source_dict = new DictionaryValue();
  source_dict->SetInteger(L"type", static_cast<int>(value_.type));
  source_dict->SetInteger(L"id", static_cast<int>(value_.id));

  dict->Set(ASCIIToWide(name_), source_dict);
  return dict;
}

}  // namespace net
