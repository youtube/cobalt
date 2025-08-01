/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/importers/json/json_utils.h"

#include "perfetto/base/build_config.h"

#include <limits>

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
#include <json/reader.h>
#include "perfetto/ext/base/string_utils.h"
#endif

namespace perfetto {
namespace trace_processor {
namespace json {

bool IsJsonSupported() {
#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  return true;
#else
  return false;
#endif
}

std::optional<int64_t> CoerceToTs(const Json::Value& value) {
  PERFETTO_DCHECK(IsJsonSupported());

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  switch (static_cast<size_t>(value.type())) {
    case Json::realValue:
      return static_cast<int64_t>(value.asDouble() * 1000.0);
    case Json::uintValue:
    case Json::intValue:
      return value.asInt64() * 1000;
    case Json::stringValue:
      return CoerceToTs(value.asString());
    default:
      return std::nullopt;
  }
#else
  perfetto::base::ignore_result(value);
  return std::nullopt;
#endif
}

std::optional<int64_t> CoerceToTs(const std::string& s) {
  PERFETTO_DCHECK(IsJsonSupported());

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  size_t lhs_end = std::min<size_t>(s.find('.'), s.size());
  size_t rhs_start = std::min<size_t>(lhs_end + 1, s.size());
  std::optional<int64_t> lhs = base::StringToInt64(s.substr(0, lhs_end));
  std::optional<double> rhs =
      base::StringToDouble("0." + s.substr(rhs_start, std::string::npos));
  if ((!lhs.has_value() && lhs_end > 0) ||
      (!rhs.has_value() && rhs_start < s.size())) {
    return std::nullopt;
  }
  return lhs.value_or(0) * 1000 +
         static_cast<int64_t>(rhs.value_or(0) * 1000.0);
#else
  perfetto::base::ignore_result(s);
  return std::nullopt;
#endif
}

std::optional<int64_t> CoerceToInt64(const Json::Value& value) {
  PERFETTO_DCHECK(IsJsonSupported());

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  switch (static_cast<size_t>(value.type())) {
    case Json::realValue:
    case Json::uintValue:
      return static_cast<int64_t>(value.asUInt64());
    case Json::intValue:
      return value.asInt64();
    case Json::stringValue: {
      std::string s = value.asString();
      char* end;
      int64_t n = strtoll(s.c_str(), &end, 10);
      if (end != s.data() + s.size())
        return std::nullopt;
      return n;
    }
    default:
      return std::nullopt;
  }
#else
  perfetto::base::ignore_result(value);
  return std::nullopt;
#endif
}

std::optional<uint32_t> CoerceToUint32(const Json::Value& value) {
  PERFETTO_DCHECK(IsJsonSupported());

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  std::optional<int64_t> result = CoerceToInt64(value);
  if (!result.has_value())
    return std::nullopt;
  int64_t n = result.value();
  if (n < 0 || n > std::numeric_limits<uint32_t>::max())
    return std::nullopt;
  return static_cast<uint32_t>(n);
#else
  perfetto::base::ignore_result(value);
  return std::nullopt;
#endif
}

std::optional<Json::Value> ParseJsonString(base::StringView raw_string) {
  PERFETTO_DCHECK(IsJsonSupported());

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  Json::CharReaderBuilder b;
  auto reader = std::unique_ptr<Json::CharReader>(b.newCharReader());

  Json::Value value;
  const char* begin = raw_string.data();
  return reader->parse(begin, begin + raw_string.size(), &value, nullptr)
             ? std::make_optional(std::move(value))
             : std::nullopt;
#else
  perfetto::base::ignore_result(raw_string);
  return std::nullopt;
#endif
}

bool AddJsonValueToArgs(const Json::Value& value,
                        base::StringView flat_key,
                        base::StringView key,
                        TraceStorage* storage,
                        ArgsTracker::BoundInserter* inserter) {
  PERFETTO_DCHECK(IsJsonSupported());

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  if (value.isObject()) {
    auto it = value.begin();
    bool inserted = false;
    for (; it != value.end(); ++it) {
      std::string child_name = it.name();
      std::string child_flat_key = flat_key.ToStdString() + "." + child_name;
      std::string child_key = key.ToStdString() + "." + child_name;
      inserted |=
          AddJsonValueToArgs(*it, base::StringView(child_flat_key),
                             base::StringView(child_key), storage, inserter);
    }
    return inserted;
  }

  if (value.isArray()) {
    auto it = value.begin();
    bool inserted_any = false;
    std::string array_key = key.ToStdString();
    StringId array_key_id = storage->InternString(key);
    for (; it != value.end(); ++it) {
      size_t array_index = inserter->GetNextArrayEntryIndex(array_key_id);
      std::string child_key =
          array_key + "[" + std::to_string(array_index) + "]";
      bool inserted = AddJsonValueToArgs(
          *it, flat_key, base::StringView(child_key), storage, inserter);
      if (inserted)
        inserter->IncrementArrayEntryIndex(array_key_id);
      inserted_any |= inserted;
    }
    return inserted_any;
  }

  // Leaf value.
  auto flat_key_id = storage->InternString(flat_key);
  auto key_id = storage->InternString(key);

  switch (value.type()) {
    case Json::ValueType::nullValue:
      break;
    case Json::ValueType::intValue:
      inserter->AddArg(flat_key_id, key_id, Variadic::Integer(value.asInt64()));
      return true;
    case Json::ValueType::uintValue:
      inserter->AddArg(flat_key_id, key_id,
                       Variadic::UnsignedInteger(value.asUInt64()));
      return true;
    case Json::ValueType::realValue:
      inserter->AddArg(flat_key_id, key_id, Variadic::Real(value.asDouble()));
      return true;
    case Json::ValueType::stringValue:
      inserter->AddArg(flat_key_id, key_id,
                       Variadic::String(storage->InternString(
                           base::StringView(value.asString()))));
      return true;
    case Json::ValueType::booleanValue:
      inserter->AddArg(flat_key_id, key_id, Variadic::Boolean(value.asBool()));
      return true;
    case Json::ValueType::objectValue:
    case Json::ValueType::arrayValue:
      PERFETTO_FATAL("Non-leaf types handled above");
      break;
  }
  return false;
#else
  perfetto::base::ignore_result(value);
  perfetto::base::ignore_result(flat_key);
  perfetto::base::ignore_result(key);
  perfetto::base::ignore_result(storage);
  perfetto::base::ignore_result(inserter);
  return false;
#endif
}

}  // namespace json
}  // namespace trace_processor
}  // namespace perfetto
