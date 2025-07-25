// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/certificate/net_parsed_certificate.h"

#include "base/containers/contains.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_util.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace openscreen::cast {

// static
ErrorOr<std::unique_ptr<ParsedCertificate>> ParsedCertificate::ParseFromDER(
    const std::vector<uint8_t>& der_cert) {
  std::shared_ptr<const net::ParsedCertificate> cert =
      net::ParsedCertificate::Create(net::x509_util::CreateCryptoBuffer(
                                         base::span<const uint8_t>(der_cert)),
                                     cast_certificate::GetCertParsingOptions(),
                                     nullptr);
  if (!cert) {
    return Error::Code::kErrCertsParse;
  }
  std::unique_ptr<ParsedCertificate> result =
      std::make_unique<::cast_certificate::NetParsedCertificate>(
          std::move(cert));
  return result;
}

}  // namespace openscreen::cast

namespace cast_certificate {

using openscreen::ErrorOr;
using openscreen::cast::DateTime;
using openscreen::cast::DigestAlgorithm;

namespace {

// Helper that extracts the Common Name from a certificate's subject field. On
// success |common_name| contains the text for the attribute (UTF-8, but for
// Cast device certs it should be ASCII).
bool GetCommonNameFromSubject(const net::der::Input& subject_tlv,
                              std::string* common_name) {
  net::RDNSequence rdn_sequence;
  if (!net::ParseName(subject_tlv, &rdn_sequence))
    return false;

  for (const net::RelativeDistinguishedName& relative_distinguished_name :
       rdn_sequence) {
    for (const auto& attribute_type_value : relative_distinguished_name) {
      if (attribute_type_value.type ==
          net::der::Input(net::kTypeCommonNameOid)) {
        return attribute_type_value.ValueAsString(common_name);
      }
    }
  }
  return false;
}

}  // namespace

net::ParseCertificateOptions GetCertParsingOptions() {
  net::ParseCertificateOptions options;

  // Some cast intermediate certificates contain serial numbers that are
  // 21 octets long, and might also not use valid DER encoding for an
  // INTEGER (non-minimal encoding).
  //
  // Allow these sorts of serial numbers.
  //
  // TODO(eroman): At some point in the future this workaround will no longer be
  // necessary. Should revisit this for removal in 2017 if not earlier.  We will
  // probably want an UMA histogram to be certain.
  options.allow_invalid_serial_numbers = true;
  return options;
}

NetParsedCertificate::NetParsedCertificate(
    std::shared_ptr<const net::ParsedCertificate> cert)
    : cert_(std::move(cert)) {}

NetParsedCertificate::~NetParsedCertificate() = default;

ErrorOr<std::vector<uint8_t>> NetParsedCertificate::SerializeToDER(
    int front_spacing) const {
  std::vector<uint8_t> result;
  base::span<const uint8_t> der_buffer = cert_->der_cert().AsSpan();
  result.reserve(front_spacing + der_buffer.size());
  result.resize(front_spacing);
  result.insert(result.end(), der_buffer.begin(), der_buffer.end());
  return result;
}

ErrorOr<DateTime> NetParsedCertificate::GetNotBeforeTime() const {
  const net::der::GeneralizedTime& t = cert_->tbs().validity_not_before;
  DateTime result;
  result.year = t.year;
  result.month = t.month;
  result.day = t.day;
  result.hour = t.hours;
  result.minute = t.minutes;
  result.second = t.seconds;
  return result;
}

ErrorOr<DateTime> NetParsedCertificate::GetNotAfterTime() const {
  const net::der::GeneralizedTime& t = cert_->tbs().validity_not_after;
  DateTime result;
  result.year = t.year;
  result.month = t.month;
  result.day = t.day;
  result.hour = t.hours;
  result.minute = t.minutes;
  result.second = t.seconds;
  return result;
}

std::string NetParsedCertificate::GetCommonName() const {
  std::string common_name;
  if (!cert_ ||
      !GetCommonNameFromSubject(cert_->tbs().subject_tlv, &common_name)) {
    return {};
  }
  return common_name;
}

std::string NetParsedCertificate::GetSpkiTlv() const {
  return cert_->tbs().spki_tlv.AsString();
}

ErrorOr<uint64_t> NetParsedCertificate::GetSerialNumber() const {
  uint64_t serial_number;
  if (!net::der::ParseUint64(cert_->tbs().serial_number, &serial_number)) {
    return openscreen::Error::Code::kErrCertsParse;
  }
  return serial_number;
}

bool NetParsedCertificate::VerifySignedData(
    DigestAlgorithm algorithm,
    const openscreen::ByteView& data,
    const openscreen::ByteView& signature) const {
  // TODO(davidben): This function only uses BoringSSL functions and the SPKI,
  // which is already exported as GetSpkiTlv(). Remove this method altogether
  // and move this into openscreen.
  CBS spki;
  CBS_init(&spki, cert_->tbs().spki_tlv.UnsafeData(),
           cert_->tbs().spki_tlv.Length());
  bssl::UniquePtr<EVP_PKEY> pubkey(EVP_parse_public_key(&spki));
  if (!pubkey || CBS_len(&spki) != 0) {
    return false;
  }

  const EVP_MD* digest;
  switch (algorithm) {
    case DigestAlgorithm::kSha1:
      digest = EVP_sha1();
      break;
    case DigestAlgorithm::kSha256:
      digest = EVP_sha256();
      break;
    case DigestAlgorithm::kSha384:
      digest = EVP_sha384();
      break;
    case DigestAlgorithm::kSha512:
      digest = EVP_sha512();
      break;
    default:
      return false;
  }

  // Verify with RSASSA-PKCS1-v1_5 and |digest|.
  bssl::ScopedEVP_MD_CTX ctx;
  return EVP_PKEY_id(pubkey.get()) == EVP_PKEY_RSA &&
         EVP_DigestVerifyInit(ctx.get(), nullptr, digest, nullptr,
                              pubkey.get()) &&
         EVP_DigestVerify(ctx.get(), signature.data(), signature.size(),
                          data.data(), data.size());
}

bool NetParsedCertificate::HasPolicyOid(const openscreen::ByteView& oid) const {
  if (!cert_->has_policy_oids()) {
    return false;
  }
  const std::vector<net::der::Input>& policies = cert_->policy_oids();
  return base::Contains(policies, net::der::Input(oid));
}

void NetParsedCertificate::SetNotBeforeTimeForTesting(time_t not_before) {
  CHECK(
      net::EncodeTimeAsGeneralizedTime(base::Time::FromTimeT(not_before),
                                       const_cast<net::der::GeneralizedTime*>(
                                           &cert_->tbs().validity_not_before)));
}

void NetParsedCertificate::SetNotAfterTimeForTesting(time_t not_after) {
  CHECK(net::EncodeTimeAsGeneralizedTime(
      base::Time::FromTimeT(not_after), const_cast<net::der::GeneralizedTime*>(
                                            &cert_->tbs().validity_not_after)));
}

}  // namespace cast_certificate
