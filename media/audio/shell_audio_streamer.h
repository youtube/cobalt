#ifndef MEDIA_AUDIO_SHELL_AUDIO_STREAMER_H_
#define MEDIA_AUDIO_SHELL_AUDIO_STREAMER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace media {

class AudioBus;
class AudioParameters;

// Abstract class for adding an audio stream to the audio streamer.
// Your class can implement this interface and then call AddStream(this) to
// attach itself to the hardware audio streamer.
class ShellAudioStream {
 public:
  // Checks if "Pause" has been requested on this stream. The streamer will
  // halt playback the next time it updates.
  virtual bool PauseRequested() const = 0;
  // This function serves several purposes:
  // 1. Once the audio stream is added to the streamer. This function will be
  // called periodically so the stream (the AudioSink) can pull data from
  // upper level, even when it is paused.
  // 2. It will return true to indicate that it is playing, false to pause.
  // NOTE: This function can be called on a low level audio mixer thread and
  // is LATENCY-SENSITIVE. Avoid locks and other high-latency operations!
  virtual bool PullFrames(uint32_t* offset_in_frame,
                          uint32_t* total_frames) = 0;
  // This function tells the stream that `frame_played` of audio frames have
  // been played and can be removed from the buffer. The stream can also use
  // this to calculate the time elapsed. The stream shouldn't pull any data
  // in this function, PullFrames is the only point to pull data.
  virtual void ConsumeFrames(uint32_t frame_played) = 0;
  // Get the AudioParameters for this stream
  virtual const AudioParameters& GetAudioParameters() const = 0;
  // Get the internal buffer of this audio stream as an AudioBus.
  // This is useful when audio is being played from the buffer directly.
  virtual AudioBus* GetAudioBus() = 0;
};

// The class contains stub functions for platform specific audio playback.
// Classes inherited from it have to implement all the pure virtual functions
// and provide implementations for the static functions.
class ShellAudioStreamer {
 public:
  struct Config {
    // Is the data in audio bus interleaved and stored as one channel.
    bool interleaved;
    // Set to true when the platform needs two streams to decode mono audio
    // TODO(***REMOVED***) : The renderer should alter the audio param to reflect
    // this.
    bool decode_mono_as_stereo;
    // These paramters control rebuffering.
    // See ShellAudioSink::ResumeAfterUnderflow for more details.
    uint32 initial_rebuffering_frames_per_channel;
    uint32 max_frames_per_channel;
    uint32 initial_frames_per_channel;
    // amount of data we should request each time from the renderer
    uint64 renderer_request_frames;
    uint32 underflow_threshold;
  };

  ShellAudioStreamer() {}
  virtual ~ShellAudioStreamer() {};

  // The only instance of the platform specific audio streamer. It becomes
  // valid after calling Initialize and become NULL after calling Terminate.
  static ShellAudioStreamer* Instance();
  static void Initialize();
  static void Terminate();

  virtual Config GetConfig() const = 0;
  virtual bool AddStream(ShellAudioStream* stream) = 0;
  virtual void RemoveStream(ShellAudioStream* stream) = 0;
  virtual bool HasStream(ShellAudioStream* stream) const = 0;
  virtual bool SetVolume(ShellAudioStream* stream, double volume) = 0;

  DISALLOW_COPY_AND_ASSIGN(ShellAudioStreamer);
};

}  // namespace media

#endif  // MEDIA_AUDIO_SHELL_AUDIO_STREAMER_H_
