// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_trigger_data.h"

#include <stdint.h>

#include <utility>

#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kTriggerData[] = "trigger_data";

}  // namespace

// static
base::expected<EventTriggerData, TriggerRegistrationError>
EventTriggerData::FromJSON(base::Value& value) {
  base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kEventTriggerDataWrongType);
  }

  auto filters = FilterPair::FromJSON(*dict);
  if (!filters.has_value())
    return base::unexpected(filters.error());

  uint64_t data = ParseUint64(*dict, kTriggerData).value_or(0);
  int64_t priority = ParsePriority(*dict);
  absl::optional<uint64_t> dedup_key = ParseDeduplicationKey(*dict);

  return EventTriggerData(data, priority, dedup_key, std::move(*filters));
}

EventTriggerData::EventTriggerData() = default;

EventTriggerData::EventTriggerData(uint64_t data,
                                   int64_t priority,
                                   absl::optional<uint64_t> dedup_key,
                                   FilterPair filters)
    : data(data),
      priority(priority),
      dedup_key(dedup_key),
      filters(std::move(filters)) {}

base::Value::Dict EventTriggerData::ToJson() const {
  base::Value::Dict dict;

  filters.SerializeIfNotEmpty(dict);

  SerializeUint64(dict, kTriggerData, data);
  SerializePriority(dict, priority);
  SerializeDeduplicationKey(dict, dedup_key);

  return dict;
}

}  // namespace attribution_reporting
