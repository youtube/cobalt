// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/android_cdm_factory.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_config.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_system_names.h"
#include "media/base/media_switches.h"
#include "media/cdm/aes_decryptor.h"
#include "url/origin.h"

namespace media {

namespace {

void ReportMediaDrmBridgeKeySystemSupport(bool supported) {
  UMA_HISTOGRAM_BOOLEAN("Media.EME.MediaDrmBridge.KeySystemSupport", supported);
}

}  // namespace

AndroidCdmFactory::AndroidCdmFactory(CreateFetcherCB create_fetcher_cb,
                                     CreateStorageCB create_storage_cb)
    : create_fetcher_cb_(std::move(create_fetcher_cb)),
      create_storage_cb_(std::move(create_storage_cb)) {}

AndroidCdmFactory::~AndroidCdmFactory() {
  weak_factory_.InvalidateWeakPtrs();
  for (auto& pending_creation : pending_creations_) {
    CdmCreatedCB cdm_created_cb = std::move(pending_creation.second.second);
    std::move(cdm_created_cb).Run(nullptr, "CDM creation aborted");
  }
}

void AndroidCdmFactory::Create(
    const std::string& key_system,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  DVLOG(1) << __func__;

  // Bound |cdm_created_cb| so we always fire it asynchronously.
  CdmCreatedCB bound_cdm_created_cb =
      BindToCurrentLoop(std::move(cdm_created_cb));

  // Create AesDecryptor here to support External Clear Key key system.
  // This is used for testing.
  if (base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting) &&
      IsExternalClearKey(key_system)) {
    scoped_refptr<ContentDecryptionModule> cdm(
        new AesDecryptor(session_message_cb, session_closed_cb,
                         session_keys_change_cb, session_expiration_update_cb));
    std::move(bound_cdm_created_cb).Run(cdm, "");
    return;
  }

  std::string error_message;

  if (!MediaDrmBridge::IsKeySystemSupported(key_system)) {
    ReportMediaDrmBridgeKeySystemSupport(false);
    std::move(bound_cdm_created_cb)
        .Run(nullptr, "Key system not supported unexpectedly: " + key_system);
    return;
  }

  ReportMediaDrmBridgeKeySystemSupport(true);

  auto factory = std::make_unique<MediaDrmBridgeFactory>(create_fetcher_cb_,
                                                         create_storage_cb_);
  auto* raw_factory = factory.get();

  creation_id_++;
  auto result = pending_creations_.emplace(
      creation_id_,
      PendingCreation(std::move(factory), std::move(bound_cdm_created_cb)));
  CHECK(result.second);

  raw_factory->Create(key_system, cdm_config, session_message_cb,
                      session_closed_cb, session_keys_change_cb,
                      session_expiration_update_cb,
                      base::BindOnce(&AndroidCdmFactory::OnCdmCreated,
                                     weak_factory_.GetWeakPtr(), creation_id_));
}

void AndroidCdmFactory::OnCdmCreated(
    uint32_t creation_id,
    const scoped_refptr<ContentDecryptionModule>& cdm,
    const std::string& error_message) {
  DVLOG(1) << __func__ << ": creation_id = " << creation_id;

  DCHECK(pending_creations_.count(creation_id));
  CdmCreatedCB cdm_created_cb =
      std::move(pending_creations_[creation_id].second);
  pending_creations_.erase(creation_id);

  LOG_IF(ERROR, !cdm) << error_message;
  std::move(cdm_created_cb).Run(cdm, error_message);
}

}  // namespace media
