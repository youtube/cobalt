// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_AT_MEMORY_QUERY_SERVICE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_AT_MEMORY_QUERY_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/personal_context_resolver.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"
#include "components/accessibility_annotator/core/at_memory_query_service_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/context_memory_error.h"

namespace optimization_guide {
class RemoteModelExecutor;
}

namespace accessibility_annotator {

class MemoryDataProvider;

// Service for querying @memory suggestions.
class AtMemoryQueryService : public KeyedService {
 public:
  AtMemoryQueryService(
      std::unique_ptr<AtMemoryQueryServiceDelegate> delegate,
      std::unique_ptr<MemoryDataProvider> data_provider,
      std::unique_ptr<PersonalContextResolver> personal_context_resolver,
      optimization_guide::RemoteModelExecutor* remote_model_executor);
  AtMemoryQueryService(const AtMemoryQueryService&) = delete;
  AtMemoryQueryService& operator=(const AtMemoryQueryService&) = delete;
  ~AtMemoryQueryService() override;

  // KeyedService:
  void Shutdown() override;

  // Executes a query and returns suggestions via `update_callback`.
  // @param query The search string provided by the user.
  // @param update_callback Invoked with search results. May be called multiple
  // times for streaming updates, providing results from different data sources.
  virtual void Query(
      std::u16string_view query,
      base::RepeatingCallback<void(MemorySearchResults)> update_callback);

 private:
  void OnClassificationComplete(
      std::u16string query,
      base::RepeatingCallback<void(MemorySearchResults)> update_callback,
      ClassifiedQuery classified_query);

  void OnDataRetrieved(
      std::u16string query,
      ClassifiedQuery classified_query,
      base::RepeatingCallback<void(MemorySearchResults)> update_callback,
      std::vector<MemorySearchResult> entries);

  // Queries the `PersonalContextResolver` for remote results.
  // `filtered_local_entries` are matching local results to be merged with
  // the remote results on success. `fallback_local_entries` are to be returned
  // if the remote query finds no matches (can be the unfiltered local entries
  // as a last resort if absolutely no keyword matches were found).
  void QueryPersonalContextResolver(
      std::u16string query,
      ClassifiedQuery classified_query,
      base::RepeatingCallback<void(MemorySearchResults)> update_callback,
      std::vector<MemorySearchResult> filtered_local_entries,
      std::vector<MemorySearchResult> fallback_local_entries);

  void OnPersonalContextResolverComplete(
      ClassifiedQuery classified_query,
      base::RepeatingCallback<void(MemorySearchResults)> update_callback,
      std::vector<MemorySearchResult> filtered_local_entries,
      std::vector<MemorySearchResult> fallback_local_entries,
      base::expected<std::vector<MemorySearchResult>,
                     personal_context::ContextMemoryError>
          personal_context_entries);

  std::unique_ptr<AtMemoryQueryServiceDelegate> delegate_;
  std::unique_ptr<MemoryDataProvider> data_provider_;
  std::unique_ptr<PersonalContextResolver> personal_context_resolver_;
  QueryClassifier classifier_;

  base::WeakPtrFactory<AtMemoryQueryService> weak_ptr_factory_{this};
  base::WeakPtrFactory<AtMemoryQueryService> personal_context_weak_ptr_factory_{
      this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_AT_MEMORY_QUERY_SERVICE_H_
