// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_PERSONAL_CONTEXT_RESOLVER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_PERSONAL_CONTEXT_RESOLVER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/personal_context/core/context_memory_error.h"

namespace accessibility_annotator {

// Interface for resolving data from `PersonalContextService`.
class PersonalContextResolver {
 public:
  using QueryCallback = base::OnceCallback<void(
      base::expected<std::vector<MemorySearchResult>,
                     personal_context::ContextMemoryError>)>;

  virtual ~PersonalContextResolver() = default;

  // Retrieves annotations from the `PersonalContextService` for a given query
  // and resolves it into `MemorySearchResult`s. Note: Calling this method while
  // a previous request is still in-flight cancels the previous request, and its
  // callback is immediately invoked with an empty result set.
  virtual void Query(std::u16string query, QueryCallback callback) = 0;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_PERSONAL_CONTEXT_RESOLVER_H_
