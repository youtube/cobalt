// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_TEST_HELPERS_H_
#define COBALT_MEDIA_BASE_TEST_HELPERS_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/string_number_conversions.h"
#include "base/threading/non_thread_safe.h"
#include "cobalt/media/base/audio_parameters.h"
#include "cobalt/media/base/channel_layout.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/base/pipeline_status.h"
#include "cobalt/media/base/sample_format.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/size.h"

namespace base {
class RunLoop;
class TimeDelta;
}

namespace media {

class AudioBuffer;
class DecoderBuffer;

// Return a callback that expects to be run once.
base::Closure NewExpectedClosure();
base::Callback<void(bool)> NewExpectedBoolCB(bool success);
PipelineStatusCB NewExpectedStatusCB(PipelineStatus status);

// Helper class for running a message loop until a callback has run. Useful for
// testing classes that run on more than a single thread.
//
// Events are intended for single use and cannot be reset.
class WaitableMessageLoopEvent : public base::NonThreadSafe {
 public:
  WaitableMessageLoopEvent();
  explicit WaitableMessageLoopEvent(base::TimeDelta timeout);
  ~WaitableMessageLoopEvent();

  // Returns a thread-safe closure that will signal |this| when executed.
  base::Closure GetClosure();
  PipelineStatusCB GetPipelineStatusCB();

  // Runs the current message loop until |this| has been signaled.
  //
  // Fails the test if the timeout is reached.
  void RunAndWait();

  // Runs the current message loop until |this| has been signaled and asserts
  // that the |expected| status was received.
  //
  // Fails the test if the timeout is reached.
  void RunAndWaitForStatus(PipelineStatus expected);

  bool is_signaled() const { return signaled_; }

 private:
  void OnCallback(PipelineStatus status);
  void OnTimeout();

  bool signaled_;
  PipelineStatus status_;
  std::unique_ptr<base::RunLoop> run_loop_;
  const base::TimeDelta timeout_;

  DISALLOW_COPY_AND_ASSIGN(WaitableMessageLoopEvent);
};

// Provides pre-canned VideoDecoderConfig. These types are used for tests that
// don't care about detailed parameters of the config.
class TestVideoConfig {
 public:
  // Returns a configuration that is invalid.
  static VideoDecoderConfig Invalid();

  static VideoDecoderConfig Normal();
  static VideoDecoderConfig NormalEncrypted();

  // Returns a configuration that is larger in dimensions than Normal().
  static VideoDecoderConfig Large();
  static VideoDecoderConfig LargeEncrypted();

  // Returns coded size for Normal and Large config.
  static gfx::Size NormalCodedSize();
  static gfx::Size LargeCodedSize();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestVideoConfig);
};

// Provides pre-canned AudioParameters objects.
class TestAudioParameters {
 public:
  static AudioParameters Normal();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAudioParameters);
};

// Create an AudioBuffer containing |frames| frames of data, where each sample
// is of type T.  |start| and |increment| are used to specify the values for the
// samples, which are created in channel order.  The value for frame and channel
// is determined by:
//
//   |start| + |channel| * |frames| * |increment| + index * |increment|
//
// E.g., for a stereo buffer the values in channel 0 will be:
//   start
//   start + increment
//   start + 2 * increment, ...
//
// While, values in channel 1 will be:
//   start + frames * increment
//   start + (frames + 1) * increment
//   start + (frames + 2) * increment, ...
//
// |start_time| will be used as the start time for the samples.
template <class T>
scoped_refptr<AudioBuffer> MakeAudioBuffer(SampleFormat format,
                                           ChannelLayout channel_layout,
                                           size_t channel_count,
                                           int sample_rate, T start,
                                           T increment, size_t frames,
                                           base::TimeDelta timestamp);

// Create a fake video DecoderBuffer for testing purpose. The buffer contains
// part of video decoder config info embedded so that the testing code can do
// some sanity check.
scoped_refptr<DecoderBuffer> CreateFakeVideoBufferForTest(
    const VideoDecoderConfig& config, base::TimeDelta timestamp,
    base::TimeDelta duration);

