/*
 * Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef MEDIA_AUDIO_SHELL_AUDIO_SINK_H_
#define MEDIA_AUDIO_SHELL_AUDIO_SINK_H_

#include "media/base/audio_renderer_sink.h"

namespace media {


class MEDIA_EXPORT ShellAudioSink
    : NON_EXPORTED_BASE(public AudioRendererSink) {
 public:
  // static factory method
  static ShellAudioSink* Create();

  // AudioRendererSink implementation
  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback) = 0;
  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual void Pause(bool flush) = 0;
  virtual void Play() = 0;
  virtual void SetPlaybackRate(float rate) = 0;
  virtual bool SetVolume(double volume) = 0;
  virtual void ResumeAfterUnderflow(bool buffer_more_audio) = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_SHELL_AUDIO_SINK_H_
