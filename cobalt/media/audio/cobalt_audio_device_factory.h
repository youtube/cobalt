// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_AUDIO_COBALT_AUDIO_DEVICE_FACTORY_H_
#define COBALT_MEDIA_AUDIO_COBALT_AUDIO_DEVICE_FACTORY_H_

#include "media/base/media_export.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"

namespace media {

class MEDIA_EXPORT CobaltAudioDeviceFactory final
    : public blink::AudioDeviceFactory {
 public:
  CobaltAudioDeviceFactory();
  ~CobaltAudioDeviceFactory() final;

  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) final;

  OutputDeviceInfo GetOutputDeviceInfo(
      const blink::LocalFrameToken& frame_token,
      const std::string& device_id) final;
};

}  // namespace media

#endif  // COBALT_MEDIA_AUDIO_COBALT_AUDIO_DEVICE_FACTORY_H_
