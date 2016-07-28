// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_TIMESTAMP_HELPER_H_
#define MEDIA_BASE_AUDIO_TIMESTAMP_HELPER_H_

#include "base/time.h"
#include "media/base/media_export.h"

namespace media {

// Generates timestamps for a sequence of audio sample bytes. This class should
// be used any place timestamps need to be calculated for a sequence of audio
// samples. It helps avoid timestamps inaccuracies caused by rounding/truncation
// in repeated sample count to timestamp conversions.
//
// The class is constructed with bytes per frame and samples_per_second
// information so that it can convert audio sample byte counts into timestamps.
// After the object is constructed, SetBaseTimestamp() must be called to specify
// the starting timestamp of the audio sequence. As audio samples are received,
// their byte counts are added to AddBytes(). These byte counts are
// accumulated by this class so GetTimestamp() can be used to determine the
// timestamp for the samples that have been added. GetDuration() calculates
// the proper duration values for samples added to the current timestamp.
// GetBytesToTarget() determines the number of bytes that need to be
// added/removed from the accumulated bytes to reach a target timestamp.
class MEDIA_EXPORT AudioTimestampHelper {
 public:
  AudioTimestampHelper(int bytes_per_frame, int samples_per_second);

  // Sets the base timestamp to |base_timestamp| and the sets count to 0.
  void SetBaseTimestamp(base::TimeDelta base_timestamp);

  base::TimeDelta base_timestamp() const;

  // Adds sample bytes to the frame counter.
  //
  // Note: SetBaseTimestamp() must be called with a value other than
  // kNoTimestamp() before this method can be called.
  void AddBytes(int byte_count);

  // Get the current timestamp. This value is computed from the base_timestamp()
  // and the number of sample bytes that have been added so far.
  base::TimeDelta GetTimestamp() const;

  // Gets the duration if |byte_count| bytes were added to the current
  // timestamp reported by GetTimestamp(). This method ensures that
  // (GetTimestamp() + GetDuration(n)) will equal the timestamp that
  // GetTimestamp() will return if AddBytes(n) is called.
  base::TimeDelta GetDuration(int byte_count) const;

  // Returns the number of bytes needed to reach the target timestamp.
  //
  // Note: |target| must be >= |base_timestamp_|.
  int64 GetBytesToTarget(base::TimeDelta target) const;

 private:
  base::TimeDelta ComputeTimestamp(int64 frame_count) const;

  int bytes_per_frame_;
  double microseconds_per_frame_;

  base::TimeDelta base_timestamp_;

  // Number of frames accumulated by byte counts passed to AddBytes() calls.
  int64 frame_count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioTimestampHelper);
};

}  // namespace media

#endif
