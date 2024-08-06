// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_SERIALIZERS_BASE_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_SERIALIZERS_BASE_H_

#include <vector>

#include "base/values.h"
#include "third_party/chromium/media/base/media_export.h"

namespace media_m96 {

namespace internal {

// Serializer specializer struct.
// All the types that base::Value's constructor can take should be passed
// by non-const values. (int, bool, std::string, char*, etc).
template <typename T>
struct MediaSerializer {
  static inline base::Value Serialize(T value) { return base::Value(value); }
};

}  // namespace internal

template <typename T>
base::Value MediaSerialize(const T& t) {
  return internal::MediaSerializer<T>::Serialize(t);
}

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_SERIALIZERS_BASE_H_
