// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_CDM_DATA_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_CDM_DATA_MOJOM_TRAITS_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "third_party/chromium/media/cdm/media_foundation_cdm_data.h"
#include "third_party/chromium/media/mojo/mojom/cdm_document_service.mojom.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::MediaFoundationCdmDataDataView,
                    std::unique_ptr<media_m96::MediaFoundationCdmData>> {
  static const base::UnguessableToken& origin_id(
      const std::unique_ptr<media_m96::MediaFoundationCdmData>& input) {
    return input->origin_id;
  }

  static const absl::optional<std::vector<uint8_t>>& client_token(
      const std::unique_ptr<media_m96::MediaFoundationCdmData>& input) {
    return input->client_token;
  }

  static const base::FilePath& cdm_store_path_root(
      const std::unique_ptr<media_m96::MediaFoundationCdmData>& input) {
    return input->cdm_store_path_root;
  }

  static bool Read(media_m96::mojom::MediaFoundationCdmDataDataView input,
                   std::unique_ptr<media_m96::MediaFoundationCdmData>* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_CDM_DATA_MOJOM_TRAITS_H_
