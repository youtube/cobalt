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

#ifndef MEDIA_FILTERS_SHELL_AUDIO_RENDERER_H_
#define MEDIA_FILTERS_SHELL_AUDIO_RENDERER_H_

#include "base/message_loop_proxy.h"
#include "media/base/audio_renderer.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/decryptor.h"

namespace media {

class MEDIA_EXPORT ShellAudioRenderer
    : public AudioRenderer,
      public media::AudioRendererSink::RenderCallback {
 public:
  // platform-specific factory method
  static ShellAudioRenderer* Create(
      media::AudioRendererSink* sink,
      const SetDecryptorReadyCB& set_decryptor_ready_cb,
      const scoped_refptr<base::MessageLoopProxy>& message_loop);

  // ======== AudioRenderer Implementation

  // ======== Filter Implementation

 protected:
  virtual ~ShellAudioRenderer() {}
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_AUDIO_RENDERER_H_
