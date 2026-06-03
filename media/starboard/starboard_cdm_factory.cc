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

#include "starboard_cdm_factory.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "media/base/cdm_config.h"
#include "media/base/key_systems.h"
#include "starboard_cdm.h"

namespace media {

StarboardCdmFactory::StarboardCdmFactory() {}

StarboardCdmFactory::~StarboardCdmFactory() = default;

void StarboardCdmFactory::Create(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  LOG(INFO) << __func__ << ": cdm_config=" << cdm_config;

  auto cdm = base::MakeRefCounted<StarboardCdm>(
      cdm_config, session_message_cb, session_closed_cb, session_keys_change_cb,
      session_expiration_update_cb);

  auto bound_cdm_created_cb =
      base::BindPostTaskToCurrentDefault(std::move(cdm_created_cb));

  if (cdm->HasValidSbDrm()) {
    std::move(bound_cdm_created_cb)
        .Run(std::move(cdm), CreateCdmStatus::kSuccess);
  } else {
    std::move(bound_cdm_created_cb)
        .Run(nullptr, CreateCdmStatus::kCdmFactoryCreationFailed);
  }
}

}  // namespace media
