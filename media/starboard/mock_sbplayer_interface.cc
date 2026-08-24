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

#include "media/starboard/mock_sbplayer_interface.h"

namespace media {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

MockSbPlayerInterface::MockSbPlayerInterface() {
  SetupDefaultExpectations();
}

MockSbPlayerInterface::~MockSbPlayerInterface() = default;

void MockSbPlayerInterface::SetupDefaultExpectations() {
  ON_CALL(*this, GetPreferredOutputMode(_))
      .WillByDefault(Return(kSbPlayerOutputModePunchOut));
  ON_CALL(*this, Destroy(_)).WillByDefault(Invoke([](SbPlayer player) {
    if (player) {
      delete reinterpret_cast<MockSbPlayer*>(player);
    }
  }));
  ON_CALL(*this, GetMaximumNumberOfSamplesPerWrite(_, _))
      .WillByDefault(Return(1));
  ON_CALL(*this, SetPlaybackRate(_, _)).WillByDefault(Return(true));
  ON_CALL(*this, SetVolume(_, _)).WillByDefault(Return());
  ON_CALL(*this, GetCurrentFrame(_))
      .WillByDefault(Return(kSbDecodeTargetInvalid));
#if BUILDFLAG(IS_IOS_TVOS)
  ON_CALL(*this, GetUrlPlayerOutputModeSupported(_))
      .WillByDefault(Return(true));
#endif  // BUILDFLAG(IS_IOS_TVOS)
  ON_CALL(*this, GetAudioConfiguration(_, _, _))
      .WillByDefault(
          Invoke([](SbPlayer /*player*/, int /*index*/,
                    SbMediaAudioConfiguration* out_audio_configuration) {
            if (out_audio_configuration) {
              *out_audio_configuration = {};
            }
            return true;
          }));
}

}  // namespace media
