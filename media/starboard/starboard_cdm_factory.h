// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MEDIA_STARBOARD_STARBOARD_CDM_FACTORY_H_
#define MEDIA_STARBOARD_STARBOARD_CDM_FACTORY_H_

#include <stdint.h>

#include "media/base/cdm_factory.h"
#include "url/origin.h"

namespace media {

class MEDIA_EXPORT StarboardCdmFactory final : public CdmFactory {
 public:
  StarboardCdmFactory();

  StarboardCdmFactory(const StarboardCdmFactory&) = delete;
  StarboardCdmFactory& operator=(const StarboardCdmFactory&) = delete;

  ~StarboardCdmFactory() override;

  // CdmFactory implementation.
  void Create(const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) override;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_CDM_FACTORY_H_
