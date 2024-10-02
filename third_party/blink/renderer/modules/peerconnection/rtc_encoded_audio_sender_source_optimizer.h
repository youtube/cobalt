// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_SENDER_SOURCE_OPTIMIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_SENDER_SOURCE_OPTIMIZER_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_source.h"

namespace blink {

class UnderlyingSourceBase;
class ScriptState;
class RTCEncodedAudioUnderlyingSource;

class MODULES_EXPORT RtcEncodedAudioSenderSourceOptimizer
    : public ReadableStreamTransferringOptimizer {
 public:
  using UnderlyingSourceSetter = WTF::CrossThreadFunction<void(
      RTCEncodedAudioUnderlyingSource*,
      scoped_refptr<base::SingleThreadTaskRunner>)>;
  RtcEncodedAudioSenderSourceOptimizer(
      UnderlyingSourceSetter,
      WTF::CrossThreadOnceClosure disconnect_callback);
  UnderlyingSourceBase* PerformInProcessOptimization(
      ScriptState* script_state) override;

 private:
  UnderlyingSourceSetter set_underlying_source_;
  WTF::CrossThreadOnceClosure disconnect_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_SENDER_SOURCE_OPTIMIZER_H_
