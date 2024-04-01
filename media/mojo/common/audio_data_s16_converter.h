// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_AUDIO_DATA_S16_CONVERTER_H_
#define MEDIA_MOJO_COMMON_AUDIO_DATA_S16_CONVERTER_H_

#include <memory>

#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace media {

class ChannelMixer;

// Converts AudioBuffer or AudioBus into mojom::AudioDataS16.
class AudioDataS16Converter {
 public:
  AudioDataS16Converter();
  virtual ~AudioDataS16Converter();
  AudioDataS16Converter(const AudioDataS16Converter&) = delete;
  AudioDataS16Converter& operator=(const AudioDataS16Converter&) = delete;

  mojom::AudioDataS16Ptr ConvertToAudioDataS16(
      scoped_refptr<AudioBuffer> buffer,
      bool is_multichannel_supported);

  mojom::AudioDataS16Ptr ConvertToAudioDataS16(
      std::unique_ptr<AudioBus> audio_bus,
      int sample_rate,
      ChannelLayout channel_layout,
      bool is_multichannel_supported);

 private:
  mojom::AudioDataS16Ptr ConvertAudioBusToAudioDataS16Internal(
      const AudioBus& audio_bus,
      int sample_rate,
      ChannelLayout channel_layout,
      bool is_multichannel_supported);

  // Recreates the temporary audio bus if the frame count or channel count
  // changed and reads the frames from the buffer into the temporary audio bus.
  void CopyBufferToTempAudioBus(const AudioBuffer& buffer);

  // Resets the temporary monaural audio bus and the channel mixer used to
  // combine multiple audio channels.
  void ResetChannelMixerIfNeeded(int frame_count, ChannelLayout channel_layout);

  // The temporary audio bus used to convert the raw audio to the appropriate
  // format.
  std::unique_ptr<AudioBus> temp_audio_bus_;

  // The temporary audio bus used to mix multichannel audio into a single
  // channel.
  std::unique_ptr<AudioBus> monaural_audio_bus_;

  std::unique_ptr<ChannelMixer> channel_mixer_;

  // The layout used to instantiate the channel mixer.
  ChannelLayout channel_layout_ = ChannelLayout::CHANNEL_LAYOUT_NONE;
};

}  // namespace media

#endif  // MEDIA_MOJO_COMMON_AUDIO_DATA_S16_CONVERTER_H_
