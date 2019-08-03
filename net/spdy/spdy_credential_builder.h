// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_CREDENTIAL_BUILDER_H_
#define NET_SPDY_SPDY_CREDENTIAL_BUILDER_H_

#include <string>

#include "net/base/net_export.h"
#include "net/base/ssl_client_cert_type.h"

namespace net {

class SSLClientSocket;
struct SpdyCredential;

// This class provides facilities for building the various fields of
// SPDY CREDENTIAL frames.
class NET_EXPORT_PRIVATE SpdyCredentialBuilder {
 public:
  static int Build(const std::string& tls_unique,
                   SSLClientCertType type,
                   const std::string& key,
                   const std::string& cert,
                   size_t slot,
                   SpdyCredential* credential);

 private:
  friend class SpdyCredentialBuilderTest;

  // Returns the secret data to be signed as part of a credential frame.
  static std::string GetCredentialSecret(const std::string& tls_unique);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_CREDENTIAL_BUILDER_H_
