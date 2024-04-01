// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/bucket_utility.h"

#include <algorithm>
#include <cmath>

namespace {

// TODO(chcunningham): Find some authoritative list of frame rates.
// Framerates in this list go way beyond typical values to account for changes
// to playback rate.
const int kFrameRateBuckets[] = {5,   10,  20,  25,  30,  40,  50,   60,
                                 70,  80,  90,  100, 120, 150, 200,  250,
                                 300, 350, 400, 450, 500, 550, 600,  650,
                                 700, 750, 800, 850, 900, 950, 1000, 1500};

// A mix of width and height dimensions for common and not-so-common resolutions
// spanning 50p -> 12K.
// TODO(chcunningham): Ponder these a bit more.
const int kSizeBuckets[] = {
    50,   100,  144,  240,  256,  280,  360,  426,  480,   640,   720,
    854,  960,  1080, 1280, 1440, 1920, 2160, 2560, 2880,  3160,  3840,
    4128, 4320, 5120, 6144, 7360, 7680, 8000, 9000, 10000, 11000, 11520};

}  //  namespace

namespace media {

gfx::Size GetSizeBucket(const gfx::Size& raw_size) {
  // If either dimension is less than 75% of the min size bucket, return an
  // empty size. Empty |natural_size_| will signal ShouldBeReporting() to return
  // false.
  const double kMinSizeBucketPercent = .75;
  if (raw_size.width() < kMinSizeBucketPercent * kSizeBuckets[0] ||
      raw_size.height() < kMinSizeBucketPercent * kSizeBuckets[0]) {
    return gfx::Size();
  }

  // Round width and height to first bucket >= |raw_size| dimensions. See
  // explanation in header file.
  const int* width_bound = std::lower_bound(
      std::begin(kSizeBuckets), std::end(kSizeBuckets), raw_size.width());
  const int* height_bound = std::lower_bound(
      std::begin(kSizeBuckets), std::end(kSizeBuckets), raw_size.height());

  // If no bucket is larger than the raw dimension, just use the last bucket.
  if (width_bound == std::end(kSizeBuckets))
    --width_bound;
  if (height_bound == std::end(kSizeBuckets))
    --height_bound;

  return gfx::Size(*width_bound, *height_bound);
}

int GetFpsBucket(double raw_fps) {
  int rounded_fps = std::round(raw_fps);

  // Find the first bucket that is strictly > than |rounded_fps|.
  const int* upper_bound =
      std::upper_bound(std::begin(kFrameRateBuckets),
                       std::end(kFrameRateBuckets), std::round(rounded_fps));

  // If no bucket is larger than |rounded_fps|, just used the last bucket;
  if (upper_bound == std::end(kFrameRateBuckets))
    return *(upper_bound - 1);

  // Return early if its the first bucket.
  if (upper_bound == std::begin(kFrameRateBuckets))
    return *upper_bound;

  int higher_bucket = *upper_bound;
  int previous_bucket = *(upper_bound - 1);
  if (std::abs(previous_bucket - rounded_fps) <
      std::abs(higher_bucket - rounded_fps)) {
    return previous_bucket;
  }

  return higher_bucket;
}

}  // namespace media