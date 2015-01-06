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

