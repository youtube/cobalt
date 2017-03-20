// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FILTERS_AUDIO_TIMESTAMP_VALIDATOR_H_
#define COBALT_MEDIA_FILTERS_AUDIO_TIMESTAMP_VALIDATOR_H_

#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cobalt/media/base/audio_buffer.h"
#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/audio_timestamp_helper.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/base/timestamp_constants.h"

namespace media {

class MEDIA_EXPORT AudioTimestampValidator {
 public:
  AudioTimestampValidator(const AudioDecoderConfig& decoder_config,
                          const scoped_refptr<MediaLog>& media_log);
  ~AudioTimestampValidator();

  // These methods monitor DecoderBuffer timestamps for gaps for the purpose of
  // warning developers when gaps may cause AV sync drift. A DecoderBuffer's
  // timestamp should roughly equal the timestamp of the previous buffer offset
  // by the previous buffer's duration.
  void CheckForTimestampGap(const scoped_refptr<DecoderBuffer>& buffer);
  void RecordOutputDuration(const scoped_refptr<AudioBuffer>& buffer);

 private:
  bool has_codec_delay_;
  scoped_refptr<MediaLog> media_log_;

  // Accumulates time from decoded audio frames. We adjust the base timestamp as
  // needed for the first few buffers (stabilization period) of decoded output
  // to account for pre-skip and codec delay. See CheckForTimestampGap().
  std::unique_ptr<AudioTimestampHelper> audio_output_ts_helper_;

  base::TimeDelta audio_base_ts_;

  // Initially false, set to true when we observe gap between encoded timestamps
  // match gap between output decoder buffers.
  bool reached_stable_state_;

  // Counts attempts to adjust |audio_output_ts_helper_| base offset in effort
  // to form expectation for encoded timestamps based on decoded output. Give up
  // making adjustments when count exceeds |limit_unstable_audio_tries_|.
  int num_unstable_audio_tries_;

  // Limits the number of attempts to stabilize audio timestamp expectations.
  int limit_unstable_audio_tries_;

  // How many milliseconds can DecoderBuffer timestamps differ from expectations
  // before we MEDIA_LOG warn developers. Threshold initially set from
  // kGapWarningThresholdMsec. Once hit, the threshold is increased by
  // the detected gap amount. This avoids log spam while still emitting
  // logs if things get worse. See CheckTimestampForGap().
  uint32_t drift_warning_threshold_msec_;

  DISALLOW_COPY_AND_ASSIGN(AudioTimestampValidator);
};

}  // namespace media

#endif  // COBALT_MEDIA_FILTERS_AUDIO_TIMESTAMP_VALIDATOR_H_
