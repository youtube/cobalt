// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_cdm_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "media/mojo/clients/mojo_cdm.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

namespace {

void OnCdmCreated(
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb,
    mojo::PendingRemote<mojom::ContentDecryptionModule> cdm_remote,
    media::mojom::CdmContextPtr cdm_context,
    const std::string& error_message) {
  // Convert from a PendingRemote to Remote so we can verify that it is
  // connected, this will also check if |cdm_remote| is null.
  mojo::Remote<mojom::ContentDecryptionModule> remote(std::move(cdm_remote));
  if (!remote || !remote.is_connected() || !cdm_context) {
    std::move(cdm_created_cb).Run(nullptr, error_message);
    return;
  }

  std::move(cdm_created_cb)
      .Run(base::MakeRefCounted<MojoCdm>(
               std::move(remote), std::move(cdm_context), session_message_cb,
               session_closed_cb, session_keys_change_cb,
               session_expiration_update_cb),
           "");
}

}  // namespace

MojoCdmFactory::MojoCdmFactory(
    media::mojom::InterfaceFactory* interface_factory)
    : interface_factory_(interface_factory) {
  DCHECK(interface_factory_);
}

MojoCdmFactory::~MojoCdmFactory() = default;

void MojoCdmFactory::Create(
    const std::string& key_system,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  DVLOG(2) << __func__ << ": " << key_system;

  // If AesDecryptor can be used, always use it here in the local process.
  // Note: We should not run AesDecryptor in the browser process except for
  // testing. See http://crbug.com/441957.
  // Note: Previously MojoRenderer doesn't work with local CDMs, this has
  // been solved by using DecryptingRenderer. See http://crbug.com/913775.
  if (CanUseAesDecryptor(key_system)) {
    scoped_refptr<ContentDecryptionModule> cdm(
        new AesDecryptor(session_message_cb, session_closed_cb,
                         session_keys_change_cb, session_expiration_update_cb));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cdm_created_cb), cdm, ""));
    return;
  }

  interface_factory_->CreateCdm(
      key_system, cdm_config,
      base::BindOnce(&OnCdmCreated, session_message_cb, session_closed_cb,
                     session_keys_change_cb, session_expiration_update_cb,
                     std::move(cdm_created_cb)));
}

}  // namespace media
