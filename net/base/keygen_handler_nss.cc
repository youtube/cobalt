// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#include <pk11pub.h>
#include <secmod.h>
#include <ssl.h>
#include <secder.h>    // DER_Encode()
#include <cryptohi.h>  // SEC_DerSignData()
#include <keyhi.h>     // SECKEY_CreateSubjectPublicKeyInfo()

#include "base/base64.h"
#include "base/nss_util_internal.h"
#include "base/nss_util.h"
#include "base/logging.h"

namespace net {

const int64 DEFAULT_RSA_PUBLIC_EXPONENT = 0x10001;

// Template for creating the signed public key structure to be sent to the CA.
DERTemplate SECAlgorithmIDTemplate[] = {
  { DER_SEQUENCE,
    0, NULL, sizeof(SECAlgorithmID) },
  { DER_OBJECT_ID,
    offsetof(SECAlgorithmID, algorithm), },
  { DER_OPTIONAL | DER_ANY,
    offsetof(SECAlgorithmID, parameters), },
  { 0, }
};

DERTemplate CERTSubjectPublicKeyInfoTemplate[] = {
  { DER_SEQUENCE,
    0, NULL, sizeof(CERTSubjectPublicKeyInfo) },
  { DER_INLINE,
    offsetof(CERTSubjectPublicKeyInfo, algorithm),
    SECAlgorithmIDTemplate, },
  { DER_BIT_STRING,
    offsetof(CERTSubjectPublicKeyInfo, subjectPublicKey), },
  { 0, }
};

DERTemplate CERTPublicKeyAndChallengeTemplate[] = {
  { DER_SEQUENCE,
    0, NULL, sizeof(CERTPublicKeyAndChallenge) },
  { DER_ANY,
    offsetof(CERTPublicKeyAndChallenge, spki), },
  { DER_IA5_STRING,
    offsetof(CERTPublicKeyAndChallenge, challenge), },
  { 0, }
};

void StoreKeyLocationInCache(const SECItem& public_key_info,
                             PK11SlotInfo *slot) {
  KeygenHandler::Cache* cache = KeygenHandler::Cache::GetInstance();
  KeygenHandler::KeyLocation key_location;
  const char* slot_name = PK11_GetSlotName(slot);
  key_location.slot_name.assign(slot_name);
  cache->Insert(std::string(reinterpret_cast<char*>(public_key_info.data),
                public_key_info.len), key_location);
}

bool KeygenHandler::KeyLocation::Equals(
    const net::KeygenHandler::KeyLocation& location) const {
  return slot_name == location.slot_name;
}

// This function is largely copied from the Firefox's
// <keygen> implementation in security/manager/ssl/src/nsKeygenHandler.cpp
// FIXME(gauravsh): Do we need a copy of the Mozilla license here?

std::string KeygenHandler::GenKeyAndSignChallenge() {
  // Key pair generation mechanism - only RSA is supported at present.
  PRUint32 keyGenMechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;  // from nss/pkcs11t.h

  // Temporary structures used for generating the result
  // in the right format.
  PK11SlotInfo *slot = NULL;
  PK11RSAGenParams rsaKeyGenParams;  // Keygen parameters.
  SECOidTag algTag;  // used by SEC_DerSignData().
  SECKEYPrivateKey *privateKey = NULL;
  SECKEYPublicKey *publicKey = NULL;
  CERTSubjectPublicKeyInfo *spkInfo = NULL;
  PRArenaPool *arena = NULL;
  SECStatus sec_rv =SECFailure;
  SECItem spkiItem;
  SECItem pkacItem;
  SECItem signedItem;
  CERTPublicKeyAndChallenge pkac;
  void *keyGenParams;
  pkac.challenge.data = NULL;
  bool isSuccess = true;  // Set to false as soon as a step fails.

  std::string result_blob;  // the result.

  // Ensure NSS is initialized.
  base::EnsureNSSInit();

  slot = base::GetDefaultNSSKeySlot();
  if (!slot) {
    LOG(ERROR) << "Couldn't get Internal key slot!";
    isSuccess = false;
    goto failure;
  }

  switch (keyGenMechanism) {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
      rsaKeyGenParams.keySizeInBits = key_size_in_bits_;
      rsaKeyGenParams.pe = DEFAULT_RSA_PUBLIC_EXPONENT;
      keyGenParams = &rsaKeyGenParams;

      algTag = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;  // from <nss/secoidt.h>.
      break;
    default:
      // TODO(gauravsh): If we ever support other mechanisms,
      // this can be changed.
      LOG(ERROR) << "Only RSA keygen mechanism is supported";
      isSuccess = false;
      goto failure;
      break;
  }

  // Need to make sure that the token was initialized.
  // Assume a null password.
  sec_rv = PK11_Authenticate(slot, PR_TRUE, NULL);
  if (SECSuccess != sec_rv) {
    LOG(ERROR) << "Couldn't initialze PK11 token!";
    isSuccess = false;
    goto failure;
  }

  LOG(INFO) << "Creating key pair...";
  privateKey = PK11_GenerateKeyPair(slot,
                                    keyGenMechanism,
                                    keyGenParams,
                                    &publicKey,
                                    PR_TRUE,  // isPermanent?
                                    PR_TRUE,  // isSensitive?
                                    NULL);
  LOG(INFO) << "done.";

  if (!privateKey) {
    LOG(INFO) << "Generation of Keypair failed!";
    isSuccess = false;
    goto failure;
  }

  // The CA expects the signed public key in a specific format
  // Let's create that now.

  // Create a subject public key info from the public key.
  spkInfo = SECKEY_CreateSubjectPublicKeyInfo(publicKey);
  if (!spkInfo) {
    LOG(ERROR) << "Couldn't create SubjectPublicKeyInfo from public key";
    isSuccess = false;
    goto failure;
  }

  // Temporary work store used by NSS.
  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena) {
    LOG(ERROR) << "PORT_NewArena: Couldn't allocate memory";
    isSuccess = false;
    goto failure;
  }

