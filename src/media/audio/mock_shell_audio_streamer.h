/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_AUDIO_MOCK_SHELL_AUDIO_STREAMER_H_
#define MEDIA_AUDIO_MOCK_SHELL_AUDIO_STREAMER_H_

#include "media/audio/shell_audio_streamer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockShellAudioStream : public ShellAudioStream {
 public:
  MockShellAudioStream() {}
  MOCK_CONST_METHOD0(PauseRequested, bool ());
  MOCK_METHOD2(PullFrames, bool (uint32_t*, uint32_t*));
  MOCK_METHOD1(ConsumeFrames, void (uint32_t));
  MOCK_CONST_METHOD0(GetAudioParameters, const AudioParameters& ());
  MOCK_METHOD0(GetAudioBus, AudioBus* ());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockShellAudioStream);
};

class MockShellAudioStreamer : public ShellAudioStreamer {
 public:
  MockShellAudioStreamer() {}
  MOCK_CONST_METHOD0(GetConfig, Config ());
  MOCK_METHOD1(AddStream, bool (ShellAudioStream*));
  MOCK_METHOD1(RemoveStream, void (ShellAudioStream*));
  MOCK_CONST_METHOD1(HasStream, bool (ShellAudioStream*));
  MOCK_METHOD2(SetVolume, bool (ShellAudioStream*, double));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockShellAudioStreamer);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MOCK_SHELL_AUDIO_STREAMER_H_

