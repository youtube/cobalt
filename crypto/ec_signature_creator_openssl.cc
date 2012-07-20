// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_signature_creator_impl.h"

#include "base/logging.h"

namespace crypto {

ECSignatureCreatorImpl::ECSignatureCreatorImpl(ECPrivateKey* key)
    : key_(key) {
  NOTIMPLEMENTED();
}

ECSignatureCreatorImpl::~ECSignatureCreatorImpl() {}

bool ECSignatureCreatorImpl::Sign(const uint8* data,
                                  int data_len,
                                  std::vector<uint8>* signature) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace crypto
