// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_decoder_config_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::AudioDecoderConfigDataView,
                  media::AudioDecoderConfig>::
    Read(media::mojom::AudioDecoderConfigDataView input,
         media::AudioDecoderConfig* output) {
  media::AudioCodec codec;
  if (!input.ReadCodec(&codec))
    return false;

  media::SampleFormat sample_format;
  if (!input.ReadSampleFormat(&sample_format))
    return false;

  media::ChannelLayout channel_layout;
  if (!input.ReadChannelLayout(&channel_layout))
    return false;

  std::vector<uint8_t> extra_data;
  if (!input.ReadExtraData(&extra_data))
    return false;

  media::EncryptionScheme encryption_scheme;
  if (!input.ReadEncryptionScheme(&encryption_scheme))
    return false;

  base::TimeDelta seek_preroll;
  if (!input.ReadSeekPreroll(&seek_preroll))
    return false;

  media::AudioCodecProfile profile;
  if (!input.ReadProfile(&profile))
    return false;

  media::ChannelLayout target_output_channel_layout;
  if (!input.ReadTargetOutputChannelLayout(&target_output_channel_layout))
    return false;

  std::vector<uint8_t> aac_extra_data;
  if (!input.ReadAacExtraData(&aac_extra_data))
    return false;

  output->Initialize(codec, sample_format, channel_layout,
                     input.samples_per_second(), std::move(extra_data),
                     encryption_scheme, seek_preroll, input.codec_delay());
  output->set_profile(profile);
  output->set_target_output_channel_layout(target_output_channel_layout);
  output->set_aac_extra_data(std::move(aac_extra_data));

  if (!input.should_discard_decoder_delay())
    output->disable_discard_decoder_delay();

  return output->IsValidConfig();
}

}  // namespace mojo
