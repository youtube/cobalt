// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// Returns parameters for logging data transferred events. Includes number of
// bytes transferred and, if the log level indicates bytes should be logged and
// |byte_count| > 0, the bytes themselves.  The bytes are hex-encoded, since
// base::StringValue only supports UTF-8.
Value* BytesTransferredCallback(int byte_count,
                                const char* bytes,
                                NetLog::LogLevel log_level) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("byte_count", byte_count);
  if (NetLog::IsLoggingBytes(log_level) && byte_count > 0)
    dict->SetString("hex_encoded_bytes", base::HexEncode(bytes, byte_count));
  return dict;
}

Value* SourceEventParametersCallback(const NetLog::Source source,
                                     NetLog::LogLevel /* log_level */) {
  if (!source.IsValid())
    return NULL;
  DictionaryValue* event_params = new DictionaryValue();
  source.AddToEventParameters(event_params);
  return event_params;
}

Value* NetLogIntegerCallback(const char* name,
                             int value,
                             NetLog::LogLevel /* log_level */) {
  DictionaryValue* event_params = new DictionaryValue();
  event_params->SetInteger(name, value);
  return event_params;
}

Value* NetLogInt64Callback(const char* name,
                           int64 value,
                           NetLog::LogLevel /* log_level */) {
  DictionaryValue* event_params = new DictionaryValue();
  event_params->SetString(name, base::Int64ToString(value));
  return event_params;
}

Value* NetLogStringCallback(const char* name,
                            const std::string* value,
                            NetLog::LogLevel /* log_level */) {
  DictionaryValue* event_params = new DictionaryValue();
  event_params->SetString(name, *value);
  return event_params;
}

Value* NetLogString16Callback(const char* name,
                              const string16* value,
                              NetLog::LogLevel /* log_level */) {
  DictionaryValue* event_params = new DictionaryValue();
  event_params->SetString(name, *value);
  return event_params;
}

}  // namespace

const uint32 NetLog::Source::kInvalidId = 0;

NetLog::Source::Source() : type(SOURCE_NONE), id(kInvalidId) {
}

NetLog::Source::Source(SourceType type, uint32 id) : type(type), id(id) {
}

bool NetLog::Source::IsValid() const {
  return id != kInvalidId;
}

void NetLog::Source::AddToEventParameters(DictionaryValue* event_params) const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("type", static_cast<int>(type));
  dict->SetInteger("id", static_cast<int>(id));
  event_params->Set("source_dependency", dict);
}

NetLog::ParametersCallback NetLog::Source::ToEventParametersCallback() const {
  return base::Bind(&SourceEventParametersCallback, *this);
}

// static
bool NetLog::Source::FromEventParameters(Value* event_params, Source* source) {
  DictionaryValue* dict;
  DictionaryValue* source_dict;
  int source_id;
  int source_type;
  if (!event_params ||
      !event_params->GetAsDictionary(&dict) ||
      !dict->GetDictionary("source_dependency", &source_dict) ||
      !source_dict->GetInteger("id", &source_id) ||
      !source_dict->GetInteger("type", &source_type)) {
    *source = Source();
    return false;
  }

  DCHECK_LE(0, source_id);
  DCHECK_LT(source_type, NetLog::SOURCE_COUNT);
  *source = Source(static_cast<SourceType>(source_type), source_id);
  return true;
}

Value* NetLog::Entry::ToValue() const {
  DictionaryValue* entry_dict(new DictionaryValue());

  entry_dict->SetString("time", TickCountToString(time_));

  // Set the entry source.
  DictionaryValue* source_dict = new DictionaryValue();
  source_dict->SetInteger("id", source_.id);
  source_dict->SetInteger("type", static_cast<int>(source_.type));
  entry_dict->Set("source", source_dict);

  // Set the event info.
  entry_dict->SetInteger("type", static_cast<int>(type_));
  entry_dict->SetInteger("phase", static_cast<int>(phase_));

  // Set the event-specific parameters.
  if (parameters_callback_) {
    Value* value = parameters_callback_->Run(log_level_);
    if (value)
      entry_dict->Set("params", value);
  }

  return entry_dict;
}

