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

#ifndef MEDIA_AUDIO_SHELL_AUDIO_STREAMER_LINUX_H_
#define MEDIA_AUDIO_SHELL_AUDIO_STREAMER_LINUX_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "media/audio/shell_audio_streamer.h"
#include "media/audio/shell_pulse_audio.h"

namespace media {

class PulseAudioHost;

class ShellAudioStreamerLinux : public ShellAudioStreamer {
 public:
  ShellAudioStreamerLinux();
  ~ShellAudioStreamerLinux();

  virtual Config GetConfig() const OVERRIDE;
  virtual bool AddStream(ShellAudioStream* stream) OVERRIDE;
  virtual void RemoveStream(ShellAudioStream* stream) OVERRIDE;
  virtual bool HasStream(ShellAudioStream* stream) const OVERRIDE;
  virtual bool SetVolume(ShellAudioStream* stream, double volume) OVERRIDE;

 private:
  typedef std::map<ShellAudioStream*, PulseAudioHost*> StreamMap;
  StreamMap streams_;
  scoped_ptr<ShellPulseAudioContext> pulse_audio_context_;
  STLValueDeleter<StreamMap> streams_value_deleter_;
  mutable base::Lock streams_lock_;

  DISALLOW_COPY_AND_ASSIGN(ShellAudioStreamerLinux);
};

}  // namespace media

#endif  // MEDIA_AUDIO_SHELL_AUDIO_STREAMER_LINUX_H_
