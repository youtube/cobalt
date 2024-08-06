// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CDM_CDM_ADAPTER_FACTORY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CDM_CDM_ADAPTER_FACTORY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/macros.h"
#include "third_party/chromium/media/base/cdm_factory.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/cdm/cdm_auxiliary_helper.h"

namespace media_m96 {

class MEDIA_EXPORT CdmAdapterFactory final : public CdmFactory {
 public:
  // Callback to create CdmAllocator for the created CDM.
  using HelperCreationCB =
      base::RepeatingCallback<std::unique_ptr<CdmAuxiliaryHelper>()>;

  explicit CdmAdapterFactory(HelperCreationCB helper_creation_cb);

  CdmAdapterFactory(const CdmAdapterFactory&) = delete;
  CdmAdapterFactory& operator=(const CdmAdapterFactory&) = delete;

  ~CdmAdapterFactory() override;

  // CdmFactory implementation.
  void Create(const std::string& key_system,
              const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) override;

 private:
  // Callback to create CdmAuxiliaryHelper for the created CDM.
  HelperCreationCB helper_creation_cb_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CDM_CDM_ADAPTER_FACTORY_H_
