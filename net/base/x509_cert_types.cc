// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_cert_types.h"

#include <ostream>

#include "net/base/x509_certificate.h"
#include "base/logging.h"

namespace net {

bool match(const std::string &str, const std::string &against) {
  // TODO(snej): Use the full matching rules specified in RFC 5280 sec. 7.1
  // including trimming and case-folding: <http://www.ietf.org/rfc/rfc5280.txt>.
  return against == str;
}

bool match(const std::vector<std::string> &rdn1,
           const std::vector<std::string> &rdn2) {
  // "Two relative distinguished names RDN1 and RDN2 match if they have the
  // same number of naming attributes and for each naming attribute in RDN1
  // there is a matching naming attribute in RDN2." --RFC 5280 sec. 7.1.
  if (rdn1.size() != rdn2.size())
    return false;
  for (unsigned i1 = 0; i1 < rdn1.size(); ++i1) {
    unsigned i2;
    for (i2 = 0; i2 < rdn2.size(); ++i2) {
      if (match(rdn1[i1], rdn2[i2]))
          break;
    }
    if (i2 == rdn2.size())
      return false;
  }
  return true;
}


bool CertPrincipal::Matches(const CertPrincipal& against) const {
  return match(common_name, against.common_name) &&
      match(common_name, against.common_name) &&
      match(locality_name, against.locality_name) &&
      match(state_or_province_name, against.state_or_province_name) &&
      match(country_name, against.country_name) &&
      match(street_addresses, against.street_addresses) &&
      match(organization_names, against.organization_names) &&
      match(organization_unit_names, against.organization_unit_names) &&
      match(domain_components, against.domain_components);
}

std::ostream& operator<<(std::ostream& s, const CertPrincipal& p) {
  s << "CertPrincipal[";
  if (!p.common_name.empty())
    s << "cn=\"" << p.common_name << "\" ";
  for (unsigned i = 0; i < p.street_addresses.size(); ++i)
    s << "street=\"" << p.street_addresses[i] << "\" ";
  if (!p.locality_name.empty())
    s << "l=\"" << p.locality_name << "\" ";
  for (unsigned i = 0; i < p.organization_names.size(); ++i)
    s << "o=\"" << p.organization_names[i] << "\" ";
  for (unsigned i = 0; i < p.organization_unit_names.size(); ++i)
    s << "ou=\"" << p.organization_unit_names[i] << "\" ";
  if (!p.state_or_province_name.empty())
    s << "st=\"" << p.state_or_province_name << "\" ";
  if (!p.country_name.empty())
    s << "c=\"" << p.country_name << "\" ";
  for (unsigned i = 0; i < p.domain_components.size(); ++i)
    s << "dc=\"" << p.domain_components[i] << "\" ";
  return s << "]";
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
