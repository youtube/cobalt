// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_CRYPTO_MODULE_BLOCKING_PASSWORD_DELEGATE_H_
#define CRYPTO_CRYPTO_MODULE_BLOCKING_PASSWORD_DELEGATE_H_
#pragma once

#include <string>

namespace crypto {

// PK11_SetPasswordFunc is a global setting.  An implementation of
// CryptoModuleBlockingPasswordDelegate should be passed as the user data
// argument (|wincx|) to relevant NSS functions, which the global password
// handler will call to do the actual work.
class CryptoModuleBlockingPasswordDelegate {
 public:
  virtual ~CryptoModuleBlockingPasswordDelegate() {}

  // Requests a password to unlock |slot_name|. The interface is
  // synchronous because NSS cannot issue an asynchronous
  // request. |retry| is true if this is a request for the retry
  // and we previously returned the wrong password.
  // The implementation should set |*cancelled| to true if the user cancelled
  // instead of entering a password, otherwise it should return the password the
  // user entered.
  virtual std::string RequestPassword(const std::string& slot_name, bool retry,
                                      bool* cancelled) = 0;
};

}  // namespace crypto

#endif  // CRYPTO_CRYPTO_MODULE_BLOCKING_PASSWORD_DELEGATE_H_
