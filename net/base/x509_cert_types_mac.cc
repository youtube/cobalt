// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_cert_types.h"

#include <CoreServices/CoreServices.h>
#include <Security/Security.h>
#include <Security/SecAsn1Coder.h>

#include "base/logging.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/utf_string_conversions.h"

namespace net {

namespace {

// The BER encoding of 0.9.2342.19200300.100.1.25.
// On 10.6 and later this is available as CSSMOID_DomainComponent, which is an
// external symbol from Security.framework. However, it appears that Apple's
// implementation improperly encoded this on 10.6+, and even still is
// unavailable on 10.5, so simply including the raw BER here.
//
// Note: CSSM is allowed to store CSSM_OIDs in any arbitrary format desired,
// as long as the symbols are properly exposed. The fact that Apple's
// implementation stores it in BER is an internal implementation detail
// observed by studying libsecurity_cssm.
const uint8 kDomainComponentData[] = {
  0x09, 0x92, 0x26, 0x89, 0x93, 0xF2, 0x2C, 0x64, 0x01, 0x19
};

const CSSM_OID kDomainComponentOID = {
    arraysize(kDomainComponentData),
    const_cast<uint8*>(kDomainComponentData)
};

const CSSM_OID* kOIDs[] = {
    &CSSMOID_CommonName,
    &CSSMOID_LocalityName,
    &CSSMOID_StateProvinceName,
    &CSSMOID_CountryName,
    &CSSMOID_StreetAddress,
    &CSSMOID_OrganizationName,
    &CSSMOID_OrganizationalUnitName,
    &kDomainComponentOID,
};

// The following structs and templates work with Apple's very arcane and under-
// documented SecAsn1Parser API, which is apparently the same as NSS's ASN.1
// decoder:
// http://www.mozilla.org/projects/security/pki/nss/tech-notes/tn1.html

// These are used to parse the contents of a raw
// BER DistinguishedName structure.

struct KeyValuePair {
  enum ValueType {
    kTypeOther = 0,
    kTypePrintableString,
    kTypeIA5String,
    kTypeT61String,
    kTypeUTF8String,
    kTypeBMPString,
    kTypeUniversalString,
  };

  CSSM_OID key;
  ValueType value_type;
  CSSM_DATA value;
};

const SecAsn1Template kStringValueTemplate[] = {
  { SEC_ASN1_CHOICE, offsetof(KeyValuePair, value_type), },
  { SEC_ASN1_PRINTABLE_STRING,
    offsetof(KeyValuePair, value), 0, KeyValuePair::kTypePrintableString },
  { SEC_ASN1_IA5_STRING,
    offsetof(KeyValuePair, value), 0, KeyValuePair::kTypeIA5String },
  { SEC_ASN1_T61_STRING,
    offsetof(KeyValuePair, value), 0, KeyValuePair::kTypeT61String },
  { SEC_ASN1_UTF8_STRING,
    offsetof(KeyValuePair, value), 0, KeyValuePair::kTypeUTF8String },
  { SEC_ASN1_BMP_STRING,
    offsetof(KeyValuePair, value), 0, KeyValuePair::kTypeBMPString },
  { SEC_ASN1_UNIVERSAL_STRING,
    offsetof(KeyValuePair, value), 0, KeyValuePair::kTypeUniversalString },
  { 0, }
};

const SecAsn1Template kKeyValuePairTemplate[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KeyValuePair) },
  { SEC_ASN1_OBJECT_ID, offsetof(KeyValuePair, key), },
  { SEC_ASN1_INLINE, 0, &kStringValueTemplate, },
  { 0, }
};

struct KeyValuePairs {
  KeyValuePair* pairs;
};

const SecAsn1Template kKeyValuePairSetTemplate[] = {
  { SEC_ASN1_SET_OF, offsetof(KeyValuePairs, pairs),
      kKeyValuePairTemplate, sizeof(KeyValuePairs) }
};

struct X509Name {
  KeyValuePairs** pairs_list;
};

