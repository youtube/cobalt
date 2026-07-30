// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/at_memory_query_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/i18n/break_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/personal_context_resolver.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "net/base/network_change_notifier.h"

namespace accessibility_annotator {

namespace {

// Tokenizes `text` using native word boundaries and returns true if any
// token exists in `filter_words_set`.
bool TextContainsAnyFilterWord(
    std::u16string_view text,
    const base::flat_set<std::u16string>& filter_words_set) {
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return false;
  }

  while (iter.Advance()) {
    if (iter.IsWord()) {
      std::u16string word = base::ToLowerASCII(iter.GetString());
      if (filter_words_set.contains(word)) {
        return true;
      }
    }
  }
  return false;
}

// Returns true if at least one word in `filter_words_set` is present in
// `entry.value` or any of its `metadata_list` values.
bool EntryMatchesAnyFilterWord(
    const MemorySearchResult& entry,
    const base::flat_set<std::u16string>& filter_words_set) {
  if (TextContainsAnyFilterWord(entry.value, filter_words_set)) {
    return true;
  }
  return std::ranges::any_of(
      entry.metadata_list, [&](const EntryMetadata& metadata) {
        return TextContainsAnyFilterWord(metadata.value, filter_words_set);
      });
}

// Deduplicates search results in `MemorySearchResults`.
// An entry is considered a duplicate if its `type`, `value` and its
// `metadata_list` are identical to an entry already in the unique set.
// The first occurrence of a duplicate entry is preserved, maintaining its
// relative order and other fields (like confidence_score). The `sources` of
// subsequent duplicates are merged into the preserved entry.
void DeduplicateResults(std::vector<MemorySearchResult>& results) {
  std::vector<MemorySearchResult> unique_results;
  unique_results.reserve(results.size());
  for (MemorySearchResult& result : results) {
    auto it = std::ranges::find_if(
        unique_results, [&result](const MemorySearchResult& existing) {
          return existing.type == result.type &&
                 existing.value == result.value &&
                 existing.metadata_list == result.metadata_list;
        });
    if (it != unique_results.end()) {
      for (MemoryEntrySource& source : result.sources) {
        if (!std::ranges::contains(it->sources, source)) {
          it->sources.push_back(std::move(source));
        }
      }
    } else {
      unique_results.push_back(std::move(result));
    }
  }
  results = std::move(unique_results);
}

std::vector<MemorySearchResult> FilterResults(
    const std::vector<MemorySearchResult>& entries,
    const base::flat_set<std::u16string>& filter_words) {
  if (filter_words.empty()) {
    return entries;
  }
  std::vector<MemorySearchResult> filtered_entries;
  filtered_entries.reserve(entries.size());
  std::ranges::copy_if(entries, std::back_inserter(filtered_entries),
                       [&](const MemorySearchResult& entry) {
                         return EntryMatchesAnyFilterWord(entry, filter_words);
                       });
  return filtered_entries;
}

MemorySearchStatus MapContextMemoryError(
    personal_context::ContextMemoryError::ExecutionError error) {
  switch (error) {
    case personal_context::ContextMemoryError::ExecutionError::
        kPermissionDenied:
    case personal_context::ContextMemoryError::ExecutionError::
        kRequestThrottled:
    case personal_context::ContextMemoryError::ExecutionError::kRetryableError:
    case personal_context::ContextMemoryError::ExecutionError::
        kNonRetryableError:
    case personal_context::ContextMemoryError::ExecutionError::kCancelled:
    case personal_context::ContextMemoryError::ExecutionError::
        kResponseParseError:
    case personal_context::ContextMemoryError::ExecutionError::kInvalidRequest:
    case personal_context::ContextMemoryError::ExecutionError::kGenericFailure:
    case personal_context::ContextMemoryError::ExecutionError::kUnknown:
      return MemorySearchStatus::kInternalFailure;
  }
}

// For debugging purposes only. Runs a debug query that directly retrieves
// local suggestions via `data_provider`, bypassing query classification and
// remote resolution.
void QueryPersonalContextDebug(
    MemoryDataProvider* data_provider,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback) {
  data_provider->RetrieveAll(
      {static_cast<MemoryDataType>(
          personal_context::features::debug::kMockPersonalContextResultTypeParam
              .Get())},
      base::BindOnce(
          [](base::RepeatingCallback<void(MemorySearchResults)> update_cb,
             std::vector<MemorySearchResult> results) {
            DeduplicateResults(results);
            update_cb.Run(MemorySearchResults(
                MemorySearchStatus::kFinalResponseSuccess, std::move(results)));
          },
          std::move(update_callback)));
}

}  // namespace

