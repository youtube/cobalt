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

#ifndef COBALT_SCRIPT_UNION_TYPE_INTERNAL_H_
#define COBALT_SCRIPT_UNION_TYPE_INTERNAL_H_

// Details of union type implementation. See union_type.h

#include <limits>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/type_id.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/sequence.h"

namespace cobalt {
namespace script {
namespace internal {

template <typename T>
struct UnionTypeDefaultTraits {
  typedef const T& ArgType;
  typedef T& ReturnType;
  typedef const T& ConstReturnType;
  static base::TypeId GetTypeID() { return base::GetTypeId<T>(); }
  static const bool is_boolean_type = false;
  static const bool is_interface_type = false;
  static const bool is_numeric_type = false;
  static const bool is_sequence_type = false;
  static const bool is_string_type = false;
  // e.g. ScriptValue<Uint8Array>* needs to be casted to
  // ScriptValue<ArrayBufferView>* manually.
  static const bool is_array_buffer_view_type = false;
  static const bool is_script_value_type = false;
  static const bool is_dictionary_type = false;
};

// Default traits for types with no specialization
template <typename T, typename = void>
struct UnionTypeTraits : UnionTypeDefaultTraits<T> {
  // This assert will only be evaluated if the template is instantiated.
  COMPILE_ASSERT(sizeof(T) == 0, UnionTypeNotImplementedForType);
};

template <typename T>
struct UnionTypeTraits<scoped_refptr<T> >
    : UnionTypeDefaultTraits<scoped_refptr<T> > {
  static base::TypeId GetTypeID() { return base::GetTypeId<T>(); }
  static const bool is_interface_type = true;
};

template <typename T>
struct UnionTypeTraits<
    T, typename std::enable_if<T::is_a_generated_dict::value>::type>
    : UnionTypeDefaultTraits<T> {
  static const bool is_dictionary_type = true;
};

template <>
struct UnionTypeTraits<Handle<ArrayBufferView>>
    : UnionTypeDefaultTraits<Handle<ArrayBufferView>> {
  static const bool is_array_buffer_view_type = true;
};

// script::Handle is always used to hold ScriptValues.
template <typename T>
struct UnionTypeTraits<Handle<T>> : UnionTypeDefaultTraits<Handle<T>> {
  static const bool is_script_value_type = true;
};

template <>
struct UnionTypeTraits<std::string> : UnionTypeDefaultTraits<std::string> {
  static const bool is_string_type = true;
};

// std::numeric_limits<T>::digits will be 0 for non-specialized types, 1 for
// bool, and something larger for the other types.
template <typename T>
struct UnionTypeTraits<
    T, typename base::enable_if<(std::numeric_limits<T>::digits > 1)>::type>
    : UnionTypeDefaultTraits<T> {
  typedef T ArgType;
  typedef T ReturnType;
  typedef T ConstReturnType;
  static const bool is_numeric_type = true;
};

template <>
struct UnionTypeTraits<bool> : UnionTypeDefaultTraits<bool> {
  typedef bool ArgType;
  typedef bool ReturnType;
  typedef bool ConstReturnType;
  static const bool is_boolean_type = true;
};

template <typename T>
struct UnionTypeTraits<Sequence<T>> : UnionTypeDefaultTraits<Sequence<T>> {
  typedef const Sequence<T>& ArgType;
  typedef Sequence<T> ReturnType;
  typedef const Sequence<T>& ConstReturnType;
  static const bool is_sequence_type = true;
};

// Explicitly blacklist nullable types. None of the union members should be
// nullable. If the union has a nullable member, then the whole union type
// should be declared nullable such as base::Optional<base::UnionTypeN<...> >
template <typename T>
struct UnionTypeTraits<base::Optional<T>> : UnionTypeDefaultTraits<T> {
  // This assert will only be evaluated if the template is instantiated.
  COMPILE_ASSERT(sizeof(T) == 0, NullableTypesAreNotPermittedInUnionss);
};

}  // namespace internal
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_UNION_TYPE_INTERNAL_H_
