// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_INFO_H_
#define NET_BASE_SSL_INFO_H_
#pragma once

#include "base/ref_counted.h"

namespace net {

class X509Certificate;

// SSL connection info.
// This is really a struct.  All members are public.
class SSLInfo {
 public:
  SSLInfo();
  SSLInfo(const SSLInfo& info);
  ~SSLInfo();
  SSLInfo& operator=(const SSLInfo& info);

  void Reset();

  bool is_valid() const { return cert != NULL; }

  // Adds the specified |error| to the cert status.
  void SetCertError(int error);

  // The SSL certificate.
  scoped_refptr<X509Certificate> cert;

  // Bitmask of status info of |cert|, representing, for example, known errors
  // and extended validation (EV) status.
  // See cert_status_flags.h for values.
  int cert_status;

  // The security strength, in bits, of the SSL cipher suite.
  // 0 means the connection is not encrypted.
  // -1 means the security strength is unknown.
  int security_bits;

  // Information about the SSL connection itself. See
  // ssl_connection_status_flags.h for values. The protocol version,
  // ciphersuite, and compression in use are encoded within.
  int connection_status;
};

}  // namespace net

#endif  // NET_BASE_SSL_INFO_H_
