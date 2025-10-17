// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_EVENT_TRIGGER_DATA_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_EVENT_TRIGGER_DATA_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace base {
class DictValue;
class Value;
}  // namespace base

namespace attribution_reporting {

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) EventTriggerData {
  static base::expected<EventTriggerData, mojom::TriggerRegistrationError>
  FromJSON(base::Value& value);

  // Data associated with trigger.
  // Will be sanitized to a lower entropy by the `AttributionStorageDelegate`
  // before storage.
  uint64_t data = 0;

  // Priority specified in conversion redirect. Used to prioritize which
  // reports to send among multiple different reports for the same attribution
  // source. Defaults to 0 if not provided.
  int64_t priority = 0;

  // Key specified in conversion redirect for deduplication against existing
  // conversions with the same source. If absent, no deduplication is
  // performed.
  std::optional<uint64_t> dedup_key;

  // The filters used to determine whether this `EventTriggerData'`s fields
  // are used.
  FilterPair filters;

  EventTriggerData();

  EventTriggerData(uint64_t data,
                   int64_t priority,
                   std::optional<uint64_t> dedup_key,
                   FilterPair);

  base::DictValue ToJson() const;

  friend bool operator==(const EventTriggerData&,
                         const EventTriggerData&) = default;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_EVENT_TRIGGER_DATA_H_