const SecAsn1Template kNameTemplate[] = {
  { SEC_ASN1_SEQUENCE_OF, offsetof(X509Name, pairs_list),
      kKeyValuePairSetTemplate, sizeof(X509Name) }
};

// Converts raw CSSM_DATA to a std::string. (Char encoding is unaltered.)
std::string DataToString(CSSM_DATA data) {
  return std::string(
      reinterpret_cast<std::string::value_type*>(data.Data),
      data.Length);
}

// Converts raw CSSM_DATA in ISO-8859-1 to a std::string in UTF-8.
std::string Latin1DataToUTF8String(CSSM_DATA data) {
  string16 utf16;
  if (!CodepageToUTF16(DataToString(data), base::kCodepageLatin1,
                       base::OnStringConversionError::FAIL, &utf16))
    return "";
  return UTF16ToUTF8(utf16);
}

// Converts big-endian UTF-16 to UTF-8 in a std::string.
// Note: The byte-order flipping is done in place on the input buffer!
bool UTF16BigEndianToUTF8(char16* chars, size_t length,
                          std::string* out_string) {
  for (size_t i = 0; i < length; i++)
    chars[i] = EndianU16_BtoN(chars[i]);
  return UTF16ToUTF8(chars, length, out_string);
}

// Converts big-endian UTF-32 to UTF-8 in a std::string.
// Note: The byte-order flipping is done in place on the input buffer!
bool UTF32BigEndianToUTF8(char32* chars, size_t length,
                          std::string* out_string) {
  for (size_t i = 0; i < length; ++i)
    chars[i] = EndianS32_BtoN(chars[i]);
#if defined(WCHAR_T_IS_UTF32)
  return WideToUTF8(reinterpret_cast<const wchar_t*>(chars),
                    length, out_string);
#else
#error This code doesn't handle 16-bit wchar_t.
#endif
}

// Adds a type+value pair to the appropriate vector from a C array.
// The array is keyed by the matching OIDs from kOIDS[].
void AddTypeValuePair(const CSSM_OID type,
                      const std::string& value,
                      std::vector<std::string>* values[]) {
  for (size_t oid = 0; oid < arraysize(kOIDs); ++oid) {
    if (CSSMOIDEqual(&type, kOIDs[oid])) {
      values[oid]->push_back(value);
      break;
    }
  }
}

// Stores the first string of the vector, if any, to *single_value.
void SetSingle(const std::vector<std::string>& values,
               std::string* single_value) {
  // We don't expect to have more than one CN, L, S, and C.
  LOG_IF(WARNING, values.size() > 1) << "Didn't expect multiple values";
  if (!values.empty())
    *single_value = values[0];
}

bool match(const std::string& str, const std::string& against) {
  // TODO(snej): Use the full matching rules specified in RFC 5280 sec. 7.1
  // including trimming and case-folding: <http://www.ietf.org/rfc/rfc5280.txt>.
  return against == str;
}

bool match(const std::vector<std::string>& rdn1,
           const std::vector<std::string>& rdn2) {
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

}  // namespace

