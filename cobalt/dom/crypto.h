// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_CRYPTO_H_
#define COBALT_DOM_CRYPTO_H_

#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/subtlecrypto/subtle_crypto.h"

namespace cobalt {
namespace dom {

// The Crypto interface represents an interface to general purpose
// cryptographic functionality including a cryptographically strong
// pseudo-random number generator seeded with truly random values.
//   https://www.w3.org/TR/WebCryptoAPI/#crypto-interface
//
// Note that in Cobalt we only need to implement getRandomValues().
class Crypto : public script::Wrappable {
 public:
  // Web API:Crypto
  //
  scoped_refptr<subtlecrypto::SubtleCrypto> subtle(
      script::EnvironmentSettings* settings) const;

  static script::Handle<script::ArrayBufferView> GetRandomValues(
      const script::Handle<script::ArrayBufferView>& array,
      script::ExceptionState* exception_state);
  DEFINE_WRAPPABLE_TYPE(Crypto);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CRYPTO_H_
