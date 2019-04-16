// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_BASE_TYPE_ID_H_
#define COBALT_BASE_TYPE_ID_H_

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"

// This file introduces the template function GetTypeId<T>() as well as the
// class TypeId.  GetTypeId<T>() will return a TypeId object that is unique
// to the type T passed as the template parameter.  This can be used to
// implement run-time type identification on certain types without actually
// enabling RTTI on the compiler.

// Some of this code is copied from gtest-internal.h.

namespace base {

namespace internal {

template <typename T>
class TypeIdHelper {
 public:
  // dummy_ must not have a const type.  Otherwise an overly eager
  // compiler (e.g. MSVC 7.1 & 8.0) may try to merge
  // TypeIdHelper<T>::dummy_ for different Ts as an "optimization".
  static bool dummy_;
};

// The compiler is required to allocate a different
// TypeIdHelper<T>::dummy_ variable for each T used to instantiate
// the template.  Therefore, the address of dummy_ is guaranteed to
// be unique.
template <typename T>
bool TypeIdHelper<T>::dummy_ = false;

}  // namespace internal

class TypeId {
 public:
  bool operator==(const TypeId& other) const { return value_ == other.value_; }
  bool operator!=(const TypeId& other) const { return !(*this == other); }
  bool operator<(const TypeId& other) const { return value_ < other.value_; }
  bool operator<=(const TypeId& other) const { return value_ <= other.value_; }
  bool operator>(const TypeId& other) const { return value_ > other.value_; }
  bool operator>=(const TypeId& other) const { return value_ >= other.value_; }

 private:
  explicit TypeId(intptr_t value) : value_(value) {}
  intptr_t value_;
  template <typename T>
  friend TypeId GetTypeId();
#if defined(BASE_HASH_USE_HASH_STRUCT)
  friend struct BASE_HASH_NAMESPACE::hash<TypeId>;
#else
  template <typename T, typename Predicate>
  friend class BASE_HASH_NAMESPACE::hash_compare;
#endif
};

// GetTypeId<T>() returns the ID of type T.  Different values will be
// returned for different types.  Calling the function twice with the
// same type argument is guaranteed to return the same ID.
template <typename T>
TypeId GetTypeId() {
  return TypeId(
      reinterpret_cast<intptr_t>(&(internal::TypeIdHelper<T>::dummy_)));
}

}  // namespace base

// Make TypeId usable as key in base::hash_map.

namespace BASE_HASH_NAMESPACE {

//
// GCC-flavored hash functor.
//
#if defined(BASE_HASH_USE_HASH_STRUCT)

// Forward declaration in case <hash_fun.h> is not #include'd.
template <>
struct hash<base::TypeId>;

template <>
struct hash<base::TypeId> {
  size_t operator()(const base::TypeId& key) const {
    return base_hash(key.value_);
  }

  hash<intptr_t> base_hash;
};

// TODO(Starboard) Migrate all platforms to use std::hash and deprecate these.
//
// Dinkumware-flavored hash functor.
//
#else

// Forward declaration in case <xhash> is not #include'd.
template <typename Key, typename Predicate>
class hash_compare;

template <typename Predicate>
class hash_compare<base::TypeId, Predicate> {
 public:
  typedef hash_compare<intptr_t> BaseHashCompare;

  enum {
    bucket_size = BaseHashCompare::bucket_size,
#if !SB_IS(COMPILER_MSVC)
    min_buckets = BaseHashCompare::min_buckets,
#endif
  };

  hash_compare() {}

  size_t operator()(const base::TypeId& key) const {
    return base_hash_compare_(key.value_);
  }

  bool operator()(const base::TypeId& lhs, const base::TypeId& rhs) const {
    return base_hash_compare_(lhs.value_, rhs.value_);
  }

 private:
  BaseHashCompare base_hash_compare_;
};

#endif
}  // namespace BASE_HASH_NAMESPACE

#endif  // COBALT_BASE_TYPE_ID_H_
