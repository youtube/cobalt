// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/rsa_private_key.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <pk11pub.h>

#include <list>

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"

// TODO(rafaelw): Consider refactoring common functions and definitions from
// rsa_private_key_win.cc or using NSS's ASN.1 encoder.
namespace {

static bool ReadAttribute(SECKEYPrivateKey* key,
                          CK_ATTRIBUTE_TYPE type,
                          std::vector<uint8>* output) {
  SECItem item;
  SECStatus rv;
  rv = PK11_ReadRawAttribute(PK11_TypePrivKey, key, type, &item);
  if (rv != SECSuccess) {
    NOTREACHED();
    return false;
  }

  output->assign(item.data, item.data + item.len);
  SECITEM_FreeItem(&item, PR_FALSE);
  return true;
}

}  // namespace

namespace crypto {

RSAPrivateKey::~RSAPrivateKey() {
  if (key_)
    SECKEY_DestroyPrivateKey(key_);
  if (public_key_)
    SECKEY_DestroyPublicKey(public_key_);
}

// static
RSAPrivateKey* RSAPrivateKey::Create(uint16 num_bits) {
  return CreateWithParams(num_bits,
                          PR_FALSE /* not permanent */,
                          PR_FALSE /* not sensitive */);
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitive(uint16 num_bits) {
  return CreateWithParams(num_bits,
                          PR_TRUE /* permanent */,
                          PR_TRUE /* sensitive */);
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  return CreateFromPrivateKeyInfoWithParams(input,
                                            PR_FALSE /* not permanent */,
                                            PR_FALSE /* not sensitive */);
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitiveFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  return CreateFromPrivateKeyInfoWithParams(input,
                                            PR_TRUE /* permanent */,
                                            PR_TRUE /* sensitive */);
}

// static
RSAPrivateKey* RSAPrivateKey::FindFromPublicKeyInfo(
    const std::vector<uint8>& input) {
  EnsureNSSInit();

  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  // First, decode and save the public key.
  SECItem key_der;
  key_der.type = siBuffer;
  key_der.data = const_cast<unsigned char*>(&input[0]);
  key_der.len = input.size();

  CERTSubjectPublicKeyInfo *spki =
      SECKEY_DecodeDERSubjectPublicKeyInfo(&key_der);
  if (!spki) {
    NOTREACHED();
    return NULL;
  }

  result->public_key_ = SECKEY_ExtractPublicKey(spki);
  SECKEY_DestroySubjectPublicKeyInfo(spki);
  if (!result->public_key_) {
    NOTREACHED();
    return NULL;
  }

  // Now, look for the associated private key in the user's
  // hardware-backed NSS DB.  If it's not there, consider that an
  // error.
  PK11SlotInfo *slot = GetPrivateNSSKeySlot();
  if (!slot) {
    NOTREACHED();
    return NULL;
  }

  // Make sure the key is an RSA key.  If not, that's an error
  if (result->public_key_->keyType != rsaKey) {
    PK11_FreeSlot(slot);
    NOTREACHED();
    return NULL;
  }

  SECItem *ck_id = PK11_MakeIDFromPubKey(&(result->public_key_->u.rsa.modulus));
  if (!ck_id) {
    PK11_FreeSlot(slot);
    NOTREACHED();
    return NULL;
  }

  // Finally...Look for the key!
  result->key_ = PK11_FindKeyByKeyID(slot, ck_id, NULL);

  // Cleanup...
  PK11_FreeSlot(slot);
  SECITEM_FreeItem(ck_id, PR_TRUE);

  // If we didn't find it, that's ok.
  if (!result->key_)
    return NULL;

  return result.release();
}


bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) {
  PrivateKeyInfoCodec private_key_info(true);

  // Manually read the component attributes of the private key and build up
  // the PrivateKeyInfo.
  if (!ReadAttribute(key_, CKA_MODULUS, private_key_info.modulus()) ||
      !ReadAttribute(key_, CKA_PUBLIC_EXPONENT,
          private_key_info.public_exponent()) ||
      !ReadAttribute(key_, CKA_PRIVATE_EXPONENT,
          private_key_info.private_exponent()) ||
      !ReadAttribute(key_, CKA_PRIME_1, private_key_info.prime1()) ||
      !ReadAttribute(key_, CKA_PRIME_2, private_key_info.prime2()) ||
      !ReadAttribute(key_, CKA_EXPONENT_1, private_key_info.exponent1()) ||
      !ReadAttribute(key_, CKA_EXPONENT_2, private_key_info.exponent2()) ||
      !ReadAttribute(key_, CKA_COEFFICIENT, private_key_info.coefficient())) {
    NOTREACHED();
    return false;
  }

  return private_key_info.Export(output);
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  SECItem* der_pubkey = SECKEY_EncodeDERSubjectPublicKeyInfo(public_key_);
  if (!der_pubkey) {
    NOTREACHED();
    return false;
  }

  for (size_t i = 0; i < der_pubkey->len; ++i)
    output->push_back(der_pubkey->data[i]);

  SECITEM_FreeItem(der_pubkey, PR_TRUE);
  return true;
}

RSAPrivateKey::RSAPrivateKey() : key_(NULL), public_key_(NULL) {
  EnsureNSSInit();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateWithParams(uint16 num_bits,
                                               bool permanent,
                                               bool sensitive) {
  EnsureNSSInit();

  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  PK11SlotInfo *slot = GetPrivateNSSKeySlot();
  if (!slot)
    return NULL;

  PK11RSAGenParams param;
  param.keySizeInBits = num_bits;
  param.pe = 65537L;
  result->key_ = PK11_GenerateKeyPair(slot, CKM_RSA_PKCS_KEY_PAIR_GEN, &param,
      &result->public_key_, permanent, sensitive, NULL);
  PK11_FreeSlot(slot);
  if (!result->key_)
    return NULL;

  return result.release();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfoWithParams(
    const std::vector<uint8>& input, bool permanent, bool sensitive) {
  // This method currently leaks some memory.
  // See http://crbug.com/34742.
  ANNOTATE_SCOPED_MEMORY_LEAK;
  EnsureNSSInit();

  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  PK11SlotInfo *slot = GetPrivateNSSKeySlot();
  if (!slot)
    return NULL;

  SECItem der_private_key_info;
  der_private_key_info.data = const_cast<unsigned char*>(&input.front());
  der_private_key_info.len = input.size();
  // Allow the private key to be used for key unwrapping, data decryption,
  // and signature generation.
  const unsigned int key_usage = KU_KEY_ENCIPHERMENT | KU_DATA_ENCIPHERMENT |
                                 KU_DIGITAL_SIGNATURE;
  SECStatus rv =  PK11_ImportDERPrivateKeyInfoAndReturnKey(
      slot, &der_private_key_info, NULL, NULL, permanent, sensitive,
      key_usage, &result->key_, NULL);
  PK11_FreeSlot(slot);
  if (rv != SECSuccess) {
    NOTREACHED();
    return NULL;
  }

  result->public_key_ = SECKEY_ConvertToPublicKey(result->key_);
  if (!result->public_key_) {
    NOTREACHED();
    return NULL;
  }

  return result.release();
}

}  // namespace crypto
