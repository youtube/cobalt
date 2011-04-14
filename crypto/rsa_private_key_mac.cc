// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/rsa_private_key.h"

#include <list>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/cssm_init.h"

namespace crypto {

// static
RSAPrivateKey* RSAPrivateKey::Create(uint16 num_bits) {
  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  CSSM_CC_HANDLE cc_handle;
  CSSM_RETURN crtn;
  crtn = CSSM_CSP_CreateKeyGenContext(GetSharedCSPHandle(), CSSM_ALGID_RSA,
                                      num_bits, NULL, NULL, NULL, NULL, NULL,
                                      &cc_handle);
  if (crtn) {
    NOTREACHED() << "CSSM_CSP_CreateKeyGenContext failed: " << crtn;
    return NULL;
  }

  CSSM_DATA label = { 9,
      const_cast<uint8*>(reinterpret_cast<const uint8*>("temp_key")) };
  crtn = CSSM_GenerateKeyPair(cc_handle,
      CSSM_KEYUSE_VERIFY,
      CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE, &label,
      result->public_key(), CSSM_KEYUSE_SIGN,
      CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE, &label, NULL,
      result->key());
  CSSM_DeleteContext(cc_handle);
  if (crtn) {
    NOTREACHED() << "CSSM_CSP_CreateKeyGenContext failed: " << crtn;
    return NULL;
  }

  return result.release();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitive(uint16 num_bits) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  if (input.empty())
    return NULL;

  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  CSSM_KEY key;
  memset(&key, 0, sizeof(key));
  key.KeyData.Data = const_cast<uint8*>(&input.front());
  key.KeyData.Length = input.size();
  key.KeyHeader.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
  key.KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
  key.KeyHeader.BlobType = CSSM_KEYBLOB_RAW;
  key.KeyHeader.AlgorithmId = CSSM_ALGID_RSA;
  key.KeyHeader.KeyClass = CSSM_KEYCLASS_PRIVATE_KEY;
  key.KeyHeader.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
  key.KeyHeader.KeyUsage = CSSM_KEYUSE_ANY;

  CSSM_KEY_SIZE key_size;
  CSSM_RETURN crtn;
  crtn = CSSM_QueryKeySizeInBits(GetSharedCSPHandle(), NULL, &key, &key_size);
  if (crtn) {
    NOTREACHED() << "CSSM_QueryKeySizeInBits failed: " << crtn;
    return NULL;
  }
  key.KeyHeader.LogicalKeySizeInBits = key_size.LogicalKeySizeInBits;

  // Perform a NULL unwrap operation on the key so that result's key_
  // instance variable points to a key that can be released via CSSM_FreeKey().
  CSSM_ACCESS_CREDENTIALS creds;
  memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
  CSSM_CC_HANDLE cc_handle;
  crtn = CSSM_CSP_CreateSymmetricContext(GetSharedCSPHandle(), CSSM_ALGID_NONE,
      CSSM_ALGMODE_NONE, &creds, NULL, NULL, CSSM_PADDING_NONE, 0, &cc_handle);
  if (crtn) {
    NOTREACHED() << "CSSM_CSP_CreateSymmetricContext failed: " << crtn;
    return NULL;
  }
  CSSM_DATA label_data, desc_data = { 0, NULL };
  label_data.Data =
      const_cast<uint8*>(reinterpret_cast<const uint8*>("unwrapped"));
  label_data.Length = 9;
  crtn = CSSM_UnwrapKey(cc_handle, NULL, &key, CSSM_KEYUSE_ANY,
      CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE, &label_data,
      NULL, result->key(), &desc_data);
  if (crtn) {
    NOTREACHED() << "CSSM_UnwrapKey failed: " << crtn;
    return NULL;
  }

  // Extract a public key from the private key.
  // Apple doesn't accept CSSM_KEYBLOB_RAW_FORMAT_X509 as a valid key
  // format when attempting to generate certs, so use PKCS1 instead.
  PrivateKeyInfoCodec codec(true);
  std::vector<uint8> private_key_data;
  private_key_data.assign(key.KeyData.Data,
                          key.KeyData.Data + key.KeyData.Length);
  if (!codec.Import(private_key_data)) {
    return NULL;
  }
  std::vector<uint8> public_key_data;
  if (!codec.ExportPublicKey(&public_key_data)) {
    return NULL;
  }

  CSSM_KEY* public_key = result->public_key();
  size_t size = public_key_data.size();
  public_key->KeyData.Data = reinterpret_cast<uint8*>(CSSMMalloc(size));
  if (!public_key->KeyData.Data) {
    NOTREACHED() << "CSSMMalloc failed";
    return NULL;
  }
  memcpy(public_key->KeyData.Data, &public_key_data.front(), size);
  public_key->KeyData.Length = size;
  public_key->KeyHeader.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
  public_key->KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
  public_key->KeyHeader.BlobType = CSSM_KEYBLOB_RAW;
  public_key->KeyHeader.AlgorithmId = CSSM_ALGID_RSA;
  public_key->KeyHeader.KeyClass = CSSM_KEYCLASS_PUBLIC_KEY;
  public_key->KeyHeader.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
  public_key->KeyHeader.KeyUsage = CSSM_KEYUSE_ANY;

  crtn = CSSM_QueryKeySizeInBits(GetSharedCSPHandle(), NULL, public_key,
                                 &key_size);
  if (crtn) {
    DLOG(ERROR) << "CSSM_QueryKeySizeInBits failed " << crtn;
    return NULL;
  }
  public_key->KeyHeader.LogicalKeySizeInBits = key_size.LogicalKeySizeInBits;

  return result.release();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitiveFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
RSAPrivateKey* RSAPrivateKey::FindFromPublicKeyInfo(
    const std::vector<uint8>& input) {
  NOTIMPLEMENTED();
  return NULL;
}

RSAPrivateKey::RSAPrivateKey() {
  memset(&key_, 0, sizeof(key_));
  memset(&public_key_, 0, sizeof(public_key_));

  EnsureCSSMInit();
}

RSAPrivateKey::~RSAPrivateKey() {
  if (key_.KeyData.Data) {
    CSSM_FreeKey(GetSharedCSPHandle(), NULL, &key_, CSSM_FALSE);
  }
  if (public_key_.KeyData.Data) {
    CSSM_FreeKey(GetSharedCSPHandle(), NULL, &public_key_, CSSM_FALSE);
  }
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) {
  if (!key_.KeyData.Data || !key_.KeyData.Length) {
    return false;
  }
  output->clear();
  output->insert(output->end(), key_.KeyData.Data,
                key_.KeyData.Data + key_.KeyData.Length);
  return true;
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  PrivateKeyInfoCodec private_key_info(true);
  std::vector<uint8> private_key_data;
  private_key_data.assign(key_.KeyData.Data,
                          key_.KeyData.Data + key_.KeyData.Length);
  return (private_key_info.Import(private_key_data) &&
          private_key_info.ExportPublicKeyInfo(output));
}

}  // namespace crypto
