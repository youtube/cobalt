// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "components/webcrypto/algorithms/ed25519.h"

#include "components/webcrypto/algorithms/asymmetric_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace webcrypto {

namespace {
// Synthesizes an import algorithm given a key algorithm, so that
// deserialization can re-use the ImportKey*() methods.
blink::WebCryptoAlgorithm SynthesizeImportAlgorithmForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(algorithm.Id(),
                                                         nullptr);
}

Status CreateWebCryptoEd25519PrivateKey(
    base::span<const uint8_t> raw_key,
    const blink::WebCryptoKeyAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) {
  // This function accepts only the RFC 8032 format.
  DCHECK_EQ(raw_key.size(), 32u);
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, /*engine*/ nullptr, raw_key.data(), raw_key.size()));
  if (!pkey)
    return Status::OperationError();

  return webcrypto::CreateWebCryptoPrivateKey(std::move(pkey), algorithm,
                                              extractable, usages, key);
}

Status CreateWebCryptoEd25519PublicKey(
    base::span<const uint8_t> raw_key,
    const blink::WebCryptoKeyAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) {
  DCHECK_EQ(raw_key.size(), 32u);
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, /*engine*/ nullptr, raw_key.data(), raw_key.size()));
  if (!pkey)
    return Status::OperationError();

  return webcrypto::CreateWebCryptoPublicKey(std::move(pkey), algorithm,
                                             extractable, usages, key);
}

// Reads a fixed length base64url-decoded bytes from a JWK.
Status ReadBytes(const JwkReader& jwk,
                 base::StringPiece member_name,
                 size_t expected_length,
                 std::vector<uint8_t>* out) {
  std::vector<uint8_t> bytes;
  Status status = jwk.GetBytes(member_name, &bytes);
  if (status.IsError())
    return status;

  if (bytes.size() != expected_length) {
    return Status::JwkOctetStringWrongLength(member_name, expected_length,
                                             bytes.size());
  }

  *out = std::move(bytes);
  return Status::Success();
}

}  // namespace

Status Ed25519Implementation::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask combined_usages,
    GenerateKeyResult* result) const {
  blink::WebCryptoKeyUsageMask public_usages = 0;
  blink::WebCryptoKeyUsageMask private_usages = 0;

  Status status = GetUsagesForGenerateAsymmetricKey(
      combined_usages, all_public_key_usages_, all_private_key_usages_,
      &public_usages, &private_usages);
  if (status.IsError())
    return status;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Generate an Ed25519 key pair using the low-level API.
  uint8_t pubkey[32], privkey[64];
  ED25519_keypair(pubkey, privkey);

  // Ed25519 algorithm doesn't need params.
  // https://wicg.github.io/webcrypto-secure-curves/#ed25519-registration
  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::CreateEd25519(algorithm.Id());

  // Note that extractable is unconditionally set to true. This is because per
  // the WebCrypto spec generated public keys are always extractable.
  blink::WebCryptoKey public_key;
  status = CreateWebCryptoEd25519PublicKey(pubkey, key_algorithm,
                                           /*extractable=*/true, public_usages,
                                           &public_key);
  if (status.IsError())
    return status;

  // Since the RFC 8032 private key format is the 32-byte prefix of
  // |ED25519_sign|'s 64-byte private key, we can be sure we'll use the same
  // seed when regenerating the public key.
  // TODO(https://crbug.com/boringssl/521): This does a redundant base point
  // multiplication, but there aren't EVP APIs to avoid it without a lot of
  // boilerplate.
  blink::WebCryptoKey private_key;
  status = CreateWebCryptoEd25519PrivateKey(base::make_span(privkey).first(32),
                                            key_algorithm, extractable,
                                            private_usages, &private_key);
  if (status.IsError())
    return status;

  result->AssignKeyPair(public_key, private_key);
  return Status::Success();
}

