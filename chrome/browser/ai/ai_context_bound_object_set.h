// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_SET_H_
#define CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_SET_H_

#include <variant>

#include "base/containers/unique_ptr_adapters.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ai/ai_context_bound_object.h"

// The data structure that supports adding and removing `AIContextBoundObject`.
class AIContextBoundObjectSet : public base::SupportsUserData::Data {
 public:
  AIContextBoundObjectSet();
  AIContextBoundObjectSet(const AIContextBoundObjectSet&) = delete;
  AIContextBoundObjectSet& operator=(const AIContextBoundObjectSet&) = delete;
  ~AIContextBoundObjectSet() override;

  // Add an `AIContextBoundObject` into the set.
  void AddContextBoundObject(std::unique_ptr<AIContextBoundObject> object);
  // Returns the size of user data set for testing purpose.
  size_t GetSizeForTesting();

  static AIContextBoundObjectSet* GetFromContext(
      base::SupportsUserData& context_user_data);

  // Remove the `AIContextBoundObject` from the set.
  void RemoveContextBoundObject(AIContextBoundObject* object);

 protected:
  base::flat_set<std::unique_ptr<AIContextBoundObject>,
                 base::UniquePtrComparator>
      context_bound_object_set_;
};

#endif  // CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_SET_H_
