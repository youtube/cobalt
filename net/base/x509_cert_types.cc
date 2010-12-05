// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_cert_types.h"

#include "net/base/x509_certificate.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/time.h"

namespace net {

namespace {

// Helper for ParseCertificateDate. |*field| must contain at least
// |field_len| characters. |*field| will be advanced by |field_len| on exit.
// |*ok| is set to false if there is an error in parsing the number, but left
// untouched otherwise. Returns the parsed integer.
int ParseIntAndAdvance(const char** field, size_t field_len, bool* ok) {
  int result = 0;
  *ok &= base::StringToInt(*field, *field + field_len, &result);
  *field += field_len;
  return result;
}

}  // namespace

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

bool ParseCertificateDate(const base::StringPiece& raw_date,
                          CertDateFormat format,
                          base::Time* time) {
  size_t year_length = format == CERT_DATE_FORMAT_UTC_TIME ? 2 : 4;

  if (raw_date.length() < 11 + year_length)
    return false;

  const char* field = raw_date.data();
  bool valid = true;
  base::Time::Exploded exploded = {0};

  exploded.year =         ParseIntAndAdvance(&field, year_length, &valid);
  exploded.month =        ParseIntAndAdvance(&field, 2, &valid);
  exploded.day_of_month = ParseIntAndAdvance(&field, 2, &valid);
  exploded.hour =         ParseIntAndAdvance(&field, 2, &valid);
  exploded.minute =       ParseIntAndAdvance(&field, 2, &valid);
  exploded.second =       ParseIntAndAdvance(&field, 2, &valid);
  if (valid && year_length == 2)
    exploded.year += exploded.year < 50 ? 2000 : 1900;

  valid &= exploded.HasValidValues();

  if (!valid)
    return false;

  *time = base::Time::FromUTCExploded(exploded);
  return true;
}

}  // namespace net
