// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#include "base/logging.h"
#include "crypto/crypto_module_blocking_password_delegate.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "net/third_party/mozilla_security_manager/nsKeygenHandler.h"

// PSM = Mozilla's Personal Security Manager.
namespace psm = mozilla_security_manager;

namespace net {

std::string KeygenHandler::GenKeyAndSignChallenge() {
  // Ensure NSS is initialized.
  crypto::EnsureNSSInit();

  // TODO(mattm): allow choosing which slot to generate and store the key.
  crypto::ScopedPK11Slot slot(crypto::GetPrivateNSSKeySlot());
  if (!slot.get()) {
    LOG(ERROR) << "Couldn't get private key slot from NSS!";
    return std::string();
  }

  // Authenticate to the token.
  if (SECSuccess != PK11_Authenticate(slot.get(), PR_TRUE,
                                      crypto_module_password_delegate_.get())) {
    LOG(ERROR) << "Couldn't authenticate to private key slot!";
    return std::string();
  }

  return psm::GenKeyAndSignChallenge(key_size_in_bits_, challenge_, url_,
                                     slot.get(), stores_key_);
}

void KeygenHandler::set_crypto_module_password_delegate(
    crypto::CryptoModuleBlockingPasswordDelegate* delegate) {
  crypto_module_password_delegate_.reset(delegate);
}

}  // namespace net
