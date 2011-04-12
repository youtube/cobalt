// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ASN1_UTIL_H_
#define NET_BASE_ASN1_UTIL_H_
#pragma once

#include "base/string_piece.h"

namespace net {

namespace asn1 {

// These are the DER encodings of the tag byte for ASN.1 objects.
static const unsigned kINTEGER = 0x02;
static const unsigned kOID = 0x06;
static const unsigned kSEQUENCE = 0x30;

// These are flags that can be ORed with the above tag numbers.
static const unsigned kContextSpecific = 0x80;
static const unsigned kCompound = 0x20;

// ParseElement parses a DER encoded ASN1 element from |in|, requiring that
// it have the given |tag_value|. It returns true on success. The following
// limitations are imposed:
//   1) tag numbers > 31 are not permitted.
//   2) lengths > 65535 are not permitted.
// On successful return:
//   |in| is advanced over the element
//   |out| contains the element, including the tag and length bytes.
//   |out_header_len| contains the length of the tag and length bytes in |out|.
bool ParseElement(base::StringPiece* in,
                  unsigned tag_value,
                  base::StringPiece* out,
                  unsigned *out_header_len);

// GetElement performs the same actions as ParseElement, except that the header
// bytes are not included in the output.
bool GetElement(base::StringPiece* in,
                unsigned tag_value,
                base::StringPiece* out);


// ExtractSPKIFromDERCert parses the DER encoded certificate in |cert| and
// extracts the bytes of the SubjectPublicKeyInfo. On successful return,
// |spki_out| is set to contain the SPKI, pointing into |cert|.
bool ExtractSPKIFromDERCert(base::StringPiece cert,
                            base::StringPiece* spki_out);

} // namespace asn1

} // namespace net

#endif // NET_BASE_ASN1_UTIL_H_
