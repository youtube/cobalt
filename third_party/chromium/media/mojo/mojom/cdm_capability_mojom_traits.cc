// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/mojom/cdm_capability_mojom_traits.h"

#include <set>
#include <utility>

namespace mojo {

namespace {

template <typename T>
bool AreUnique(const std::vector<T>& values) {
  std::set<T> unique_values;
  for (const auto& value : values)
    unique_values.emplace(value);
  return values.size() == unique_values.size();
}

}  // namespace

// static
bool StructTraits<media_m96::mojom::CdmCapabilityDataView, media_m96::CdmCapability>::
    Read(media_m96::mojom::CdmCapabilityDataView input,
         media_m96::CdmCapability* output) {
  std::vector<media_m96::AudioCodec> audio_codecs;
  if (!input.ReadAudioCodecs(&audio_codecs))
    return false;

  // Ensure that the AudioCodecs are unique.
  if (!AreUnique(audio_codecs))
    return false;

  media_m96::CdmCapability::VideoCodecMap video_codecs;
  if (!input.ReadVideoCodecs(&video_codecs))
    return false;

  // Ensure that the VideoCodecProfiles in each entry are unique.
  for (const auto& codec : video_codecs) {
    if (!AreUnique(codec.second))
      return false;
  }

  std::vector<media_m96::EncryptionScheme> encryption_schemes;
  if (!input.ReadEncryptionSchemes(&encryption_schemes))
    return false;

  std::vector<media_m96::CdmSessionType> session_types;
  if (!input.ReadSessionTypes(&session_types))
    return false;

  // |encryption_schemes| and |session_types| are convert to a base::flat_map
  // implicitly.
  *output = media_m96::CdmCapability(
      std::move(audio_codecs), std::move(video_codecs),
      std::move(encryption_schemes), std::move(session_types));
  return true;
}

}  // namespace mojo
