// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/asn1_util.h"

namespace net {

namespace asn1 {

bool ParseElement(base::StringPiece* in,
                  unsigned tag_value,
                  base::StringPiece* out,
                  unsigned *out_header_len) {
  const uint8* data = reinterpret_cast<const uint8*>(in->data());

  if (in->size() < 2)
    return false;

  if (static_cast<unsigned char>(data[0]) != tag_value)
    return false;

  size_t len = 0;
  if ((data[1] & 0x80) == 0) {
    // short form length
    if (out_header_len)
      *out_header_len = 2;
    len = static_cast<size_t>(data[1]) + 2;
  } else {
    // long form length
    const unsigned num_bytes = data[1] & 0x7f;
    if (num_bytes == 0 || num_bytes > 2)
      return false;
    if (in->size() < 2 + num_bytes)
      return false;
    len = data[2];
    if (num_bytes == 2) {
      if (len == 0) {
        // the length encoding must be minimal.
        return false;
      }
      len <<= 8;
      len += data[3];
    }
    if (len < 128) {
      // the length should have been encoded in short form. This distinguishes
      // DER from BER encoding.
      return false;
    }
    if (out_header_len)
      *out_header_len = 2 + num_bytes;
    len += 2 + num_bytes;
  }

  if (in->size() < len)
    return false;
  if (out)
    *out = base::StringPiece(in->data(), len);
  in->remove_prefix(len);
  return true;
}

bool GetElement(base::StringPiece* in,
                unsigned tag_value,
                base::StringPiece* out) {
  unsigned header_len;
  if (!ParseElement(in, tag_value, out, &header_len))
    return false;
  if (out)
    out->remove_prefix(header_len);
  return true;
}

bool ExtractSPKIFromDERCert(base::StringPiece cert,
                            base::StringPiece* spki_out) {
  // From RFC 5280, section 4.1
  //    Certificate  ::=  SEQUENCE  {
  //      tbsCertificate       TBSCertificate,
  //      signatureAlgorithm   AlgorithmIdentifier,
  //      signatureValue       BIT STRING  }

  // TBSCertificate  ::=  SEQUENCE  {
  //      version         [0]  EXPLICIT Version DEFAULT v1,
  //      serialNumber         CertificateSerialNumber,
  //      signature            AlgorithmIdentifier,
  //      issuer               Name,
  //      validity             Validity,
  //      subject              Name,
  //      subjectPublicKeyInfo SubjectPublicKeyInfo,

  base::StringPiece certificate;
  if (!asn1::GetElement(&cert, asn1::kSEQUENCE, &certificate))
    return false;

  base::StringPiece tbs_certificate;
  if (!asn1::GetElement(&certificate, asn1::kSEQUENCE, &tbs_certificate))
    return false;

  // The version is optional, so a failure to parse it is fine.
  asn1::GetElement(&tbs_certificate,
                   asn1::kCompound | asn1::kContextSpecific | 0,
                   NULL);

  // serialNumber
  if (!asn1::GetElement(&tbs_certificate, asn1::kINTEGER, NULL))
    return false;
  // signature
  if (!asn1::GetElement(&tbs_certificate, asn1::kSEQUENCE, NULL))
    return false;
  // issuer
  if (!asn1::GetElement(&tbs_certificate, asn1::kSEQUENCE, NULL))
    return false;
  // validity
  if (!asn1::GetElement(&tbs_certificate, asn1::kSEQUENCE, NULL))
    return false;
  // subject
  if (!asn1::GetElement(&tbs_certificate, asn1::kSEQUENCE, NULL))
    return false;
  // subjectPublicKeyInfo
  if (!asn1::ParseElement(&tbs_certificate, asn1::kSEQUENCE, spki_out, NULL))
    return false;
  return true;
}

} // namespace asn1

} // namespace net