bool CertPrincipal::ParseDistinguishedName(const void* ber_name_data,
                                           size_t length) {
  DCHECK(ber_name_data);

  // First parse the BER |name_data| into the above structs.
  SecAsn1CoderRef coder = NULL;
  SecAsn1CoderCreate(&coder);
  DCHECK(coder);
  X509Name* name = NULL;
  OSStatus err = SecAsn1Decode(coder, ber_name_data, length, kNameTemplate,
                               &name);
  if (err) {
    LOG(ERROR) << "SecAsn1Decode returned " << err << "; name=" << name;
    SecAsn1CoderRelease(coder);
    return false;
  }

  // Now scan the structs and add the values to my string vectors.
  // I don't store multiple common/locality/state/country names, so use
  // temporary vectors for those.
  std::vector<std::string> common_names, locality_names, state_names,
      country_names;
  std::vector<std::string>* values[] = {
      &common_names, &locality_names,
      &state_names, &country_names,
      &this->street_addresses,
      &this->organization_names,
      &this->organization_unit_names,
      &this->domain_components
  };
  DCHECK(arraysize(kOIDs) == arraysize(values));

  for (int rdn = 0; name[rdn].pairs_list; ++rdn) {
    KeyValuePair *pair;
    for (int pair_index = 0;
         NULL != (pair = name[rdn].pairs_list[0][pair_index].pairs);
         ++pair_index) {
      switch (pair->value_type) {
        case KeyValuePair::kTypeIA5String:          // ASCII (that means 7-bit!)
        case KeyValuePair::kTypePrintableString:    // a subset of ASCII
        case KeyValuePair::kTypeUTF8String:         // UTF-8
          AddTypeValuePair(pair->key, DataToString(pair->value), values);
          break;
        case KeyValuePair::kTypeT61String:          // T61, pretend it's Latin-1
          AddTypeValuePair(pair->key,
                           Latin1DataToUTF8String(pair->value),
                           values);
          break;
        case KeyValuePair::kTypeBMPString: {        // UTF-16, big-endian
          std::string value;
          UTF16BigEndianToUTF8(reinterpret_cast<char16*>(pair->value.Data),
                               pair->value.Length / sizeof(char16),
                               &value);
          AddTypeValuePair(pair->key, value, values);
          break;
        }
        case KeyValuePair::kTypeUniversalString: {  // UTF-32, big-endian
          std::string value;
          UTF32BigEndianToUTF8(reinterpret_cast<char32*>(pair->value.Data),
                               pair->value.Length / sizeof(char32),
                               &value);
          AddTypeValuePair(pair->key, value, values);
          break;
        }
        default:
          DCHECK_EQ(pair->value_type, KeyValuePair::kTypeOther);
          // We don't know what data type this is, but we'll store it as a blob.
          // Displaying the string may not work, but at least it can be compared
          // byte-for-byte by a Matches() call.
          AddTypeValuePair(pair->key, DataToString(pair->value), values);
          break;
      }
    }
  }

  SetSingle(common_names, &this->common_name);
  SetSingle(locality_names, &this->locality_name);
  SetSingle(state_names, &this->state_or_province_name);
  SetSingle(country_names, &this->country_name);

  // Releasing |coder| frees all the memory pointed to via |name|.
  SecAsn1CoderRelease(coder);
  return true;
}

void CertPrincipal::Parse(const CSSM_X509_NAME* name) {
  std::vector<std::string> common_names, locality_names, state_names,
      country_names;

  std::vector<std::string>* values[] = {
      &common_names, &locality_names,
      &state_names, &country_names,
      &(this->street_addresses),
      &(this->organization_names),
      &(this->organization_unit_names),
      &(this->domain_components)
  };
  DCHECK(arraysize(kOIDs) == arraysize(values));

  for (size_t rdn = 0; rdn < name->numberOfRDNs; ++rdn) {
    CSSM_X509_RDN rdn_struct = name->RelativeDistinguishedName[rdn];
    for (size_t pair = 0; pair < rdn_struct.numberOfPairs; ++pair) {
      CSSM_X509_TYPE_VALUE_PAIR pair_struct =
          rdn_struct.AttributeTypeAndValue[pair];
      AddTypeValuePair(pair_struct.type,
                       DataToString(pair_struct.value),
                       values);
    }
  }

  SetSingle(common_names, &this->common_name);
  SetSingle(locality_names, &this->locality_name);
  SetSingle(state_names, &this->state_or_province_name);
  SetSingle(country_names, &this->country_name);
}

bool CertPrincipal::Matches(const CertPrincipal& against) const {
  return match(common_name, against.common_name) &&
      match(locality_name, against.locality_name) &&
      match(state_or_province_name, against.state_or_province_name) &&
      match(country_name, against.country_name) &&
      match(street_addresses, against.street_addresses) &&
      match(organization_names, against.organization_names) &&
      match(organization_unit_names, against.organization_unit_names) &&
      match(domain_components, against.domain_components);
}

}  // namespace net
