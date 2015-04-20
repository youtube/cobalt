// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_cert_types.h"

#include <cstdlib>
#include <cstring>

#include "base/logging.h"
#include "base/sha1.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/time.h"
#include "net/base/x509_certificate.h"

namespace net {

namespace {

// Helper for ParseCertificateDate. |*field| must contain at least
// |field_len| characters. |*field| will be advanced by |field_len| on exit.
// |*ok| is set to false if there is an error in parsing the number, but left
// untouched otherwise. Returns the parsed integer.
int ParseIntAndAdvance(const char** field, size_t field_len, bool* ok) {
  int result = 0;
  *ok &= base::StringToInt(base::StringPiece(*field, field_len), &result);
  *field += field_len;
  return result;
}

// CompareSHA1Hashes is a helper function for using bsearch() with an array of
// SHA1 hashes.
int CompareSHA1Hashes(const void* a, const void* b) {
  return memcmp(a, b, base::kSHA1Length);
}

}  // namespace

// static
bool IsSHA1HashInSortedArray(const SHA1HashValue& hash,
                             const uint8* array,
                             size_t array_byte_len) {
  DCHECK_EQ(0u, array_byte_len % base::kSHA1Length);
  const size_t arraylen = array_byte_len / base::kSHA1Length;
  return NULL != bsearch(hash.data, array, arraylen, base::kSHA1Length,
                         CompareSHA1Hashes);
}

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

bool HashValue::Equals(const HashValue& other) const {
  if (tag != other.tag)
    return false;
  switch (tag) {
    case HASH_VALUE_SHA1:
      return fingerprint.sha1.Equals(other.fingerprint.sha1);
    case HASH_VALUE_SHA256:
      return fingerprint.sha256.Equals(other.fingerprint.sha256);
    default:
      NOTREACHED() << "Unknown HashValueTag " << tag;
      return false;
  }
}

size_t HashValue::size() const {
  switch (tag) {
    case HASH_VALUE_SHA1:
      return sizeof(fingerprint.sha1.data);
    case HASH_VALUE_SHA256:
      return sizeof(fingerprint.sha256.data);
    default:
      NOTREACHED() << "Unknown HashValueTag " << tag;
      // Although this is NOTREACHED, this function might be inlined and its
      // return value can be passed to memset as the length argument. If we
      // returned 0 here, it might result in what appears (in some stages of
      // compilation) to be a call to to memset with a length argument of 0,
      // which results in a warning. Therefore, we return a dummy value
      // here.
      return sizeof(fingerprint.sha1.data);
  }
}

unsigned char* HashValue::data() {
  return const_cast<unsigned char*>(const_cast<const HashValue*>(this)->data());
}

const unsigned char* HashValue::data() const {
  switch (tag) {
    case HASH_VALUE_SHA1:
      return fingerprint.sha1.data;
    case HASH_VALUE_SHA256:
      return fingerprint.sha256.data;
    default:
      NOTREACHED() << "Unknown HashValueTag " << tag;
      return NULL;
  }
}

}  // namespace net
