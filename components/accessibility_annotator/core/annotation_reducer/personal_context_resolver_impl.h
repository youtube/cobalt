// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_PERSONAL_CONTEXT_RESOLVER_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_PERSONAL_CONTEXT_RESOLVER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/accessibility_annotator/core/annotation_reducer/personal_context_resolver.h"
#include "components/personal_context/core/personal_context_service.h"

namespace accessibility_annotator {

class PersonalContextResolverImpl : public PersonalContextResolver {
 public:
  PersonalContextResolverImpl(
      personal_context::PersonalContextService* personal_context_service,
      const std::string& locale);
  PersonalContextResolverImpl(const PersonalContextResolverImpl&) = delete;
  PersonalContextResolverImpl& operator=(const PersonalContextResolverImpl&) =
      delete;
  ~PersonalContextResolverImpl() override;

  // PersonalContextResolver:
  void Query(std::u16string query, QueryCallback callback) override;

 private:
  void OnPersonalContextRetrieved(personal_context::FetchContextResult result);

  raw_ptr<personal_context::PersonalContextService> personal_context_service_ =
      nullptr;

  std::string locale_;

  // Callback for the currently active query request.
  QueryCallback in_flight_query_callback_;

  base::WeakPtrFactory<PersonalContextResolverImpl> weak_ptr_factory_{this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_PERSONAL_CONTEXT_RESOLVER_IMPL_H_
