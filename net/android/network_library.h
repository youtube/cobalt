// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_LIBRARY_H_
#define NET_ANDROID_NETWORK_LIBRARY_H_
#pragma once

#include <jni.h>

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace net {
namespace android {

enum VerifyResult {
  // Certificate verification was successful.
  VERIFY_OK,
  // Certificate domain name doesn't match host name.
  VERIFY_BAD_HOSTNAME,
  // Certificate verification was failed. There is no detail error information
  // given by Android API.
  VERIFY_NO_TRUSTED_ROOT,
  // Error occurs when invoke JNI methods.
  VERIFY_INVOCATION_ERROR,
};

// |cert_chain| is DER encoded chain of certificates, with the server's own
// certificate listed first.
// |hostname| is validated against the supplied cert. |auth_type| is as per
// the Java X509Certificate.checkServerTrusted method.

VerifyResult VerifyX509CertChain(const std::vector<std::string>& cert_chain,
                                 const std::string& hostname,
                                 const std::string& auth_type);

// Helper for the <keygen> handler. Passes the DER-encoded key  pair via
// JNI to the Credentials store.
bool StoreKeyPair(const uint8* public_key,
                  size_t public_len,
                  const uint8* private_key,
                  size_t private_len);

// Get the mime type (if any) that is associated with the file extension.
// Returns true if a corresponding mime type exists.
bool GetMimeTypeFromExtension(const std::string& extension,
                              std::string* result);

// Register JNI methods
bool RegisterNetworkLibrary(JNIEnv* env);

}  // namespace android
}  // namespace net

#endif  // NET_ANDROID_NETWORK_LIBRARY_H_
