// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_adapter_factory.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_factory.h"
#include "media/cdm/cdm_adapter.h"
#include "media/cdm/cdm_module.h"
#include "url/origin.h"

namespace media {

CdmAdapterFactory::CdmAdapterFactory(HelperCreationCB helper_creation_cb)
    : helper_creation_cb_(std::move(helper_creation_cb)) {
  DCHECK(helper_creation_cb_);
}

CdmAdapterFactory::~CdmAdapterFactory() = default;

void CdmAdapterFactory::Create(
    const std::string& key_system,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  DVLOG(1) << __func__ << ": key_system=" << key_system;

  CdmAdapter::CreateCdmFunc create_cdm_func =
      CdmModule::GetInstance()->GetCreateCdmFunc();
  if (!create_cdm_func) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cdm_created_cb), nullptr,
                                  "CreateCdmFunc not available."));
    return;
  }

  std::unique_ptr<CdmAuxiliaryHelper> cdm_helper = helper_creation_cb_.Run();
  if (!cdm_helper) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cdm_created_cb), nullptr,
                                  "CDM helper creation failed."));
    return;
  }

  CdmAdapter::Create(key_system, cdm_config, create_cdm_func,
                     std::move(cdm_helper), session_message_cb,
                     session_closed_cb, session_keys_change_cb,
                     session_expiration_update_cb, std::move(cdm_created_cb));
}

}  // namespace media
