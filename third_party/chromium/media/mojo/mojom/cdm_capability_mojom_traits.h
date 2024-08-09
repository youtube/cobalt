// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_CDM_CAPABILITY_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_CDM_CAPABILITY_MOJOM_TRAITS_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "third_party/chromium/media/base/audio_codecs.h"
#include "third_party/chromium/media/base/content_decryption_module.h"
#include "third_party/chromium/media/base/encryption_scheme.h"
#include "third_party/chromium/media/base/video_codecs.h"
#include "third_party/chromium/media/cdm/cdm_capability.h"
#include "third_party/chromium/media/mojo/mojom/key_system_support.mojom.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::CdmCapabilityDataView, media_m96::CdmCapability> {
  static const std::vector<media_m96::AudioCodec>& audio_codecs(
      const media_m96::CdmCapability& input) {
    return input.audio_codecs;
  }

  static const media_m96::CdmCapability::VideoCodecMap& video_codecs(
      const media_m96::CdmCapability& input) {
    return input.video_codecs;
  }

  // List of encryption schemes supported by the CDM (e.g. cenc).
  static const base::flat_set<media_m96::EncryptionScheme>& encryption_schemes(
      const media_m96::CdmCapability& input) {
    return input.encryption_schemes;
  }

  // List of session types supported by the CDM.
  static const base::flat_set<media_m96::CdmSessionType>& session_types(
      const media_m96::CdmCapability& input) {
    return input.session_types;
  }

  static bool Read(media_m96::mojom::CdmCapabilityDataView input,
                   media_m96::CdmCapability* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_CDM_CAPABILITY_MOJOM_TRAITS_H_
