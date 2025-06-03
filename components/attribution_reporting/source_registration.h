// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_config.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
void RecordSourceRegistrationError(mojom::SourceRegistrationError);

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) SourceRegistration {
  // Doesn't log metric on parsing failures.
  static base::expected<SourceRegistration, mojom::SourceRegistrationError>
      Parse(base::Value::Dict, mojom::SourceType);

  // Logs metric on parsing failures.
  static base::expected<SourceRegistration, mojom::SourceRegistrationError>
  Parse(base::StringPiece json, mojom::SourceType);

  explicit SourceRegistration(DestinationSet);

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  explicit SourceRegistration(mojo::DefaultConstruct::Tag);

  ~SourceRegistration();

  SourceRegistration(const SourceRegistration&);
  SourceRegistration& operator=(const SourceRegistration&);

  SourceRegistration(SourceRegistration&&);
  SourceRegistration& operator=(SourceRegistration&&);

  base::Value::Dict ToJson() const;

  bool IsValid() const;
  bool IsValidForSourceType(mojom::SourceType) const;

  uint64_t source_event_id = 0;
  DestinationSet destination_set;
  // These `base::TimeDelta`s must be non-negative if set. This is verified by
  // the `Parse()` and `IsValid()` methods.
  base::TimeDelta expiry = kMaxSourceExpiry;
  EventReportWindows event_report_windows;
  base::TimeDelta aggregatable_report_window = expiry;
  // Must be non-negative and <= `kMaxSettableEventLevelAttributions`.
  // This is verified by the `Parse()` and `IsValid()` methods.
  int max_event_level_reports = 0;
  int64_t priority = 0;
  FilterData filter_data;
  absl::optional<uint64_t> debug_key;
  AggregationKeys aggregation_keys;
  bool debug_reporting = false;
  TriggerConfig trigger_config;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_REGISTRATION_H_
