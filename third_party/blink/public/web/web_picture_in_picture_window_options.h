// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PICTURE_IN_PICTURE_WINDOW_OPTIONS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PICTURE_IN_PICTURE_WINDOW_OPTIONS_H_

#include <cstdint>

namespace blink {

struct WebPictureInPictureWindowOptions {
  uint64_t width = 0;
  uint64_t height = 0;
  double initial_aspect_ratio = 0.0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PICTURE_IN_PICTURE_WINDOW_OPTIONS_H_
