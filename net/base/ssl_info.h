// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_INFO_H_
#define NET_BASE_SSL_INFO_H_

#include <string>

#include "net/base/cert_status_flags.h"
#include "net/base/x509_certificate.h"

namespace net {

// SSL connection info.
// This is really a struct.  All members are public.
class SSLInfo {
 public:
  // Next Protocol Negotiation (NPN) allows a TLS client and server to come to
  // an agreement about the application level protocol to speak over a
  // connection.  See also the next_protos field in SSLConfig.
  enum NextProtoStatus {
    kNextProtoUnsupported = 0,  // The server doesn't support NPN.
    kNextProtoNegotiated = 1,   // We agreed on a protocol, see next_proto
    kNextProtoNoOverlap = 2,    // No protocols in common. We requested
                                // |next_proto|.
  };

  SSLInfo() : cert_status(0), security_bits(-1),
              next_proto_status(kNextProtoUnsupported) { }

  void Reset() {
    cert = NULL;
    security_bits = -1;
    cert_status = 0;
    next_proto.clear();
    next_proto_status = kNextProtoUnsupported;
  }

  bool is_valid() const { return cert != NULL; }

  // Adds the specified |error| to the cert status.
  void SetCertError(int error) {
    cert_status |= MapNetErrorToCertStatus(error);
  }

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

  NextProtoStatus next_proto_status;  // One of kNextProto*
  std::string next_proto; // The negotiated protocol, if any.
};

}  // namespace net

#endif  // NET_BASE_SSL_INFO_H_