AtMemoryQueryService::AtMemoryQueryService(
    std::unique_ptr<AtMemoryQueryServiceDelegate> delegate,
    std::unique_ptr<MemoryDataProvider> data_provider,
    std::unique_ptr<PersonalContextResolver> personal_context_resolver,
    optimization_guide::RemoteModelExecutor* remote_model_executor)
    : delegate_(std::move(delegate)),
      data_provider_(std::move(data_provider)),
      personal_context_resolver_(std::move(personal_context_resolver)),
      classifier_(CreateQueryClassifier(remote_model_executor)) {}

AtMemoryQueryService::~AtMemoryQueryService() = default;

void AtMemoryQueryService::Shutdown() {
  data_provider_.reset();
  personal_context_resolver_.reset();
}

void AtMemoryQueryService::Query(
    std::u16string_view query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback) {
  // Invalidate any in-flight queries.
  weak_ptr_factory_.InvalidateWeakPtrs();
  personal_context_weak_ptr_factory_.InvalidateWeakPtrs();

  if (net::NetworkChangeNotifier::IsOffline()) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kNoConnectionFailure));
    return;
  }

  // We can't query if we don't have any data providers configured.
  if (!data_provider_) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kInternalFailure));
    return;
  }

  if (base::FeatureList::IsEnabled(
          personal_context::features::debug::kMockPersonalContextResult)) {
    QueryPersonalContextDebug(data_provider_.get(), std::move(update_callback));
    return;
  }

  // Run the query classifier to understand the user's intent, extracting
  // intent type and filter words.
  // TODO(crbug.com/524177036): The `PersonalContextResolver` query should
  // return the AutofillFetchPlan, which then should be used to access the data
  // from `AutofillDataProvider`.
  classifier_.Run(
      std::u16string(query),
      base::BindOnce(&AtMemoryQueryService::OnClassificationComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::u16string(query),
                     std::move(update_callback)));
}

void AtMemoryQueryService::OnClassificationComplete(
    std::u16string query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    ClassifiedQuery classified_query) {
  // If the classifier couldn't figure out what the user is asking for, we try
  // the personal context resolver as a fallback.
  if (classified_query.intent == MemoryDataType::kUnknown) {
    QueryPersonalContextResolver(std::move(query), classified_query,
                                 update_callback, /*filtered_local_entries=*/{},
                                 /*fallback_local_entries=*/{});
    return;
  }

  MemoryDataType intent = classified_query.intent;

  auto callback = base::BindOnce(
      &AtMemoryQueryService::OnDataRetrieved, weak_ptr_factory_.GetWeakPtr(),
      std::move(query), std::move(classified_query), update_callback);

  auto log_and_call_retrieved = base::BindOnce(
      [](base::OnceCallback<void(std::vector<MemorySearchResult>)> callback,
         std::vector<MemorySearchResult> results) {
        base::UmaHistogramCounts1000(
            "AccessibilityAnnotator.AtMemoryQueryService."
            "ProviderResultCount.AutofillDataProvider",
            results.size());
        std::move(callback).Run(std::move(results));
      },
      std::move(callback));

  data_provider_->RetrieveAll({intent}, std::move(log_and_call_retrieved));
}

