// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/base/audio_point.h"

#include <stddef.h>

namespace media_m96 {

std::string PointsToString(const std::vector<Point>& points) {
  std::string points_string;
  if (!points.empty()) {
    for (size_t i = 0; i < points.size() - 1; ++i) {
      points_string.append(points[i].ToString());
      points_string.append(", ");
    }
    points_string.append(points.back().ToString());
  }
  return points_string;
}

}  // namespace media_m96
