// Copyright (c) 2007-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_CERT_TYPES_H_
#define NET_BASE_X509_CERT_TYPES_H_

#include <string.h>

#include <functional>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#elif defined(OS_MACOSX)
#include <Security/x509defs.h>
#elif defined(USE_NSS)
// Forward declaration; real one in <cert.h>
struct CERTCertificateStr;
#endif

namespace net {

class X509Certificate;

// SHA-1 fingerprint (160 bits) of a certificate.
struct SHA1Fingerprint {
  bool Equals(const SHA1Fingerprint& other) const {
    return memcmp(data, other.data, sizeof(data)) == 0;
  }

  unsigned char data[20];
};

class SHA1FingerprintLessThan
    : public std::binary_function<SHA1Fingerprint, SHA1Fingerprint, bool> {
 public:
  bool operator() (const SHA1Fingerprint& lhs,
                   const SHA1Fingerprint& rhs) const {
    return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) < 0;
  }
};

// CertPrincipal represents the issuer or subject field of an X.509 certificate.
struct CertPrincipal {
  CertPrincipal() { }
  explicit CertPrincipal(const std::string& name) : common_name(name) { }

  // Parses a BER-format DistinguishedName.
  bool ParseDistinguishedName(const void* ber_name_data, size_t length);

#if defined(OS_MACOSX)
  // Parses a CSSM_X509_NAME struct.
  void Parse(const CSSM_X509_NAME* name);
#endif

  // Returns true if all attributes of the two objects match,
  // where "match" is defined in RFC 5280 sec. 7.1.
  bool Matches(const CertPrincipal& against) const;

  // The different attributes for a principal.  They may be "".
  // Note that some of them can have several values.

  std::string common_name;
  std::string locality_name;
  std::string state_or_province_name;
  std::string country_name;

  std::vector<std::string> street_addresses;
  std::vector<std::string> organization_names;
  std::vector<std::string> organization_unit_names;
  std::vector<std::string> domain_components;
};

// Writes a human-readable description of a CertPrincipal, for debugging.
std::ostream& operator<<(std::ostream& s, const CertPrincipal& p);

// This class is useful for maintaining policies about which certificates are
// permitted or forbidden for a particular purpose.
class CertPolicy {
 public:
  // The judgments this policy can reach.
  enum Judgment {
    // We don't have policy information for this certificate.
    UNKNOWN,

    // This certificate is allowed.
    ALLOWED,

    // This certificate is denied.
    DENIED,
  };

  // Returns the judgment this policy makes about this certificate.
  Judgment Check(X509Certificate* cert) const;

  // Causes the policy to allow this certificate.
  void Allow(X509Certificate* cert);

  // Causes the policy to deny this certificate.
  void Deny(X509Certificate* cert);

  // Returns true if this policy has allowed at least one certificate.
  bool HasAllowedCert() const;

  // Returns true if this policy has denied at least one certificate.
  bool HasDeniedCert() const;

 private:
  // The set of fingerprints of allowed certificates.
  std::set<SHA1Fingerprint, SHA1FingerprintLessThan> allowed_;

  // The set of fingerprints of denied certificates.
  std::set<SHA1Fingerprint, SHA1FingerprintLessThan> denied_;
};

#if defined(OS_MACOSX)
// Compares two OIDs by value.
inline bool CSSMOIDEqual(const CSSM_OID* oid1, const CSSM_OID* oid2) {
  return oid1->Length == oid2->Length &&
  (memcmp(oid1->Data, oid2->Data, oid1->Length) == 0);
}
#endif

}  // namespace net

#endif  // NET_BASE_X509_CERT_TYPES_H_
