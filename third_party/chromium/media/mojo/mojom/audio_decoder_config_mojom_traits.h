// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_AUDIO_DECODER_CONFIG_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_AUDIO_DECODER_CONFIG_MOJOM_TRAITS_H_

#include "third_party/chromium/media/base/audio_decoder_config.h"
#include "third_party/chromium/media/base/ipc/media_param_traits.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::AudioDecoderConfigDataView,
                    media_m96::AudioDecoderConfig> {
  static media_m96::AudioCodec codec(const media_m96::AudioDecoderConfig& input) {
    return input.codec();
  }

  static media_m96::SampleFormat sample_format(
      const media_m96::AudioDecoderConfig& input) {
    return input.sample_format();
  }

  static media_m96::ChannelLayout channel_layout(
      const media_m96::AudioDecoderConfig& input) {
    return input.channel_layout();
  }

  static int samples_per_second(const media_m96::AudioDecoderConfig& input) {
    return input.samples_per_second();
  }

  static const std::vector<uint8_t>& extra_data(
      const media_m96::AudioDecoderConfig& input) {
    return input.extra_data();
  }

  static media_m96::EncryptionScheme encryption_scheme(
      const media_m96::AudioDecoderConfig& input) {
    return input.encryption_scheme();
  }

  static base::TimeDelta seek_preroll(const media_m96::AudioDecoderConfig& input) {
    return input.seek_preroll();
  }

  static int codec_delay(const media_m96::AudioDecoderConfig& input) {
    return input.codec_delay();
  }

  static media_m96::AudioCodecProfile profile(
      const media_m96::AudioDecoderConfig& input) {
    return input.profile();
  }

  static media_m96::ChannelLayout target_output_channel_layout(
      const media_m96::AudioDecoderConfig& input) {
    return input.target_output_channel_layout();
  }

  static bool should_discard_decoder_delay(
      const media_m96::AudioDecoderConfig& input) {
    return input.should_discard_decoder_delay();
  }

  static const std::vector<uint8_t>& aac_extra_data(
      const media_m96::AudioDecoderConfig& input) {
    return input.aac_extra_data();
  }

  static bool Read(media_m96::mojom::AudioDecoderConfigDataView input,
                   media_m96::AudioDecoderConfig* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_AUDIO_DECODER_CONFIG_MOJOM_TRAITS_H_