Status Ed25519Implementation::ImportKey(
    blink::WebCryptoKeyFormat format,
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatRaw:
      return ImportKeyRaw(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatPkcs8:
      return ImportKeyPkcs8(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatSpki:
      return ImportKeySpki(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatJwk:
      return ImportKeyJwk(key_data, algorithm, extractable, usages, key);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status Ed25519Implementation::ExportKey(blink::WebCryptoKeyFormat format,
                                        const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatRaw:
      return ExportKeyRaw(key, buffer);
    case blink::kWebCryptoKeyFormatPkcs8:
      return ExportKeyPkcs8(key, buffer);
    case blink::kWebCryptoKeyFormatSpki:
      return ExportKeySpki(key, buffer);
    case blink::kWebCryptoKeyFormatJwk:
      return ExportKeyJwk(key, buffer);
    default:
      return Status::ErrorUnsupportedExportKeyFormat();
  }
}

Status Ed25519Implementation::Sign(const blink::WebCryptoAlgorithm& algorithm,
                                   const blink::WebCryptoKey& key,
                                   base::span<const uint8_t> message,
                                   std::vector<uint8_t>* signature) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  size_t sig_len = 64;
  signature->resize(sig_len);
  bssl::ScopedEVP_MD_CTX ctx;
  if (!EVP_DigestSignInit(ctx.get(), /*pctx=*/nullptr, /*type=*/nullptr,
                          /*e=*/nullptr, GetEVP_PKEY(key)) ||
      !EVP_DigestSign(ctx.get(), signature->data(), &sig_len, message.data(),
                      message.size())) {
    return Status::OperationError();
  }
  DCHECK_EQ(sig_len, (size_t)64);

  return Status::Success();
}

Status Ed25519Implementation::Verify(const blink::WebCryptoAlgorithm& algorithm,
                                     const blink::WebCryptoKey& key,
                                     base::span<const uint8_t> signature,
                                     base::span<const uint8_t> message,
                                     bool* signature_match) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::ScopedEVP_MD_CTX ctx;
  if (!EVP_DigestVerifyInit(ctx.get(), /*pctx=*/nullptr, /*type=*/nullptr,
                            /*e=*/nullptr, GetEVP_PKEY(key))) {
    return Status::OperationError();
  }

  *signature_match =
      1 == EVP_DigestVerify(ctx.get(), signature.data(), signature.size(),
                            message.data(), message.size());

  return Status::Success();
}

Status Ed25519Implementation::ImportKeyRaw(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  DCHECK_EQ(algorithm.Id(), blink::kWebCryptoAlgorithmIdEd25519);
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  Status status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError())
    return status;

  if (key_data.size() != 32)
    return Status::ErrorImportEd25519KeyLength();

  return CreateWebCryptoEd25519PublicKey(
      key_data, blink::WebCryptoKeyAlgorithm::CreateEd25519(algorithm.Id()),
      extractable, usages, key);
}

Status Ed25519Implementation::ImportKeyPkcs8(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_private_key_usages_, usages);
  if (status.IsError())
    return status;

  bssl::UniquePtr<EVP_PKEY> private_key;
  status =
      ImportUnverifiedPkeyFromPkcs8(key_data, EVP_PKEY_ED25519, &private_key);
  if (status.IsError())
    return status;

  return CreateWebCryptoPrivateKey(
      std::move(private_key),
      blink::WebCryptoKeyAlgorithm::CreateEd25519(algorithm.Id()), extractable,
      usages, key);
}

Status Ed25519Implementation::ImportKeySpki(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError())
    return status;

  bssl::UniquePtr<EVP_PKEY> public_key;
  status =
      ImportUnverifiedPkeyFromSpki(key_data, EVP_PKEY_ED25519, &public_key);
  if (status.IsError())
    return status;

  return CreateWebCryptoPublicKey(
      std::move(public_key),
      blink::WebCryptoKeyAlgorithm::CreateEd25519(algorithm.Id()), extractable,
      usages, key);
}

// The format for JWK Ed25519 keys is given by:
// https://www.rfc-editor.org/rfc/rfc8037#section-2
Status Ed25519Implementation::ImportKeyJwk(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  JwkReader jwk;

  // 3. If the kty field of jwk is not "OKP", then throw a DataError.
  // 5. If the alg field of jwk is present and is not "EdDSA", then throw a
  // DataError.
  // 7. If the key_ops field of jwk is present, and is invalid according to the
  // requirements of JSON Web Key [JWK], or it does not contain all of the
  // specified usages values, then throw a DataError.
  // 8. If the ext field of jwk is present and has the value false and
  // extractable is true, then throw a DataError.
  Status status = jwk.Init(key_data, extractable, usages, "OKP", "EdDSA");
  if (status.IsError())
    return status;

  // 4. If the crv field of jwk is not "Ed25519", then throw a DataError.
  std::string jwk_crv;
  status = jwk.GetString("crv", &jwk_crv);
  if (status.IsError())
    return status;
  if (jwk_crv != "Ed25519")
    return Status::ErrorJwkIncorrectCrv();

  // Only private keys have a "d" parameter. The key may still be invalid, but
  // tentatively decide if it is a public or private key.
  bool is_private_key = jwk.HasMember("d");

  // 2. If the d field is present and usages contains a value which is not
  // "sign", or, if the d field is not present and usages contains a value which
  // is not "verify" then throw a SyntaxError.
  status = is_private_key
               ? CheckKeyCreationUsages(all_private_key_usages_, usages)
               : CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError())
    return status;

  // 9.1 If jwk does not meet the requirements of Section 2 of [RFC8037], then
  // throw a DataError.
  //  + The parameter "x" MUST be present and contain the public key.
  //  + The parameter "d" MUST be present for private keys.
  std::vector<uint8_t> raw_public_key;
  status = ReadBytes(jwk, "x", 32, &raw_public_key);
  if (status.IsError())
    return status;

  // 9.2 Let key be a new CryptoKey object that represents the Ed25519
  // private/public key. 9.3. Set the [[type]] internal slot of Key to "private"
  // or "public".
  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::CreateEd25519(algorithm.Id());
  if (!is_private_key) {
    return CreateWebCryptoEd25519PublicKey(raw_public_key, key_algorithm,
                                           extractable, usages, key);
  }

  std::vector<uint8_t> raw_private_key;
  status = ReadBytes(jwk, "d", 32, &raw_private_key);
  if (status.IsError())
    return status;
  blink::WebCryptoKey private_key;
  status = CreateWebCryptoEd25519PrivateKey(raw_private_key, key_algorithm,
                                            extractable, usages, &private_key);
  if (status.IsError())
    return status;

  // Check the public key matches the private key.
  size_t len = 32;
  uint8_t raw_key[32];
  if (!EVP_PKEY_get_raw_public_key(GetEVP_PKEY(private_key), raw_key, &len))
    return Status::OperationError();
  DCHECK_EQ(len, 32u);
  if (memcmp(raw_public_key.data(), raw_key, 32) != 0)
    return Status::DataError();

  *key = private_key;
  return Status::Success();
}

