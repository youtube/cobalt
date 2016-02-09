#ifndef MEDIA_AUDIO_NULL_AUDIO_STREAMER_H_
#define MEDIA_AUDIO_NULL_AUDIO_STREAMER_H_

#include <map>

#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/timer.h"
#include "media/audio/shell_audio_streamer.h"

namespace media {

// This class implements a ShellAudioStreamer to be used when output to an
// audio device is not possible or not desired. It starts a thread that will
// regularly pull and consume frames from any added ShellAudioStreams at a rate
// expected by the stream's sampling frequency.
class NullAudioStreamer : public ShellAudioStreamer {
 public:
  static NullAudioStreamer* GetInstance() {
    return Singleton<NullAudioStreamer>::get();
  }

  Config GetConfig() const OVERRIDE;
  bool AddStream(ShellAudioStream* stream) OVERRIDE;
  void RemoveStream(ShellAudioStream* stream) OVERRIDE;
  bool HasStream(ShellAudioStream* stream) const OVERRIDE;
  bool SetVolume(ShellAudioStream* /* stream */, double /* volume*/) OVERRIDE {
    return true;
  }

 private:
  struct NullAudioStream {
    NullAudioStream() : total_available_frames(0) {}
    uint32 total_available_frames;
  };

  NullAudioStreamer();
  ~NullAudioStreamer() OVERRIDE;

  void StartNullStreamer();
  void AdvanceStreams();
  void PullFrames(ShellAudioStream* stream,
                  base::TimeDelta time_played,
                  NullAudioStream* null_audio_stream);

  base::Thread null_streamer_thread_;
  base::RepeatingTimer<NullAudioStreamer> advance_streams_timer_;
  base::Time last_run_time_;

  mutable base::Lock streams_lock_;
  typedef std::map<ShellAudioStream*, NullAudioStream> NullAudioStreamMap;
  NullAudioStreamMap streams_;

  DISALLOW_COPY_AND_ASSIGN(NullAudioStreamer);
  friend struct DefaultSingletonTraits<NullAudioStreamer>;
};

}  // namespace media

#endif  // MEDIA_AUDIO_NULL_AUDIO_STREAMER_H_
