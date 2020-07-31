// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <set>
#include <string>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"

#include "cobalt/subtlecrypto/crypto_impl.h"
#include "cobalt/subtlecrypto/subtle_crypto.h"

namespace cobalt {
namespace subtlecrypto {

namespace {

const ByteVector to_vector(const dom::BufferSource &data) {
  const uint8_t *buff;
  int buf_len;
  cobalt::dom::GetBufferAndSize(data, &buff, &buf_len);
  return ByteVector(buff, buff + buf_len);
}

std::string algo_name(const Algorithm &algo) {
  return algo.has_name() ? algo.name() : "";
}

template <typename T, typename W>
std::string get_name(const W &algorithm) {
  if (algorithm.template IsType<T>()) {
    return algo_name(algorithm.template AsType<T>());
  }
  return std::is_same<T, Algorithm>::value ? ""
                                           : get_name<Algorithm>(algorithm);
}

template <typename T = Algorithm, typename W>
std::string get_name_or_string(const W &algorithm) {
  if (algorithm.template IsType<T>()) {
    return algo_name(algorithm.template AsType<T>());
  }
  return algorithm.template AsType<std::string>();
}

template <typename Promise>
Promise reject(Promise &&promise,
               const scoped_refptr<script::ScriptException> &result) {
  promise->Reject(result);
  return promise;
}

template <typename Promise>
Promise reject(Promise &&promise, dom::DOMException::ExceptionCode error) {
  return reject(promise, new dom::DOMException(error));
}

template <typename Promise>
Promise reject(Promise &&promise, const std::string &dom_error) {
  return reject(promise, new dom::DOMException(dom_error, dom_error));
}

}  // namespace

using PromiseBool = SubtleCrypto::PromiseBool;
using PromiseArray = SubtleCrypto::PromiseArray;
using PromiseWrappable = SubtleCrypto::PromiseWrappable;

SubtleCrypto::SubtleCrypto(script::EnvironmentSettings *environment) {
  dom::DOMSettings *dom_settings =
      base::polymorphic_downcast<dom::DOMSettings *>(environment);
  global_env_ = dom_settings->global_environment();
  script_value_factory_ = global_env_->script_value_factory();
}

SubtleCrypto::~SubtleCrypto() {}

PromiseWrappable SubtleCrypto::CreateKeyPromise() {
  DCHECK(script_value_factory_);
  return script_value_factory_
      ->CreateInterfacePromise<scoped_refptr<CryptoKeyPtr>>();
}

dom::BufferSource SubtleCrypto::CreateBufferSource(const ByteVector &input) {
  DCHECK(global_env_);
  auto arrayBuffer =
      script::ArrayBuffer::New(global_env_, input.data(), input.size());
  return dom::BufferSource(arrayBuffer);
}

PromiseArray SubtleCrypto::Decrypt(EncryptionAlgorithm algorithm,
                                   const CryptoKeyPtr &key,
                                   const BufferSource &data) {
  // 1. Let algorithm and key be the algorithm and key parameters passed to the
  // decrypt method, respectively.
  // 2. Let data be the result of getting a copy of the bytes held by the data
  // parameter passed to the decrypt method.
  // 3. Let normalizedAlgorithm be the result of normalizing an algorithm, with
  // alg set to algorithm and op set to "decrypt".
  std::string normalizedAlgorithm = get_name<AesCtrParams>(algorithm);
  // 5. Let promise be a new Promise.
  auto promise = CreatePromise();
  // 4. If an error occurred, return a Promise rejected with
  // normalizedAlgorithm.
  if (normalizedAlgorithm != key->get_algorithm()) {
    return reject(promise, DOMException::kInvalidAccessErr);
  }
  // 9. If the [[usages]] internal slot of key does not contain an entry that is
  // "decrypt", then throw an InvalidAccessError.
  if (!key->usage(KeyUsage::kKeyUsageDecrypt)) {
    return reject(promise, DOMException::kInvalidAccessErr);
  }
  if (key->get_key().size() == 0) {
    return reject(promise, DOMException::kInvalidAccessErr);
  }
  if (normalizedAlgorithm == "AES-CTR") {
    auto ctr_params = algorithm.AsType<AesCtrParams>();
    if (!(ctr_params.has_length() && ctr_params.has_counter())) {
      return reject(promise, DOMException::kValidationErr);
    }
    auto iv = to_vector(ctr_params.counter());
    auto counter_len = ctr_params.length();
    // 25.7.1 If the counter member of normalizedAlgorithm does not have length
    // 16 bytes, then throw an OperationError.
    // 25.7.2 If the length member of normalizedAlgorithm is zero or is greater
    // than 128, then throw an OperationError.
    if (counter_len < 1 || counter_len > 128 || iv.size() != 16) {
      return reject(promise, "OperationError");
    }
    // 10. Let plaintext be the result of performing the decrypt operation
    // specified by normalizedAlgorithm using key and algorithm and with
    // data as ciphertext.
    auto out = CalculateAES_CTR(to_vector(data), key->get_key(), iv);
    if (out.empty()) {
      reject(promise, "Internal error");
    }
    // 11. Resolve promise with plaintext.
    promise->Resolve(CreateBufferSource(out));
  } else {
    // 7. If the following steps or referenced procedures say to throw an error,
    // reject promise with the returned error and then terminate the algorithm.
    return reject(promise, DOMException::kNotSupportedErr);
  }
  // 6. Return promise and asynchronously perform the remaining steps.
  return promise;
}

// TODO: Consider sharing the implementation with Decrypt, as the flow is
// very similar for symmetric algos.
PromiseArray SubtleCrypto::Encrypt(EncryptionAlgorithm algorithm,
                                   const CryptoKeyPtr &key,
                                   const BufferSource &data) {
  // 1. Let algorithm and key be the algorithm and key parameters passed to the
  // encrypt method, respectively.
  // 2. Let data be the result of getting a copy of the bytes held by the data
  // parameter passed to the encrypt method.
  // 3. Let normalizedAlgorithm be the result of normalizing an algorithm, with
  // alg set to algorithm and op set to "encrypt".
  std::string normalizedAlgorithm = get_name<AesCtrParams>(algorithm);
  // 5. Let promise be a new Promise.
  auto promise = CreatePromise();
  // 4. If an error occurred, return a Promise rejected with
  // normalizedAlgorithm.
  if (normalizedAlgorithm != key->get_algorithm()) {
    return reject(promise, DOMException::kInvalidAccessErr);
  }
  // 9. If the [[usages]] internal slot of key does not contain an entry that is
  // "encrypt", then throw an InvalidAccessError.
  if (!key->usage(KeyUsage::kKeyUsageEncrypt)) {
    return reject(promise, DOMException::kInvalidAccessErr);
  }
  if (key->get_key().size() == 0) {
    return reject(promise, DOMException::kInvalidAccessErr);
  }
  if (normalizedAlgorithm == "AES-CTR") {
    auto ctr_params = algorithm.AsType<AesCtrParams>();
    if (!(ctr_params.has_length() && ctr_params.has_counter())) {
      return reject(promise, DOMException::kValidationErr);
    }
    auto iv = to_vector(ctr_params.counter());
    auto counter_len = ctr_params.length();
    // 25.7.1 If the counter member of normalizedAlgorithm does not have length
    // 16 bytes, then throw an OperationError.
    // 25.7.2 If the length member of normalizedAlgorithm is zero or is greater
    // than 128, then throw an OperationError.
    if (counter_len < 1 || counter_len > 128 || iv.size() != 16) {
      return reject(promise, "OperationError");
    }
    // 10. Let ciphertext be the result of performing the encrypt operation
    // specified by normalizedAlgorithm using algorithm and key and with data
    // as plaintext.
    auto out = CalculateAES_CTR(to_vector(data), key->get_key(), iv);
    if (out.empty()) {
      reject(promise, "Internal error");
    }
    // 11. Resolve promise with ciphertext.
    promise->Resolve(CreateBufferSource(out));
  } else {
    // 7. If the following steps or referenced procedures say to throw an error,
    // reject promise with the returned error and then terminate the algorithm.
    return reject(promise, DOMException::kNotSupportedErr);
  }
  // 6. Return promise and asynchronously perform the remaining steps.
  return promise;
}

PromiseArray SubtleCrypto::Sign(AlgorithmIdentifier algorithm,
                                const CryptoKeyPtr &key,
                                const BufferSource &data) {
  // 1. Let algorithm and key be the algorithm and key parameters passed to the
  // sign method, respectively.
  // 2. Let data be the result of getting a copy of the bytes held by the data
  // parameter passed to the sign method.
  // 3. Let normalizedAlgorithm be the result of normalizing an algorithm, with
  // alg set to algorithm and op set to "sign".
  std::string normalizedAlgorithm = get_name_or_string(algorithm);
  // 5. Let promise be a new Promise.
  auto promise = CreatePromise();
  if (normalizedAlgorithm == "HMAC") {
    auto hash = Hash::CreateByName(key->get_hash());
    // 9. If the [[usages]] internal slot of key does not contain an entry that
    // is "sign", then throw an InvalidAccessError.
    if (!key->usage(KeyUsage::kKeyUsageSign) || !hash) {
      return reject(promise, DOMException::kInvalidAccessErr);
    }
    // 10. Let result be the result of performing the sign operation specified
    // by normalizedAlgorithm using key and algorithm and with data as message.
    auto out = hash->CalculateHMAC(to_vector(data), key->get_key());
    // 11. Resolve promise with result.
    promise->Resolve(CreateBufferSource(out));
  } else {
    // 7. If the following steps or referenced procedures say to throw an error,
    // reject promise with the returned error and then terminate the algorithm.
    return reject(promise, DOMException::kNotSupportedErr);
  }
  // 6. Return promise and asynchronously perform the remaining steps.
  return promise;
}

PromiseBool SubtleCrypto::Verify(AlgorithmIdentifier algorithm,
                                 const CryptoKeyPtr &key,
                                 const BufferSource &signature,
                                 const BufferSource &data) {
  // 1. Let algorithm and key be the algorithm and key parameters passed to the
  // verify method, respectively.
  // 2. Let signature be the result of getting a copy of the bytes held by the
  // signature parameter passed to the verify method.
  // 3. Let data be the result of getting a copy of the bytes held by the data
  // parameter passed to the verify method.
  // 4. Let normalizedAlgorithm be the result of normalizing an algorithm, with
  // alg set to algorithm and op set to "verify".
  std::string normalizedAlgorithm = get_name_or_string(algorithm);
  // 6. Let promise be a new Promise.
  auto promise = CreatePromise<PromiseBool, bool>();
  if (normalizedAlgorithm == "HMAC") {
    auto hash = Hash::CreateByName(key->get_hash());
    // 10. If the [[usages]] internal slot of key does not contain an entry that
    // is "verify", then throw an InvalidAccessError.
    if (!key->usage(KeyUsage::kKeyUsageVerify) || !hash) {
      return reject(promise, DOMException::kInvalidAccessErr);
    }
    // 11. Let result be the result of performing the verify operation
    // specified by normalizedAlgorithm using key, algorithm and signature
    // and with data as message.
    auto out = hash->CalculateHMAC(to_vector(data), key->get_key());
    bool signature_matches = to_vector(signature) == out;
    // 12. Resolve promise with result.
    promise->Resolve(signature_matches);
  } else {
    // 8. If the following steps or referenced procedures say to throw an error,
    // reject promise with the returned error and then terminate the algorithm.
    return reject(promise, DOMException::kNotSupportedErr);
  }
  // 7. Return promise and asynchronously perform the remaining steps.
  return promise;
}

PromiseArray SubtleCrypto::Digest(AlgorithmIdentifier algorithm,
                                  const BufferSource &data) {
  // 1. Let algorithm be the algorithm parameter passed to the digest method.
  // 2. Let data be the result of getting a copy of the bytes held by the data
  // parameter passed to the digest method.
  // 3. Let normalizedAlgorithm be the result of normalizing an algorithm, with
  // alg set to algorithm and op set to "digest".
  auto normalizedAlgorithm = get_name_or_string(algorithm);
  // 5. Let promise be a new Promise.
  auto promise = CreatePromise();
  auto hash = Hash::CreateByName(normalizedAlgorithm);
  if (!hash) {
    // 4. If an error occurred, return a Promise rejected with
    // normalizedAlgorithm.
    return reject(promise, DOMException::kNotSupportedErr);
  }
  // 8. Let result be the result of performing the digest operation specified by
  // normalizedAlgorithm using algorithm, with data as message.
  hash->Update(to_vector(data));
  auto out = hash->Finish();
  // 9. Resolve promise with result.
  promise->Resolve(CreateBufferSource(out));
  // 6. Return promise and asynchronously perform the remaining steps.
  return promise;
}

PromiseArray SubtleCrypto::GenerateKey(AlgorithmIdentifier algorithm,
                                       bool extractable,
                                       const KeyUsages &keyUsages) {
  NOTIMPLEMENTED();
  return reject(CreatePromise(), DOMException::kNotSupportedErr);
}

PromiseArray SubtleCrypto::DeriveKey(AlgorithmIdentifier algorithm,
                                     const CryptoKeyPtr &key,
                                     AlgorithmIdentifier derivedKeyType,
                                     bool extractable,
                                     const KeyUsages &keyUsages) {
  NOTIMPLEMENTED();
  return reject(CreatePromise(), DOMException::kNotSupportedErr);
}

PromiseArray SubtleCrypto::DeriveBits(AlgorithmIdentifier algorithm,
                                      const CryptoKeyPtr &key,
                                      const uint32_t length) {
  NOTIMPLEMENTED();
  return reject(CreatePromise(), DOMException::kNotSupportedErr);
}

PromiseWrappable SubtleCrypto::ImportKey(
    KeyFormat format, const BufferSource &keyData,
    script::UnionType2<ImportKeyAlgorithmParams, std::string> algorithm,
    bool extractable, const KeyUsages &keyUsages) {
  // 1. Let format, algorithm, extractable and usages, be the format, algorithm,
  // extractable and keyUsages parameters passed to the importKey method,
  // respectively.
  // 2. Let keyData be the result of getting a copy of the bytes held by the
  // keyData parameter passed to the importKey method.
  // 3. Let normalizedAlgorithm be the result of normalizing an algorithm, with
  // alg set to algorithm and op set to "importKey".
  std::string normalizedAlgorithm =
      get_name_or_string<ImportKeyAlgorithmParams>(algorithm);
  // 5. Let promise be a new Promise.
  auto promise = CreateKeyPromise();

  if (format != kKeyFormatRaw) {
    NOTIMPLEMENTED();
    return reject(promise, new DOMException(DOMException::kNotSupportedErr));
  }

  if (!(normalizedAlgorithm == "AES-CTR" || normalizedAlgorithm == "HMAC")) {
    return reject(promise, new DOMException(DOMException::kNotSupportedErr));
  }

  // 8. Let result be the CryptoKey object that results from performing the
  // import key operation specified by normalizedAlgorithm using keyData,
  // algorithm, format, extractable and usages.
  scoped_refptr<CryptoKey> cryptoKey = new CryptoKey(normalizedAlgorithm);

  // 11. Set the [[usages]] internal slot of result to the normalized value of
  // usages.
  if (!cryptoKey->set_usages(keyUsages)) {
    return reject(promise, new DOMException("Invalid key", "not allowed"));
  }
  // 25.7.1 Let data be the octet string contained in keyData.
  cryptoKey->set_key(to_vector(keyData));
  if (algorithm.IsType<ImportKeyAlgorithmParams>()) {
    ImportKeyAlgorithmParams algo_params =
        algorithm.AsType<ImportKeyAlgorithmParams>();
    // 29.6.2 Set hash to equal the hash member of normalizedAlgorithm.
    if (algo_params.has_hash()) {
      cryptoKey->set_hash(algo_params.hash());
    }
  }
  // 12. Resolve promise with result.
  promise->Resolve(cryptoKey);
  // 6. Return promise and asynchronously perform the remaining steps.
  return promise;
}

PromiseArray SubtleCrypto::ExportKey(KeyFormat format,
                                     const CryptoKeyPtr &key) {
  NOTIMPLEMENTED();
  return reject(CreatePromise(), DOMException::kNotSupportedErr);
}

PromiseArray SubtleCrypto::WrapKey(KeyFormat format, const CryptoKeyPtr &key,
                                   const CryptoKeyPtr &wrappingKey,
                                   AlgorithmIdentifier algorithm) {
  NOTIMPLEMENTED();
  return reject(CreatePromise(), DOMException::kNotSupportedErr);
}

PromiseWrappable SubtleCrypto::UnwrapKey(
    KeyFormat format, const BufferSource &wrappedKey,
    const CryptoKeyPtr &unwrappingKey, AlgorithmIdentifier unwrapAlgorithm,
    AlgorithmIdentifier unwrappedKeyAlgorithm, bool extractacble,
    const KeyUsages &keyUsages) {
  NOTIMPLEMENTED();
  return reject(CreateKeyPromise(), DOMException::kNotSupportedErr);
}

}  // namespace subtlecrypto
}  // namespace cobalt
