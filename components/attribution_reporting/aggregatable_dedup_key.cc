// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_dedup_key.h"

#include <stdint.h>

#include <utility>

#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

}  // namespace

// static
base::expected<AggregatableDedupKey, TriggerRegistrationError>
AggregatableDedupKey::FromJSON(base::Value& value) {
  base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableDedupKeyWrongType);
  }

  auto filters = FilterPair::FromJSON(*dict);
  if (!filters.has_value()) {
    return base::unexpected(filters.error());
  }

  absl::optional<uint64_t> dedup_key = ParseDeduplicationKey(*dict);

  return AggregatableDedupKey(dedup_key, std::move(*filters));
}

AggregatableDedupKey::AggregatableDedupKey() = default;

AggregatableDedupKey::AggregatableDedupKey(absl::optional<uint64_t> dedup_key,
                                           FilterPair filters)
    : dedup_key(dedup_key), filters(std::move(filters)) {}

base::Value::Dict AggregatableDedupKey::ToJson() const {
  base::Value::Dict dict;

  filters.SerializeIfNotEmpty(dict);

  SerializeDeduplicationKey(dict, dedup_key);

  return dict;
}

}  // namespace attribution_reporting
