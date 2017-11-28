/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef MEDIA_AUDIO_NULL_AUDIO_STREAMER_H_
#define MEDIA_AUDIO_NULL_AUDIO_STREAMER_H_

#include <map>

#include "base/memory/singleton.h"
#include "base/optional.h"
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

  Config GetConfig() const override;
  bool AddStream(ShellAudioStream* stream) override;
  void RemoveStream(ShellAudioStream* stream) override;
  bool HasStream(ShellAudioStream* stream) const override;
  bool SetVolume(ShellAudioStream* /* stream */, double /* volume*/) override {
    return true;
  }

 private:
  struct NullAudioStream {
    NullAudioStream() : total_available_frames(0) {}
    uint32 total_available_frames;
  };

  NullAudioStreamer();
  ~NullAudioStreamer() override;

  void StartNullStreamer();
  void StopNullStreamer();
  void AdvanceStreams();
  void PullFrames(ShellAudioStream* stream,
                  base::TimeDelta time_played,
                  NullAudioStream* null_audio_stream);

  base::Thread null_streamer_thread_;
  typedef base::RepeatingTimer<NullAudioStreamer> RepeatingTimer;
  base::optional<RepeatingTimer> advance_streams_timer_;
  base::Time last_run_time_;

  mutable base::Lock streams_lock_;
  typedef std::map<ShellAudioStream*, NullAudioStream> NullAudioStreamMap;
  NullAudioStreamMap streams_;

  DISALLOW_COPY_AND_ASSIGN(NullAudioStreamer);
  friend struct DefaultSingletonTraits<NullAudioStreamer>;
};

}  // namespace media

#endif  // MEDIA_AUDIO_NULL_AUDIO_STREAMER_H_
