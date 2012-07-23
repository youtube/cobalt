// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_signature_creator.h"

#include "crypto/ec_signature_creator_impl.h"

namespace crypto {

namespace {

ECSignatureCreatorFactory* g_factory_ = NULL;

}  // namespace

// static
ECSignatureCreator* ECSignatureCreator::Create(ECPrivateKey* key) {
  if (g_factory_)
    return g_factory_->Create(key);
  return new ECSignatureCreatorImpl(key);
}

// static
void ECSignatureCreator::SetFactoryForTesting(
    ECSignatureCreatorFactory* factory) {
  g_factory_ = factory;
}

}  // namespace crypto
