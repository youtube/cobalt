// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_CDM_KEY_INFORMATION_H_
#define COBALT_MEDIA_BASE_CDM_KEY_INFORMATION_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "cobalt/media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT CdmKeyInformation {
  enum KeyStatus {
    USABLE = 0,
    INTERNAL_ERROR = 1,
    EXPIRED = 2,
    OUTPUT_RESTRICTED = 3,
    OUTPUT_DOWNSCALED = 4,
    KEY_STATUS_PENDING = 5,
    RELEASED = 6,
    KEY_STATUS_MAX = RELEASED
  };

  // Default constructor needed for passing this type through IPC. Regular
  // code should use one of the other constructors.
  CdmKeyInformation();
  CdmKeyInformation(const std::vector<uint8_t>& key_id, KeyStatus status,
                    uint32_t system_code);
  CdmKeyInformation(const std::string& key_id, KeyStatus status,
                    uint32_t system_code);
  CdmKeyInformation(const uint8_t* key_id_data, size_t key_id_length,
                    KeyStatus status, uint32_t system_code);
  CdmKeyInformation(const CdmKeyInformation& other);
  ~CdmKeyInformation();

  std::vector<uint8_t> key_id;
  KeyStatus status;
  uint32_t system_code;
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_CDM_KEY_INFORMATION_H_