Value* NetLog::Entry::ParametersToValue() const {
  if (parameters_callback_)
    return parameters_callback_->Run(log_level_);
  return NULL;
}

NetLog::Entry::Entry(
    EventType type,
    Source source,
    EventPhase phase,
    base::TimeTicks time,
    const ParametersCallback* parameters_callback,
    LogLevel log_level)
    : type_(type),
      source_(source),
      phase_(phase),
      time_(time),
      parameters_callback_(parameters_callback),
      log_level_(log_level) {
};

NetLog::Entry::~Entry() {
}

NetLog::ThreadSafeObserver::ThreadSafeObserver() : log_level_(LOG_BASIC),
                                                   net_log_(NULL) {
}

NetLog::ThreadSafeObserver::~ThreadSafeObserver() {
  // Make sure we aren't watching a NetLog on destruction.  Because the NetLog
  // may pass events to each observer on multiple threads, we cannot safely
  // stop watching a NetLog automatically from a parent class.
  DCHECK(!net_log_);
}

NetLog::LogLevel NetLog::ThreadSafeObserver::log_level() const {
  DCHECK(net_log_);
  return log_level_;
}

NetLog* NetLog::ThreadSafeObserver::net_log() const {
  return net_log_;
}

void NetLog::AddGlobalEntry(EventType type) {
  AddEntry(type,
           Source(net::NetLog::SOURCE_NONE, NextID()),
           net::NetLog::PHASE_NONE,
           NULL);
}

void NetLog::AddGlobalEntry(
    EventType type,
    const NetLog::ParametersCallback& parameters_callback) {
  AddEntry(type,
           Source(net::NetLog::SOURCE_NONE, NextID()),
           net::NetLog::PHASE_NONE,
           &parameters_callback);
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
    default:
      NOTREACHED();
      return NULL;
  }
}

// static
base::Value* NetLog::GetEventTypesAsValue() {
  DictionaryValue* dict = new DictionaryValue();
  for (int i = 0; i < EVENT_COUNT; ++i) {
    dict->SetInteger(EventTypeToString(static_cast<EventType>(i)), i);
  }
  return dict;
}

// static
const char* NetLog::SourceTypeToString(SourceType source) {
  switch (source) {
#define SOURCE_TYPE(label) case SOURCE_ ## label: return #label;
#include "net/base/net_log_source_type_list.h"
#undef SOURCE_TYPE
    default:
      NOTREACHED();
      return NULL;
  }
}

