// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_splicer.h"

#include <cstdlib>

#include "base/logging.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/buffers.h"
#include "media/base/data_buffer.h"

namespace media {

// Largest gap or overlap allowed by this class. Anything
// larger than this will trigger an error.
// This is an arbitrary value, but the initial selection of 50ms
// roughly represents the duration of 2 compressed AAC or MP3 frames.
static const int kMaxTimeDeltaInMilliseconds = 50;

AudioSplicer::AudioSplicer(int bytes_per_frame, int samples_per_second)
    : output_timestamp_helper_(bytes_per_frame, samples_per_second),
      min_gap_size_(2 * bytes_per_frame),
      received_end_of_stream_(false) {
}

AudioSplicer::~AudioSplicer() {
}

void AudioSplicer::Reset() {
  output_timestamp_helper_.SetBaseTimestamp(kNoTimestamp());
  output_buffers_.clear();
  received_end_of_stream_ = false;
}

bool AudioSplicer::AddInput(const scoped_refptr<Buffer>& input){
  DCHECK(!received_end_of_stream_ || input->IsEndOfStream());

  if (input->IsEndOfStream()) {
    output_buffers_.push_back(input);
    received_end_of_stream_ = true;
    return true;
  }

  DCHECK(input->GetTimestamp() != kNoTimestamp());
  DCHECK(input->GetDuration() > base::TimeDelta());
  DCHECK_GT(input->GetDataSize(), 0);

  if (output_timestamp_helper_.base_timestamp() == kNoTimestamp())
    output_timestamp_helper_.SetBaseTimestamp(input->GetTimestamp());

  if (output_timestamp_helper_.base_timestamp() > input->GetTimestamp()) {
    DVLOG(1) << "Input timestamp is before the base timestamp.";
    return false;
  }

  base::TimeDelta timestamp = input->GetTimestamp();
  base::TimeDelta expected_timestamp = output_timestamp_helper_.GetTimestamp();
  base::TimeDelta delta = timestamp - expected_timestamp;

  if (std::abs(delta.InMilliseconds()) > kMaxTimeDeltaInMilliseconds) {
    DVLOG(1) << "Timestamp delta too large: " << delta.InMicroseconds() << "us";
    return false;
  }

  int bytes_to_fill = 0;
  if (delta != base::TimeDelta())
    bytes_to_fill = output_timestamp_helper_.GetBytesToTarget(timestamp);

  if (bytes_to_fill == 0 || std::abs(bytes_to_fill) < min_gap_size_) {
    AddOutputBuffer(input);
    return true;
  }

  if (bytes_to_fill > 0) {
    DVLOG(1) << "Gap detected @ " << expected_timestamp.InMicroseconds()
             << " us: " << delta.InMicroseconds() << " us";

    // Create a buffer with enough silence samples to fill the gap and
    // add it to the output buffer.
    scoped_refptr<DataBuffer> gap = new DataBuffer(bytes_to_fill);
    gap->SetDataSize(bytes_to_fill);
    memset(gap->GetWritableData(), 0, bytes_to_fill);
    gap->SetTimestamp(expected_timestamp);
    gap->SetDuration(output_timestamp_helper_.GetDuration(bytes_to_fill));
    AddOutputBuffer(gap);

    // Add the input buffer now that the gap has been filled.
    AddOutputBuffer(input);
    return true;
  }

  int bytes_to_skip = -bytes_to_fill;

  DVLOG(1) << "Overlap detected @ " << expected_timestamp.InMicroseconds()
           << " us: "  << -delta.InMicroseconds() << " us";

  if (input->GetDataSize() <= bytes_to_skip) {
    DVLOG(1) << "Dropping whole buffer";
    return true;
  }

  // Copy the trailing samples that do not overlap samples already output
  // into a new buffer. Add this new buffer to the output queue.
  //
  // TODO(acolwell): Implement a cross-fade here so the transition is less
  // jarring.
  int new_buffer_size = input->GetDataSize() - bytes_to_skip;

  scoped_refptr<DataBuffer> new_buffer = new DataBuffer(new_buffer_size);
  new_buffer->SetDataSize(new_buffer_size);
  memcpy(new_buffer->GetWritableData(),
         input->GetData() + bytes_to_skip,
         new_buffer_size);
  new_buffer->SetTimestamp(expected_timestamp);
  new_buffer->SetDuration(
      output_timestamp_helper_.GetDuration(new_buffer_size));
  AddOutputBuffer(new_buffer);
  return true;
}

bool AudioSplicer::HasNextBuffer() const {
  return !output_buffers_.empty();
}

scoped_refptr<Buffer> AudioSplicer::GetNextBuffer() {
  scoped_refptr<Buffer> ret = output_buffers_.front();
  output_buffers_.pop_front();
  return ret;
}

void AudioSplicer::AddOutputBuffer(const scoped_refptr<Buffer>& buffer) {
  output_timestamp_helper_.AddBytes(buffer->GetDataSize());
  output_buffers_.push_back(buffer);
}

}  // namespace media
