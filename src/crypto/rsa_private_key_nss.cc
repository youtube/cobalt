// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/rsa_private_key.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <secmod.h>

#include <list>

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"

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

  CERTSubjectPublicKeyInfo* spki =
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

  // Make sure the key is an RSA key.  If not, that's an error
  if (result->public_key_->keyType != rsaKey) {
    NOTREACHED();
    return NULL;
  }

  ScopedSECItem ck_id(
      PK11_MakeIDFromPubKey(&(result->public_key_->u.rsa.modulus)));
  if (!ck_id.get()) {
    NOTREACHED();
    return NULL;
  }

  // Search all slots in all modules for the key with the given ID.
  AutoSECMODListReadLock auto_lock;
  SECMODModuleList* head = SECMOD_GetDefaultModuleList();
  for (SECMODModuleList* item = head; item != NULL; item = item->next) {
    int slot_count = item->module->loaded ? item->module->slotCount : 0;
    for (int i = 0; i < slot_count; i++) {
      // Finally...Look for the key!
      result->key_ = PK11_FindKeyByKeyID(item->module->slots[i],
                                         ck_id.get(), NULL);
      if (result->key_)
        return result.release();
    }
  }

  // We didn't find the key.
  return NULL;
}

RSAPrivateKey* RSAPrivateKey::Copy() const {
  RSAPrivateKey* copy = new RSAPrivateKey();
  copy->key_ = SECKEY_CopyPrivateKey(key_);
  copy->public_key_ = SECKEY_CopyPublicKey(public_key_);
  return copy;
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) const {
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

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) const {
  ScopedSECItem der_pubkey(SECKEY_EncodeDERSubjectPublicKeyInfo(public_key_));
  if (!der_pubkey.get()) {
    NOTREACHED();
    return false;
  }

  output->assign(der_pubkey->data, der_pubkey->data + der_pubkey->len);
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

  ScopedPK11Slot slot(GetPrivateNSSKeySlot());
  if (!slot.get())
    return NULL;

  PK11RSAGenParams param;
  param.keySizeInBits = num_bits;
  param.pe = 65537L;
  result->key_ = PK11_GenerateKeyPair(slot.get(),
                                      CKM_RSA_PKCS_KEY_PAIR_GEN,
                                      &param,
                                      &result->public_key_,
                                      permanent,
                                      sensitive,
                                      NULL);
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

  ScopedPK11Slot slot(GetPrivateNSSKeySlot());
  if (!slot.get())
    return NULL;

  SECItem der_private_key_info;
  der_private_key_info.data = const_cast<unsigned char*>(&input.front());
  der_private_key_info.len = input.size();
  // Allow the private key to be used for key unwrapping, data decryption,
  // and signature generation.
  const unsigned int key_usage = KU_KEY_ENCIPHERMENT | KU_DATA_ENCIPHERMENT |
                                 KU_DIGITAL_SIGNATURE;
  SECStatus rv =  PK11_ImportDERPrivateKeyInfoAndReturnKey(
      slot.get(), &der_private_key_info, NULL, NULL, permanent, sensitive,
      key_usage, &result->key_, NULL);
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