// static
base::Value* NetLog::GetSourceTypesAsValue() {
  DictionaryValue* dict = new DictionaryValue();
  for (int i = 0; i < SOURCE_COUNT; ++i) {
    dict->SetInteger(SourceTypeToString(static_cast<SourceType>(i)), i);
  }
  return dict;
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
bool NetLog::IsLoggingBytes(LogLevel log_level) {
  return log_level == NetLog::LOG_ALL;
}

// static
bool NetLog::IsLoggingAllEvents(LogLevel log_level) {
  return log_level <= NetLog::LOG_ALL_BUT_BYTES;
}

// static
NetLog::ParametersCallback NetLog::IntegerCallback(const char* name,
                                                   int value) {
  return base::Bind(&NetLogIntegerCallback, name, value);
}

// static
NetLog::ParametersCallback NetLog::Int64Callback(const char* name,
                                                 int64 value) {
  return base::Bind(&NetLogInt64Callback, name, value);
}

// static
NetLog::ParametersCallback NetLog::StringCallback(const char* name,
                                                  const std::string* value) {
  DCHECK(value);
  return base::Bind(&NetLogStringCallback, name, value);
}

// static
NetLog::ParametersCallback NetLog::StringCallback(const char* name,
                                                  const string16* value) {
  DCHECK(value);
  return base::Bind(&NetLogString16Callback, name, value);
}

void NetLog::OnAddObserver(ThreadSafeObserver* observer, LogLevel log_level) {
  DCHECK(!observer->net_log_);
  observer->net_log_ = this;
  observer->log_level_ = log_level;
}

void NetLog::OnSetObserverLogLevel(ThreadSafeObserver* observer,
                                   LogLevel log_level) {
  DCHECK_EQ(this, observer->net_log_);
  observer->log_level_ = log_level;
}

void NetLog::OnRemoveObserver(ThreadSafeObserver* observer) {
  DCHECK_EQ(this, observer->net_log_);
  observer->net_log_ = NULL;
}

void NetLog::AddEntry(EventType type,
                      const Source& source,
                      EventPhase phase,
                      const NetLog::ParametersCallback* parameters_callback) {
  Entry entry(type, source, phase, base::TimeTicks::Now(),
              parameters_callback, GetLogLevel());
  OnAddEntry(entry);
}

void BoundNetLog::AddEntry(NetLog::EventType type,
                           NetLog::EventPhase phase) const {
  if (!net_log_)
    return;
  net_log_->AddEntry(type, source_, phase, NULL);
}

void BoundNetLog::AddEntry(
    NetLog::EventType type,
    NetLog::EventPhase phase,
    const NetLog::ParametersCallback& get_parameters) const {
  if (!net_log_)
    return;
  net_log_->AddEntry(type, source_, phase, &get_parameters);
}

void BoundNetLog::AddEvent(NetLog::EventType type) const {
  AddEntry(type, NetLog::PHASE_NONE);
}

void BoundNetLog::AddEvent(
    NetLog::EventType type,
    const NetLog::ParametersCallback& get_parameters) const {
  AddEntry(type, NetLog::PHASE_NONE, get_parameters);
}

void BoundNetLog::BeginEvent(NetLog::EventType type) const {
  AddEntry(type, NetLog::PHASE_BEGIN);
}

void BoundNetLog::BeginEvent(
    NetLog::EventType type,
    const NetLog::ParametersCallback& get_parameters) const {
  AddEntry(type, NetLog::PHASE_BEGIN, get_parameters);
}

void BoundNetLog::EndEvent(NetLog::EventType type) const {
  AddEntry(type, NetLog::PHASE_END);
}

void BoundNetLog::EndEvent(
    NetLog::EventType type,
    const NetLog::ParametersCallback& get_parameters) const {
  AddEntry(type, NetLog::PHASE_END, get_parameters);
}

void BoundNetLog::AddEventWithNetErrorCode(NetLog::EventType event_type,
                                           int net_error) const {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  if (net_error >= 0) {
    AddEvent(event_type);
  } else {
    AddEvent(event_type, NetLog::IntegerCallback("net_error", net_error));
  }
}

void BoundNetLog::EndEventWithNetErrorCode(NetLog::EventType event_type,
                                           int net_error) const {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  if (net_error >= 0) {
    EndEvent(event_type);
  } else {
    EndEvent(event_type, NetLog::IntegerCallback("net_error", net_error));
  }
}

void BoundNetLog::AddByteTransferEvent(NetLog::EventType event_type,
                                       int byte_count,
                                       const char* bytes) const {
  AddEvent(event_type, base::Bind(BytesTransferredCallback, byte_count, bytes));
}

NetLog::LogLevel BoundNetLog::GetLogLevel() const {
  if (net_log_)
    return net_log_->GetLogLevel();
  return NetLog::LOG_BASIC;
}

bool BoundNetLog::IsLoggingBytes() const {
  return NetLog::IsLoggingBytes(GetLogLevel());
}

bool BoundNetLog::IsLoggingAllEvents() const {
  return NetLog::IsLoggingAllEvents(GetLogLevel());
}

// static
BoundNetLog BoundNetLog::Make(NetLog* net_log,
                              NetLog::SourceType source_type) {
  if (!net_log)
    return BoundNetLog();

  NetLog::Source source(source_type, net_log->NextID());
  return BoundNetLog(source, net_log);
}

}  // namespace net
