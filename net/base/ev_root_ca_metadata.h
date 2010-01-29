// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_EV_ROOT_CA_METADATA_H_
#define NET_BASE_EV_ROOT_CA_METADATA_H_

#include "build/build_config.h"

#if defined(USE_NSS)
#include <secoidt.h>
#endif

#include <map>
#include <vector>

#include "net/base/x509_certificate.h"

template <typename T>
struct DefaultSingletonTraits;

namespace net {

// A singleton.  This class stores the meta data of the root CAs that issue
// extended-validation (EV) certificates.
class EVRootCAMetadata {
 public:
#if defined(USE_NSS)
  typedef SECOidTag PolicyOID;
#else
  typedef const char* PolicyOID;
#endif

  static EVRootCAMetadata* GetInstance();

  // If the root CA cert has an EV policy OID, returns true and stores the
  // policy OID in *policy_oid.  Otherwise, returns false.
  bool GetPolicyOID(const X509Certificate::Fingerprint& fingerprint,
                    PolicyOID* policy_oid) const;

  const PolicyOID* GetPolicyOIDs() const { return &policy_oids_[0]; }
  int NumPolicyOIDs() const { return policy_oids_.size(); }

 private:
  EVRootCAMetadata();
  ~EVRootCAMetadata() { }

  friend struct DefaultSingletonTraits<EVRootCAMetadata>;

  typedef std::map<X509Certificate::Fingerprint, PolicyOID,
                   X509Certificate::FingerprintLessThan> PolicyOidMap;

  // Maps an EV root CA cert's SHA-1 fingerprint to its EV policy OID.
  PolicyOidMap ev_policy_;

  std::vector<PolicyOID> policy_oids_;

  DISALLOW_COPY_AND_ASSIGN(EVRootCAMetadata);
};

}  // namespace net

#endif  // NET_BASE_EV_ROOT_CA_METADATA_H_
