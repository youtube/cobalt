// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/mojom/video_decoder_config_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media_m96::mojom::VideoDecoderConfigDataView,
                  media_m96::VideoDecoderConfig>::
    Read(media_m96::mojom::VideoDecoderConfigDataView input,
         media_m96::VideoDecoderConfig* output) {
  media_m96::VideoCodec codec;
  if (!input.ReadCodec(&codec))
    return false;

  media_m96::VideoCodecProfile profile;
  if (!input.ReadProfile(&profile))
    return false;

  media_m96::VideoTransformation transformation;
  if (!input.ReadTransformation(&transformation))
    return false;

  gfx::Size coded_size;
  if (!input.ReadCodedSize(&coded_size))
    return false;

  gfx::Rect visible_rect;
  if (!input.ReadVisibleRect(&visible_rect))
    return false;

  gfx::Size natural_size;
  if (!input.ReadNaturalSize(&natural_size))
    return false;

  std::vector<uint8_t> extra_data;
  if (!input.ReadExtraData(&extra_data))
    return false;

  media_m96::EncryptionScheme encryption_scheme;
  if (!input.ReadEncryptionScheme(&encryption_scheme))
    return false;

  media_m96::VideoColorSpace color_space;
  if (!input.ReadColorSpaceInfo(&color_space))
    return false;

  absl::optional<gfx::HDRMetadata> hdr_metadata;
  if (!input.ReadHdrMetadata(&hdr_metadata))
    return false;

  output->Initialize(codec, profile,
                     input.has_alpha()
                         ? media_m96::VideoDecoderConfig::AlphaMode::kHasAlpha
                         : media_m96::VideoDecoderConfig::AlphaMode::kIsOpaque,
                     color_space, transformation, coded_size, visible_rect,
                     natural_size, extra_data, encryption_scheme);

  if (hdr_metadata)
    output->set_hdr_metadata(hdr_metadata.value());

  output->set_level(input.level());

  if (!output->IsValidConfig())
    return false;

  return true;
}

}  // namespace mojo
