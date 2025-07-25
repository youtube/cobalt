// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CLONE_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CLONE_TRAITS_H_

#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

template <typename T, typename SFINAE = void>
struct HasCloneMethod : std ::false_type {
  static_assert(sizeof(T), "T must be a complete type.");
};

template <typename T>
struct HasCloneMethod<T,
                      std::void_t<decltype(std::declval<const T&>().Clone())>>
    : std::true_type {};

template <typename T>
T Clone(const T& input);

template <typename T>
struct CloneTraits {
  static T Clone(const T& input) {
    if constexpr (HasCloneMethod<T>::value) {
      return input.Clone();
    } else {
      return input;
    }
  }
};

template <typename T>
struct CloneTraits<absl::optional<T>> {
  static absl::optional<T> Clone(const absl::optional<T>& input) {
    if (!input)
      return absl::nullopt;

    return absl::optional<T>(mojo::Clone(*input));
  }
};

template <typename T>
struct CloneTraits<std::vector<T>> {
  static std::vector<T> Clone(const std::vector<T>& input) {
    std::vector<T> result;
    result.reserve(input.size());
    for (const auto& element : input)
      result.push_back(mojo::Clone(element));

    return result;
  }
};

template <typename K, typename V>
struct CloneTraits<base::flat_map<K, V>> {
  static base::flat_map<K, V> Clone(const base::flat_map<K, V>& input) {
    base::flat_map<K, V> result;
    for (const auto& element : input) {
      result.insert(std::make_pair(mojo::Clone(element.first),
                                   mojo::Clone(element.second)));
    }
    return result;
  }
};

template <typename T>
T Clone(const T& input) {
  return CloneTraits<T>::Clone(input);
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CLONE_TRAITS_H_
