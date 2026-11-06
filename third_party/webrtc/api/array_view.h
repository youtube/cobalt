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

namespace webrtc {

template <typename T, size_t extent = std::dynamic_extent>
using ArrayView = std::span<T, extent>;

// TODO: bugs.webrtc.org/439801349 - deprecate when unused by WebRTC
template <typename T>
inline ArrayView<T> MakeArrayView(T* data, size_t size) {
  return ArrayView<T>(data, size);
}

}  //  namespace webrtc


#endif  // API_ARRAY_VIEW_H_
