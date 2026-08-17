// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MEDIA_STARBOARD_MOCK_SBPLAYER_INTERFACE_H_
#define MEDIA_STARBOARD_MOCK_SBPLAYER_INTERFACE_H_

#include "build/build_config.h"
#include "media/starboard/sbplayer_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// A lightweight mock player structure used to back opaque SbPlayer handles in
// tests without conflicting with the global SbPlayerPrivate definition.
struct MockSbPlayer {
  MockSbPlayer() = default;
  MockSbPlayer(const MockSbPlayer&) = delete;
  MockSbPlayer& operator=(const MockSbPlayer&) = delete;
  ~MockSbPlayer() = default;
};

// A mock implementation of SbPlayerInterface used for testing the media
// pipeline's interaction with the Starboard player. This class is typically
// owned by the test fixture or instantiated as a local variable within a test,
// and is thread-safe.
class MockSbPlayerInterface : public SbPlayerInterface {
 public:
  MockSbPlayerInterface();
  ~MockSbPlayerInterface() override;

  MOCK_METHOD(SbPlayer,
              Create,
              (SbWindow,
               const SbPlayerCreationParam*,
               SbPlayerDeallocateSampleFunc,
               SbPlayerDecoderStatusFunc,
               SbPlayerStatusFunc,
               SbPlayerErrorFunc,
               void*,
               SbDecodeTargetGraphicsContextProvider*),
              (override));
  SbPlayerOutputMode GetPreferredOutputMode(
      const SbPlayerCreationParam* creation_param) override {
    return kSbPlayerOutputModePunchOut;
  }
  void Destroy(SbPlayer player) override {
    if (player) {
      delete reinterpret_cast<MockSbPlayer*>(player);
    }
  }
  MOCK_METHOD(void, Seek, (SbPlayer, base::TimeDelta, int), (override));
  MOCK_METHOD(void,
              WriteSamples,
              (SbPlayer, SbMediaType, const SbPlayerSampleInfo*, int),
              (override));
  int GetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                        SbMediaType sample_type) override {
    return 1;
  }
  MOCK_METHOD(void, WriteEndOfStream, (SbPlayer, SbMediaType), (override));
  MOCK_METHOD(void, SetBounds, (SbPlayer, int, int, int, int, int), (override));
  bool SetPlaybackRate(SbPlayer player, double playback_rate) override {
    return true;
  }
  void SetVolume(SbPlayer player, double volume) override {}
  MOCK_METHOD(void, GetInfo, (SbPlayer, SbPlayerInfo*), (override));
  SbDecodeTarget GetCurrentFrame(SbPlayer player) override {
    return kSbDecodeTargetInvalid;
  }

#if BUILDFLAG(IS_IOS_TVOS)
  MOCK_METHOD(SbPlayer,
              CreateUrlPlayer,
              (const char*,
               SbWindow,
               SbPlayerStatusFunc,
               SbPlayerEncryptedMediaInitDataEncounteredCB,
               SbPlayerErrorFunc,
               void*),
              (override));
  MOCK_METHOD(void, SetUrlPlayerDrmSystem, (SbPlayer, SbDrmSystem), (override));
  MOCK_METHOD(void,
              GetUrlPlayerExtraInfo,
              (SbPlayer, SbUrlPlayerExtraInfo*),
              (override));

  bool GetUrlPlayerOutputModeSupported(
      SbPlayerOutputMode output_mode) override {
    return true;
  }
#endif  // BUILDFLAG(IS_IOS_TVOS)

  bool GetAudioConfiguration(
      SbPlayer player,
      int index,
      SbMediaAudioConfiguration* out_audio_configuration) override {
    if (out_audio_configuration) {
      *out_audio_configuration = {};
    }
    return true;
  }
};

}  // namespace media

#endif  // MEDIA_STARBOARD_MOCK_SBPLAYER_INTERFACE_H_
