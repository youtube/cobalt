// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_MOCK_AUDIO_SOURCE_CALLBACK_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_MOCK_AUDIO_SOURCE_CALLBACK_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/chromium/media/audio/audio_io.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_m96 {

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MockAudioSourceCallback();

  MockAudioSourceCallback(const MockAudioSourceCallback&) = delete;
  MockAudioSourceCallback& operator=(const MockAudioSourceCallback&) = delete;

  ~MockAudioSourceCallback() override;

  MOCK_METHOD4(OnMoreData,
               int(base::TimeDelta, base::TimeTicks, int, AudioBus*));
  MOCK_METHOD1(OnError, void(ErrorType));
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_MOCK_AUDIO_SOURCE_CALLBACK_H_
