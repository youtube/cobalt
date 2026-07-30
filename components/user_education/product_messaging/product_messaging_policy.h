// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_POLICY_H_
#define COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_POLICY_H_

#include "base/containers/flat_set.h"
#include "components/user_education/product_messaging/product_messaging_types.h"

namespace user_education {

// Provides a policy for how product messages interact with each other.
class ProductMessagingPolicy {
 public:
  ProductMessagingPolicy() = default;
  ProductMessagingPolicy(const ProductMessagingPolicy&) = delete;
  void operator=(const ProductMessagingPolicy&) = delete;
  virtual ~ProductMessagingPolicy() = default;

  using Ids = base::flat_set<internal::ProductMessageUniqueId>;

  // Returns whether `key` is blocked by any of the keys in `others`, including
  // itself.
  virtual bool BlockedByAnyOf(ProductMessageKey key,
                              const Ids& others,
                              bool include_self) const = 0;

  // Returns whether `key` must always show after `other`.
  virtual bool MustShowAfterAnyOf(ProductMessageKey key,
                                  const Ids& others) const = 0;

  // Describes the relationship between two types.
  enum TypeRelationship {
    // Type A can show regardless of whether B is showing.
    kIndependentOf,
    // Type A and B can show in any order but no at the same time.
    kEquivalentTo,
    // Type A must wait until all promos of type B are done.
    kSupersededBy,
  };
  virtual TypeRelationship GetRelationship(
      ProductMessageType type,
      ProductMessageType other_type) const = 0;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_POLICY_H_
