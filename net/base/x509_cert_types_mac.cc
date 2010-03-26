// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
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

static const CSSM_OID* kOIDs[] = {
    &CSSMOID_CommonName,
    &CSSMOID_LocalityName,
    &CSSMOID_StateProvinceName,
    &CSSMOID_CountryName,
    &CSSMOID_StreetAddress,
    &CSSMOID_OrganizationName,
    &CSSMOID_OrganizationalUnitName,
    &CSSMOID_DNQualifier      // This should be "DC" but is undoubtedly wrong.
};                            // TODO(avi): Find the right OID.

// Converts raw CSSM_DATA to a std::string. (Char encoding is unaltered.)
static std::string DataToString(CSSM_DATA data);

// Converts raw CSSM_DATA in ISO-8859-1 to a std::string in UTF-8.
static std::string Latin1DataToUTF8String(CSSM_DATA data);

// Converts big-endian UTF-16 to UTF-8 in a std::string.
// Note: The byte-order flipping is done in place on the input buffer!
static bool UTF16BigEndianToUTF8(char16* chars, size_t length,
                                 std::string* out_string);

// Converts big-endian UTF-32 to UTF-8 in a std::string.
// Note: The byte-order flipping is done in place on the input buffer!
static bool UTF32BigEndianToUTF8(char32* chars, size_t length,
                                 std::string* out_string);

// Adds a type+value pair to the appropriate vector from a C array.
// The array is keyed by the matching OIDs from kOIDS[].
  static void AddTypeValuePair(const CSSM_OID type,
                               const std::string& value,
                               std::vector<std::string>* values[]);

// Stores the first string of the vector, if any, to *single_value.
static void SetSingle(const std::vector<std::string> &values,
                      std::string* single_value);


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


// The following structs and templates work with Apple's very arcane and under-
// documented SecAsn1Parser API, which is apparently the same as NSS's ASN.1
// decoder:
// http://www.mozilla.org/projects/security/pki/nss/tech-notes/tn1.html

// These are used to parse the contents of a raw
// BER DistinguishedName structure.

struct KeyValuePair {
  CSSM_OID key;
  int value_type;
  CSSM_DATA value;

  enum {
    kTypeOther = 0,
    kTypePrintableString,
    kTypeIA5String,
    kTypeT61String,
    kTypeUTF8String,
    kTypeBMPString,
    kTypeUniversalString,
  };
};

static const SecAsn1Template kStringValueTemplate[] = {
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

static const SecAsn1Template kKeyValuePairTemplate[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KeyValuePair) },
  { SEC_ASN1_OBJECT_ID, offsetof(KeyValuePair, key), },
  { SEC_ASN1_INLINE, 0, &kStringValueTemplate, },
  { 0, }
};

struct KeyValuePairs {
  KeyValuePair* pairs;
};

static const SecAsn1Template kKeyValuePairSetTemplate[] = {
  { SEC_ASN1_SET_OF, offsetof(KeyValuePairs,pairs),
      kKeyValuePairTemplate, sizeof(KeyValuePairs) }
};

struct X509Name {
  KeyValuePairs** pairs_list;
};

static const SecAsn1Template kNameTemplate[] = {
  { SEC_ASN1_SEQUENCE_OF, offsetof(X509Name,pairs_list),
      kKeyValuePairSetTemplate, sizeof(X509Name) }
};

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

  for (int rdn=0; name[rdn].pairs_list; ++rdn) {
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

  
// SUBROUTINES:

static std::string DataToString(CSSM_DATA data) {
  return std::string(
      reinterpret_cast<std::string::value_type*>(data.Data),
      data.Length);
}

static std::string Latin1DataToUTF8String(CSSM_DATA data) {
  string16 utf16;
  if (!CodepageToUTF16(DataToString(data), base::kCodepageLatin1,
                       base::OnStringConversionError::FAIL, &utf16))
    return "";
  return UTF16ToUTF8(utf16);
}

bool UTF16BigEndianToUTF8(char16* chars, size_t length,
                          std::string* out_string) {
  for (size_t i = 0; i < length; i++)
    chars[i] = EndianU16_BtoN(chars[i]);
  return UTF16ToUTF8(chars, length, out_string);
}

bool UTF32BigEndianToUTF8(char32* chars, size_t length,
                          std::string* out_string) {
  for (size_t i = 0; i < length; i++)
    chars[i] = EndianS32_BtoN(chars[i]);
#if defined(WCHAR_T_IS_UTF32)
  return WideToUTF8(reinterpret_cast<const wchar_t*>(chars),
                    length, out_string);
#else
#error This code doesn't handle 16-bit wchar_t.
#endif
}

  static void AddTypeValuePair(const CSSM_OID type,
                               const std::string& value,
                               std::vector<std::string>* values[]) {
  for (size_t oid = 0; oid < arraysize(kOIDs); ++oid) {
    if (CSSMOIDEqual(&type, kOIDs[oid])) {
      values[oid]->push_back(value);
      break;
    }
  }
}

static void SetSingle(const std::vector<std::string> &values,
                      std::string* single_value) {
  // We don't expect to have more than one CN, L, S, and C.
  LOG_IF(WARNING, values.size() > 1) << "Didn't expect multiple values";
  if (values.size() > 0)
    *single_value = values[0];
}

}  // namespace net
