// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

bool IsValidSharedStorageURLsArrayLength(size_t length) {
  return length != 0 &&
         length <=
             features::kSharedStorageURLSelectionOperationInputURLSizeLimit
                 .Get();
}

void LogSharedStorageWorkletError(SharedStorageWorkletErrorType error_type) {
  base::UmaHistogramEnumeration("Storage.SharedStorage.Worklet.Error.Type",
                                error_type);
}

void LogSharedStorageSelectURLBudgetStatus(
    SharedStorageSelectUrlBudgetStatus budget_status) {
  base::UmaHistogramEnumeration(
      "Storage.SharedStorage.Worklet.SelectURL.BudgetStatus", budget_status);
}

bool ShouldDefinePrivateAggregationInSharedStorage() {
  return base::FeatureList::IsEnabled(
             blink::features::kPrivateAggregationApi) &&
         blink::features::kPrivateAggregationApiEnabledInSharedStorage.Get();
}

bool IsValidPrivateAggregationContextId(std::string_view context_id) {
  return context_id.size() <= blink::kPrivateAggregationApiContextIdMaxLength &&
         base::IsStringUTF8AllowingNoncharacters(context_id);
}

bool IsValidPrivateAggregationFilteringIdMaxBytes(
    size_t filtering_id_max_bytes) {
  return filtering_id_max_bytes > 0 &&
         filtering_id_max_bytes <= kPrivateAggregationApiMaxFilteringIdMaxBytes;
}

}  // namespace blink
