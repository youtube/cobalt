// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_CDM_KEY_INFORMATION_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_CDM_KEY_INFORMATION_MOJOM_TRAITS_H_

#include "third_party/chromium/media/base/cdm_key_information.h"
#include "third_party/chromium/media/mojo/mojom/content_decryption_module.mojom.h"

namespace mojo {

template <>
struct EnumTraits<media_m96::mojom::CdmKeyStatus,
                  media_m96::CdmKeyInformation::KeyStatus> {
  static media_m96::mojom::CdmKeyStatus ToMojom(
      media_m96::CdmKeyInformation::KeyStatus key_status);

  static bool FromMojom(media_m96::mojom::CdmKeyStatus input,
                        media_m96::CdmKeyInformation::KeyStatus* out);
};

template <>
struct StructTraits<media_m96::mojom::CdmKeyInformationDataView,
                    std::unique_ptr<media_m96::CdmKeyInformation>> {
  static const std::vector<uint8_t>& key_id(
      const std::unique_ptr<media_m96::CdmKeyInformation>& input) {
    return input->key_id;
  }

  static media_m96::CdmKeyInformation::KeyStatus status(
      const std::unique_ptr<media_m96::CdmKeyInformation>& input) {
    return input->status;
  }

  static uint32_t system_code(
      const std::unique_ptr<media_m96::CdmKeyInformation>& input) {
    return input->system_code;
  }

  static bool Read(media_m96::mojom::CdmKeyInformationDataView input,
                   std::unique_ptr<media_m96::CdmKeyInformation>* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_CDM_KEY_INFORMATION_MOJOM_TRAITS_H_
