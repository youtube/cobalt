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

#ifndef COBALT_SUBTLECRYPTO_SUBTLE_CRYPTO_H_
#define COBALT_SUBTLECRYPTO_SUBTLE_CRYPTO_H_

#include <string>
#include <vector>

#include "cobalt/dom/buffer_source.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/promise.h"

#include "cobalt/subtlecrypto/aes_ctr_params.h"
#include "cobalt/subtlecrypto/algorithm.h"
#include "cobalt/subtlecrypto/crypto_key.h"
#include "cobalt/subtlecrypto/import_key_algorithm_params.h"
#include "cobalt/subtlecrypto/key_format.h"
#include "cobalt/subtlecrypto/key_usage.h"

namespace cobalt {

namespace subtlecrypto {

class SubtleCrypto : public base::SupportsWeakPtr<SubtleCrypto>,
                     public script::Wrappable {
 public:
  using KeyUsages = script::Sequence<KeyUsage>;
  using AlgorithmIdentifier = script::UnionType2<Algorithm, std::string>;
  using EncryptionAlgorithm = script::UnionType2<AesCtrParams, Algorithm>;
  using CryptoKeyPtr = scoped_refptr<CryptoKey>;
  using BufferSource = dom::BufferSource;
  using DOMException = dom::DOMException;

  using PromiseArray = script::Handle<script::Promise<dom::BufferSource>>;
  using PromiseBool = script::Handle<script::Promise<bool>>;
  using PromiseWrappable =
      script::Handle<script::Promise<scoped_refptr<script::Wrappable>>>;

  explicit SubtleCrypto(script::EnvironmentSettings* environment);
  ~SubtleCrypto() override;

  PromiseArray Decrypt(EncryptionAlgorithm algorithm, const CryptoKeyPtr& key,
                       const dom::BufferSource& data);
  PromiseArray Encrypt(EncryptionAlgorithm algorithm, const CryptoKeyPtr& key,
                       const dom::BufferSource& data);
  PromiseArray Sign(AlgorithmIdentifier algorithm, const CryptoKeyPtr& key,
                    const dom::BufferSource& data);
  PromiseBool Verify(AlgorithmIdentifier algorithm, const CryptoKeyPtr& key,
                     const dom::BufferSource& signature,
                     const dom::BufferSource& data);
  PromiseArray Digest(AlgorithmIdentifier algorithm,
                      const dom::BufferSource& data);
  PromiseArray GenerateKey(AlgorithmIdentifier algorithm, bool extractable,
                           const KeyUsages& keyUsages);
  PromiseArray DeriveKey(AlgorithmIdentifier algorithm, const CryptoKeyPtr& key,
                         AlgorithmIdentifier derivedKeyType, bool extractable,
                         const KeyUsages& keyUsages);
  PromiseArray DeriveBits(AlgorithmIdentifier algorithm,
                          const CryptoKeyPtr& key, const uint32_t length);
  PromiseWrappable ImportKey(
      KeyFormat format, const dom::BufferSource& keyData,
      script::UnionType2<ImportKeyAlgorithmParams, std::string> algorithm,
      bool extractable, const KeyUsages& keyUsages);
  PromiseArray ExportKey(KeyFormat format, const CryptoKeyPtr& key);
  PromiseArray WrapKey(KeyFormat format, const CryptoKeyPtr& key,
                       const CryptoKeyPtr& wrappingKey,
                       AlgorithmIdentifier algorithm);
  PromiseWrappable UnwrapKey(KeyFormat format,
                             const dom::BufferSource& wrappedKey,
                             const CryptoKeyPtr& unwrappingKey,
                             AlgorithmIdentifier unwrapAlgorithm,
                             AlgorithmIdentifier unwrappedKeyAlgorithm,
                             bool extractacble, const KeyUsages& keyUsages);

  DEFINE_WRAPPABLE_TYPE(SubtleCrypto);

 private:
  SubtleCrypto(const SubtleCrypto&) = delete;
  SubtleCrypto& operator=(const SubtleCrypto&) = delete;

  cobalt::script::GlobalEnvironment* global_env_ = nullptr;
  cobalt::script::ScriptValueFactory* script_value_factory_ = nullptr;

  template <typename Promise = PromiseArray, typename T = dom::BufferSource>
  Promise CreatePromise() {
    DCHECK(script_value_factory_);
    return script_value_factory_->CreateBasicPromise<T>();
  }

  PromiseWrappable CreateKeyPromise();
  dom::BufferSource CreateBufferSource(const std::vector<uint8_t>& input);
};

}  // namespace subtlecrypto

}  // namespace cobalt

#endif  // COBALT_SUBTLECRYPTO_SUBTLE_CRYPTO_H_