  // DER encode the whole subjectPublicKeyInfo.
  sec_rv = DER_Encode(arena, &spkiItem, CERTSubjectPublicKeyInfoTemplate,
                      spkInfo);
  if (SECSuccess != sec_rv) {
    LOG(ERROR) << "Couldn't DER Encode subjectPublicKeyInfo";
    isSuccess = false;
    goto failure;
  }

  // Set up the PublicKeyAndChallenge data structure, then DER encode it.
  pkac.spki = spkiItem;
  pkac.challenge.len = challenge_.length();
  pkac.challenge.data = (unsigned char *)strdup(challenge_.c_str());
  if (!pkac.challenge.data) {
    LOG(ERROR) << "Out of memory while making a copy of challenge data";
    isSuccess = false;
    goto failure;
  }
  sec_rv = DER_Encode(arena, &pkacItem, CERTPublicKeyAndChallengeTemplate,
                      &pkac);
  if (SECSuccess != sec_rv) {
    LOG(ERROR) << "Couldn't DER Encode PublicKeyAndChallenge";
    isSuccess = false;
    goto failure;
  }

  // Sign the DER encoded PublicKeyAndChallenge.
  sec_rv = SEC_DerSignData(arena, &signedItem, pkacItem.data, pkacItem.len,
                           privateKey, algTag);
  if (SECSuccess != sec_rv) {
    LOG(ERROR) << "Couldn't sign the DER encoded PublicKeyandChallenge";
    isSuccess = false;
    goto failure;
  }

  // Convert the signed public key and challenge into base64/ascii.
  if (!base::Base64Encode(std::string(reinterpret_cast<char*>(signedItem.data),
                                      signedItem.len),
                          &result_blob)) {
    LOG(ERROR) << "Couldn't convert signed public key into base64";
    isSuccess = false;
    goto failure;
  }

  StoreKeyLocationInCache(spkiItem, slot);

 failure:
  if (!isSuccess) {
    LOG(ERROR) << "SSL Keygen failed!";
  } else {
    LOG(INFO) << "SSL Keygen succeeded!";
  }

  // Do cleanups
  if (privateKey) {
    // On successful keygen we need to keep the private key, of course,
    // or we won't be able to use the client certificate.
    if (!isSuccess || !stores_key_) {
      PK11_DestroyTokenObject(privateKey->pkcs11Slot, privateKey->pkcs11ID);
    }
    SECKEY_DestroyPrivateKey(privateKey);
  }

  if (publicKey) {
    if (!isSuccess || !stores_key_) {
      PK11_DestroyTokenObject(publicKey->pkcs11Slot, publicKey->pkcs11ID);
    }
    SECKEY_DestroyPublicKey(publicKey);
  }
  if (spkInfo) {
    SECKEY_DestroySubjectPublicKeyInfo(spkInfo);
  }
  if (arena) {
    PORT_FreeArena(arena, PR_TRUE);
  }
  if (slot != NULL) {
    PK11_FreeSlot(slot);
  }
  if (pkac.challenge.data) {
    free(pkac.challenge.data);
  }

  return (isSuccess ? result_blob : std::string());
}

}  // namespace net
