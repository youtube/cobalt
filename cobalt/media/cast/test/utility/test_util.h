// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_CAST_TEST_UTILITY_TEST_UTIL_H_
#define COBALT_MEDIA_CAST_TEST_UTILITY_TEST_UTIL_H_

#include <string>
#include <vector>

#include "starboard/types.h"

namespace cobalt {
namespace media {
namespace cast {
namespace test {

class MeanAndError {
 public:
  MeanAndError() {}
  explicit MeanAndError(const std::vector<double>& values);
  std::string AsString() const;

  size_t num_values;
  double mean;
  double std_dev;
};

}  // namespace test
}  // namespace cast
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAST_TEST_UTILITY_TEST_UTIL_H_
