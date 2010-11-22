// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_cert_types.h"

#include "net/base/x509_certificate.h"
#include "base/logging.h"

namespace net {

CertPrincipal::CertPrincipal() {
}

CertPrincipal::CertPrincipal(const std::string& name) : common_name(name) {}

CertPrincipal::~CertPrincipal() {
}

std::string CertPrincipal::GetDisplayName() const {
  if (!common_name.empty())
    return common_name;
  if (!organization_names.empty())
    return organization_names[0];
  if (!organization_unit_names.empty())
    return organization_unit_names[0];

  return std::string();
}

CertPolicy::CertPolicy() {
}

CertPolicy::~CertPolicy() {
}

CertPolicy::Judgment CertPolicy::Check(
    X509Certificate* cert) const {
  // It shouldn't matter which set we check first, but we check denied first
  // in case something strange has happened.

  if (denied_.find(cert->fingerprint()) != denied_.end()) {
    // DCHECK that the order didn't matter.
    DCHECK(allowed_.find(cert->fingerprint()) == allowed_.end());
    return DENIED;
  }

  if (allowed_.find(cert->fingerprint()) != allowed_.end()) {
    // DCHECK that the order didn't matter.
    DCHECK(denied_.find(cert->fingerprint()) == denied_.end());
    return ALLOWED;
  }

  // We don't have a policy for this cert.
  return UNKNOWN;
}

void CertPolicy::Allow(X509Certificate* cert) {
  // Put the cert in the allowed set and (maybe) remove it from the denied set.
  denied_.erase(cert->fingerprint());
  allowed_.insert(cert->fingerprint());
}

void CertPolicy::Deny(X509Certificate* cert) {
  // Put the cert in the denied set and (maybe) remove it from the allowed set.
  allowed_.erase(cert->fingerprint());
  denied_.insert(cert->fingerprint());
}

bool CertPolicy::HasAllowedCert() const {
  return !allowed_.empty();
}

bool CertPolicy::HasDeniedCert() const {
  return !denied_.empty();
}

}  // namespace net
