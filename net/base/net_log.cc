// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/net_errors.h"

namespace net {

Value* NetLog::Source::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("type", static_cast<int>(type));
  dict->SetInteger("id", static_cast<int>(id));
  return dict;
}

// static
std::string NetLog::TickCountToString(const base::TimeTicks& time) {
  int64 delta_time = (time - base::TimeTicks()).InMilliseconds();
  return base::Int64ToString(delta_time);
}

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

// static
const char* NetLog::SourceTypeToString(SourceType source) {
  switch (source) {
#define SOURCE_TYPE(label, id) case id: return #label;
#include "net/base/net_log_source_type_list.h"
#undef SOURCE_TYPE
  }
  NOTREACHED();
  return NULL;
}

// static
const char* NetLog::EventPhaseToString(EventPhase phase) {
  switch (phase) {
    case PHASE_BEGIN:
      return "PHASE_BEGIN";
    case PHASE_END:
      return "PHASE_END";
    case PHASE_NONE:
      return "PHASE_NONE";
  }
  NOTREACHED();
  return NULL;
}

// static
Value* NetLog::EntryToDictionaryValue(net::NetLog::EventType type,
                                      const base::TimeTicks& time,
                                      const net::NetLog::Source& source,
                                      net::NetLog::EventPhase phase,
                                      net::NetLog::EventParameters* params,
                                      bool use_strings) {
  DictionaryValue* entry_dict = new DictionaryValue();

  entry_dict->SetString("time", TickCountToString(time));

  // Set the entry source.
  DictionaryValue* source_dict = new DictionaryValue();
  source_dict->SetInteger("id", source.id);
  if (!use_strings) {
    source_dict->SetInteger("type", static_cast<int>(source.type));
  } else {
    source_dict->SetString("type",
                           net::NetLog::SourceTypeToString(source.type));
  }
  entry_dict->Set("source", source_dict);

  // Set the event info.
  if (!use_strings) {
    entry_dict->SetInteger("type", static_cast<int>(type));
    entry_dict->SetInteger("phase", static_cast<int>(phase));
  } else {
    entry_dict->SetString("type", net::NetLog::EventTypeToString(type));
    entry_dict->SetString("phase", net::NetLog::EventPhaseToString(phase));
  }

  // Set the event-specific parameters.
  if (params)
    entry_dict->Set("params", params->ToValue());

  return entry_dict;
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

void BoundNetLog::EndEventWithNetErrorCode(NetLog::EventType event_type,
                                           int net_error) const {
  DCHECK_NE(net_error, net::ERR_IO_PENDING);
  if (net_error >= 0) {
    EndEvent(event_type, NULL);
  } else {
    EndEvent(
        event_type,
        make_scoped_refptr(new NetLogIntegerParameter("net_error", net_error)));
  }
}

NetLog::LogLevel BoundNetLog::GetLogLevel() const {
  if (net_log_)
    return net_log_->GetLogLevel();
  return NetLog::LOG_BASIC;
}

bool BoundNetLog::IsLoggingBytes() const {
  return GetLogLevel() == NetLog::LOG_ALL;
}

bool BoundNetLog::IsLoggingAllEvents() const {
  return GetLogLevel() <= NetLog::LOG_ALL_BUT_BYTES;
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

NetLogStringParameter::~NetLogStringParameter() {
}

Value* NetLogIntegerParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(name_, value_);
  return dict;
}

Value* NetLogStringParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(name_, value_);
  return dict;
}

Value* NetLogSourceParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->Set(name_, value_.ToValue());
  return dict;
}

ScopedNetLogEvent::ScopedNetLogEvent(
    const BoundNetLog& net_log,
    NetLog::EventType event_type,
    const scoped_refptr<NetLog::EventParameters>& params)
    : net_log_(net_log),
      event_type_(event_type) {
  net_log_.BeginEvent(event_type, params);
}

ScopedNetLogEvent::~ScopedNetLogEvent() {
  net_log_.EndEvent(event_type_, end_event_params_);
}

void ScopedNetLogEvent::SetEndEventParameters(
    const scoped_refptr<NetLog::EventParameters>& end_event_params) {
  DCHECK(!end_event_params_.get());
  end_event_params_ = end_event_params;
}

const BoundNetLog& ScopedNetLogEvent::net_log() const {
  return net_log_;
}

}  // namespace net
