// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_creator.h"

#include <stdlib.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/cssm_init.h"

namespace crypto {

// static
SignatureCreator* SignatureCreator::Create(RSAPrivateKey* key) {
  scoped_ptr<SignatureCreator> result(new SignatureCreator);
  result->key_ = key;

  CSSM_RETURN crtn;
  crtn = CSSM_CSP_CreateSignatureContext(GetSharedCSPHandle(),
                                         CSSM_ALGID_SHA1WithRSA,
                                         NULL,
                                         key->key(),
                                         &result->sig_handle_);
  if (crtn) {
    NOTREACHED();
    return NULL;
  }

  crtn = CSSM_SignDataInit(result->sig_handle_);
  if (crtn) {
    NOTREACHED();
    return NULL;
  }

  return result.release();
}

SignatureCreator::SignatureCreator() : sig_handle_(0) {
  EnsureCSSMInit();
}

SignatureCreator::~SignatureCreator() {
  CSSM_RETURN crtn;
  if (sig_handle_) {
    crtn = CSSM_DeleteContext(sig_handle_);
    DCHECK(crtn == CSSM_OK);
  }
}

bool SignatureCreator::Update(const uint8* data_part, int data_part_len) {
  CSSM_DATA data;
  data.Data = const_cast<uint8*>(data_part);
  data.Length = data_part_len;
  CSSM_RETURN crtn = CSSM_SignDataUpdate(sig_handle_, &data, 1);
  DCHECK(crtn == CSSM_OK);
  return true;
}

bool SignatureCreator::Final(std::vector<uint8>* signature) {
  ScopedCSSMData sig;
  CSSM_RETURN crtn = CSSM_SignDataFinal(sig_handle_, sig);

  if (crtn) {
    NOTREACHED();
    return false;
  }

  signature->assign(sig->Data, sig->Data + sig->Length);
  return true;
}

}  // namespace crypto
