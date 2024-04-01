// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/media_foundation_cdm_data.h"

namespace media {

MediaFoundationCdmData::MediaFoundationCdmData() = default;

MediaFoundationCdmData::MediaFoundationCdmData(
    const base::UnguessableToken& origin_id,
    const absl::optional<std::vector<uint8_t>>& client_token,
    const base::FilePath& cdm_store_path_root)
    : origin_id(origin_id),
      client_token(client_token),
      cdm_store_path_root(cdm_store_path_root) {}

MediaFoundationCdmData::~MediaFoundationCdmData() = default;

}  // namespace media