Status Ed25519Implementation::ExportKeyRaw(const blink::WebCryptoKey& key,
                                           std::vector<uint8_t>* buffer) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (key.GetType() != blink::kWebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  size_t len = 32;
  buffer->resize(32);
  if (!EVP_PKEY_get_raw_public_key(GetEVP_PKEY(key), buffer->data(), &len))
    return Status::OperationError();
  DCHECK_EQ(len, buffer->size());

  return Status::Success();
}

Status Ed25519Implementation::ExportKeyPkcs8(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();

  return ExportPKeyPkcs8(GetEVP_PKEY(key), buffer);
}

Status Ed25519Implementation::ExportKeySpki(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  return ExportPKeySpki(GetEVP_PKEY(key), buffer);
}

Status Ed25519Implementation::ExportKeyJwk(const blink::WebCryptoKey& key,
                                           std::vector<uint8_t>* buffer) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EVP_PKEY* pkey = GetEVP_PKEY(key);

  size_t keylen = 32;
  uint8_t raw_public_key[32];
  if (!EVP_PKEY_get_raw_public_key(pkey, raw_public_key, &keylen))
    return Status::OperationError();
  DCHECK_EQ(keylen, sizeof(raw_public_key));

  // No "alg" is set for OKP keys.
  JwkWriter jwk(std::string(), key.Extractable(), key.Usages(), "OKP");
  jwk.SetString("crv", "Ed25519");

  // Set "x", and "d" if it is a private key.
  jwk.SetBytes("x", raw_public_key);
  if (key.GetType() == blink::kWebCryptoKeyTypePrivate) {
    uint8_t raw_private_key[32];
    if (!EVP_PKEY_get_raw_private_key(pkey, raw_private_key, &keylen))
      return Status::OperationError();
    DCHECK_EQ(keylen, sizeof(raw_private_key));

    jwk.SetBytes("d", raw_private_key);
  }

  jwk.ToJson(buffer);
  return Status::Success();
}

Status Ed25519Implementation::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    base::span<const uint8_t> key_data,
    blink::WebCryptoKey* key) const {
  // Ed25519 algorithm doesn't need params for the generateKey method.
  // https://wicg.github.io/webcrypto-secure-curves/#ed25519-registration
  if (algorithm.ParamsType() != blink::kWebCryptoKeyAlgorithmParamsTypeNone)
    return Status::ErrorUnexpected();

  blink::WebCryptoAlgorithm import_algorithm =
      SynthesizeImportAlgorithmForClone(algorithm);

  Status status;

  // The serialized data will be either SPKI or PKCS8 formatted.
  switch (type) {
    case blink::kWebCryptoKeyTypePublic:
      status =
          ImportKeySpki(key_data, import_algorithm, extractable, usages, key);
      break;
    case blink::kWebCryptoKeyTypePrivate:
      status =
          ImportKeyPkcs8(key_data, import_algorithm, extractable, usages, key);
      break;
    default:
      return Status::ErrorUnexpected();
  }

  if (!status.IsSuccess())
    return status;

  // There is some duplicated information in the serialized format used by
  // structured clone (since the KeyAlgorithm is serialized separately from the
  // key data). Use this extra information to further validate what was
  // deserialized from the key data.

  if (algorithm.Id() != key->Algorithm().Id())
    return Status::ErrorUnexpected();

  if (type != key->GetType())
    return Status::ErrorUnexpected();

  if (key->Algorithm().ParamsType() !=
      blink::kWebCryptoKeyAlgorithmParamsTypeNone) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

std::unique_ptr<AlgorithmImplementation> CreateEd25519Implementation() {
  return std::make_unique<Ed25519Implementation>();
}

}  // namespace webcrypto
