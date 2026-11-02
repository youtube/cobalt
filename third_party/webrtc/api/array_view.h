/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ARRAY_VIEW_H_
#define API_ARRAY_VIEW_H_

#include <cstddef>
#include <span>
#include <type_traits>

namespace webrtc {

template <typename T, size_t extent = std::dynamic_extent>
using ArrayView = std::span<T, extent>;

// TODO: bugs.webrtc.org/439801349 - deprecate when unused by WebRTC
template <typename T>
inline ArrayView<T> MakeArrayView(T* data, size_t size) {
  return ArrayView<T>(data, size);
}

// Only for primitive types that have the same size and aligment.
// Allow reinterpret cast of the array view to another primitive type of the
// same size.
// Template arguments order is (U, T, Size) to allow deduction of the template
// arguments in client calls: reinterpret_array_view<target_type>(array_view).
template <typename U, typename T, size_t Size>
[[deprecated]] ArrayView<U, Size> reinterpret_array_view(
    ArrayView<T, Size> view) {
  static_assert(sizeof(U) == sizeof(T) && alignof(U) == alignof(T),
                "ArrayView reinterpret_cast is only supported for casting "
                "between views that represent the same chunk of memory.");
  static_assert(
      std::is_fundamental<T>::value && std::is_fundamental<U>::value,
      "ArrayView reinterpret_cast is only supported for casting between "
      "fundamental types.");
  return ArrayView<U, Size>(reinterpret_cast<U*>(view.data()), view.size());
}

}  //  namespace webrtc


#endif  // API_ARRAY_VIEW_H_
