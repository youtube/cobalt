// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_FAKE_KEYCHAIN_H_
#define DEVICE_FIDO_MAC_FAKE_KEYCHAIN_H_

#include <string>
#include <vector>

#import <Foundation/Foundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/component_export.h"
#include "device/fido/mac/keychain.h"

namespace device::fido::mac {

// FakeKeychain is an implementation of the Keychain API for testing. It works
// around behavior that can't be relied on in tests, such as writing to the
// actual Keychain or using functionality that requires code-signed, entitled
// builds.
class COMPONENT_EXPORT(DEVICE_FIDO) FakeKeychain : public Keychain {
 public:
  explicit FakeKeychain(const std::string& keychain_access_group);
  FakeKeychain(const FakeKeychain&) = delete;
  FakeKeychain& operator=(const FakeKeychain&) = delete;
  ~FakeKeychain() override;

  // Keychain:
  base::apple::ScopedCFTypeRef<SecKeyRef> KeyCreateRandomKey(
      CFDictionaryRef params,
      CFErrorRef* error) override;
  OSStatus ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result) override;
  OSStatus ItemDelete(CFDictionaryRef query) override;
  OSStatus ItemUpdate(CFDictionaryRef query,
                      base::apple::ScopedCFTypeRef<CFMutableDictionaryRef>
                          keychain_data) override;

 private:
  // items_ contains the keychain items created by `KeyCreateRandomKey`.
  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> items_;
  // keychain_access_group_ is the value of `kSecAttrAccessGroup` that this
  // keychain expects to operate on.
  base::apple::ScopedCFTypeRef<CFStringRef> keychain_access_group_;
};

// ScopedFakeKeychain installs itself as testing override for
// `Keychain::GetInstance()`.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedFakeKeychain : public FakeKeychain {
 public:
  explicit ScopedFakeKeychain(const std::string& keychain_access_group);
  ~ScopedFakeKeychain() override;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_FAKE_KEYCHAIN_H_