void AtMemoryQueryService::OnDataRetrieved(
    std::u16string query,
    ClassifiedQuery classified_query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<MemorySearchResult> entries) {
  DeduplicateResults(entries);
  std::vector<MemorySearchResult> filtered_entries =
      FilterResults(entries, classified_query.filter_words);

  if (!personal_context_resolver_) {
    update_callback.Run(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess,
        filtered_entries.empty() ? std::move(entries)
                                 : std::move(filtered_entries)));
    return;
  }

  if (filtered_entries.empty()) {
    QueryPersonalContextResolver(std::move(query), std::move(classified_query),
                                 update_callback,
                                 /*filtered_local_entries=*/{},
                                 /*fallback_local_entries=*/std::move(entries));
    return;
  }

  // Report the matching local results as a partial response.
  update_callback.Run(MemorySearchResults(
      MemorySearchStatus::kPartialResponseSuccess, filtered_entries));

  // Query the personal context resolver in the background. If it finds matches,
  // they will be merged with the filtered local entries. Otherwise, we fallback
  // to the filtered local entries.
  std::vector<MemorySearchResult> fallback_local_entries = filtered_entries;
  QueryPersonalContextResolver(std::move(query), classified_query,
                               update_callback, std::move(filtered_entries),
                               std::move(fallback_local_entries));
}

void AtMemoryQueryService::QueryPersonalContextResolver(
    std::u16string query,
    ClassifiedQuery classified_query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<MemorySearchResult> filtered_local_entries,
    std::vector<MemorySearchResult> fallback_local_entries) {
  // Invalidate any in-flight personal context queries.
  personal_context_weak_ptr_factory_.InvalidateWeakPtrs();

  if (!personal_context_resolver_) {
    update_callback.Run(
        MemorySearchResults(classified_query.intent == MemoryDataType::kUnknown
                                ? MemorySearchStatus::kUnsupportedQuery
                                : MemorySearchStatus::kFinalResponseSuccess,
                            std::move(fallback_local_entries)));
    return;
  }

  personal_context_resolver_->Query(
      query,
      base::BindOnce(&AtMemoryQueryService::OnPersonalContextResolverComplete,
                     personal_context_weak_ptr_factory_.GetWeakPtr(),
                     std::move(classified_query), update_callback,
                     std::move(filtered_local_entries),
                     std::move(fallback_local_entries)));
}

void AtMemoryQueryService::OnPersonalContextResolverComplete(
    ClassifiedQuery classified_query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<MemorySearchResult> filtered_local_entries,
    std::vector<MemorySearchResult> fallback_local_entries,
    base::expected<std::vector<MemorySearchResult>,
                   personal_context::ContextMemoryError>
        personal_context_entries) {
  if (!personal_context_entries.has_value()) {
    if (personal_context_entries.error().error() ==
        personal_context::ContextMemoryError::ExecutionError::kCancelled) {
      return;
    }
    update_callback.Run(MemorySearchResults(
        MapContextMemoryError(personal_context_entries.error().error()),
        std::move(fallback_local_entries)));
    return;
  }

  std::vector<MemorySearchResult> filtered_personal_context_entries =
      FilterResults(personal_context_entries.value(),
                    classified_query.filter_words);

  // If the query completed successfully but returned no additional results
  // after filtering, return the fallback local entries with the final status.
  if (filtered_personal_context_entries.empty()) {
    update_callback.Run(
        MemorySearchResults(classified_query.intent == MemoryDataType::kUnknown
                                ? MemorySearchStatus::kUnsupportedQuery
                                : MemorySearchStatus::kFinalResponseSuccess,
                            std::move(fallback_local_entries)));
    return;
  }

  // In order to avoid extra allocations, remote results are merged into the
  // `filtered_local_entries` vector.
  base::Extend(filtered_local_entries,
               std::move(filtered_personal_context_entries));
  DeduplicateResults(filtered_local_entries);
  update_callback.Run(
      MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                          std::move(filtered_local_entries)));
}

}  // namespace accessibility_annotator