// Verify if a fake video DecoderBuffer is valid.
bool VerifyFakeVideoBufferForTest(const scoped_refptr<DecoderBuffer>& buffer,
                                  const VideoDecoderConfig& config);

MATCHER_P(HasTimestamp, timestamp_in_ms, "") {
  return arg.get() && !arg->end_of_stream() &&
         arg->timestamp().InMilliseconds() == timestamp_in_ms;
}

MATCHER(IsEndOfStream, "") { return arg.get() && arg->end_of_stream(); }

MATCHER_P(SegmentMissingFrames, track_id, "") {
  return CONTAINS_STRING(
      arg, "Media segment did not contain any coded frames for track " +
               std::string(track_id));
}

MATCHER(StreamParsingFailed, "") {
  return CONTAINS_STRING(arg, "Append: stream parsing failed.");
}

MATCHER_P(FoundStream, stream_type_string, "") {
  return CONTAINS_STRING(
             arg, "found_" + std::string(stream_type_string) + "_stream") &&
         CONTAINS_STRING(arg, "true");
}

MATCHER_P2(CodecName, stream_type_string, codec_string, "") {
  return CONTAINS_STRING(arg,
                         std::string(stream_type_string) + "_codec_name") &&
         CONTAINS_STRING(arg, std::string(codec_string));
}

MATCHER_P2(InitSegmentMismatchesMimeType, stream_type, codec_name, "") {
  return CONTAINS_STRING(arg, std::string(stream_type) + " stream codec " +
                                  std::string(codec_name) +
                                  " doesn't match SourceBuffer codecs.");
}

MATCHER_P(InitSegmentMissesExpectedTrack, missing_codec, "") {
  return CONTAINS_STRING(arg, "Initialization segment misses expected " +
                                  std::string(missing_codec) + " track.");
}

MATCHER_P2(UnexpectedTrack, track_type, id, "") {
  return CONTAINS_STRING(arg, std::string("Got unexpected ") + track_type +
                                  " track track_id=" + id);
}

MATCHER_P2(GeneratedSplice, duration_microseconds, time_microseconds, "") {
  return CONTAINS_STRING(arg, "Generated splice of overlap duration " +
                                  base::IntToString(duration_microseconds) +
                                  "us into new buffer at " +
                                  base::IntToString(time_microseconds) + "us.");
}

MATCHER_P2(SkippingSpliceAtOrBefore, new_microseconds, existing_microseconds,
           "") {
  return CONTAINS_STRING(
      arg, "Skipping splice frame generation: first new buffer at " +
               base::IntToString(new_microseconds) +
               "us begins at or before existing buffer at " +
               base::IntToString(existing_microseconds) + "us.");
}

MATCHER_P(SkippingSpliceAlreadySpliced, time_microseconds, "") {
  return CONTAINS_STRING(
      arg, "Skipping splice frame generation: overlapped buffers at " +
               base::IntToString(time_microseconds) +
               "us are in a previously buffered splice.");
}

MATCHER_P(WebMSimpleBlockDurationEstimated, estimated_duration_ms, "") {
  return CONTAINS_STRING(arg, "Estimating WebM block duration to be " +
                                  base::IntToString(estimated_duration_ms) +
                                  "ms for the last (Simple)Block in the "
                                  "Cluster for this Track. Use BlockGroups "
                                  "with BlockDurations at the end of each "
                                  "Track in a Cluster to avoid estimation.");
}

MATCHER_P(WebMNegativeTimecodeOffset, timecode_string, "") {
  return CONTAINS_STRING(arg, "Got a block with negative timecode offset " +
                                  std::string(timecode_string));
}

MATCHER(WebMOutOfOrderTimecode, "") {
  return CONTAINS_STRING(
      arg, "Got a block with a timecode before the previous block.");
}

MATCHER(WebMClusterBeforeFirstInfo, "") {
  return CONTAINS_STRING(arg, "Found Cluster element before Info.");
}

}  // namespace media

#endif  // COBALT_MEDIA_BASE_TEST_HELPERS_H_
