// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_ENCRYPTEDMEDIA_MEDIA_KEYS_GET_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_ENCRYPTEDMEDIA_MEDIA_KEYS_GET_METRICS_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

class MediaKeys;

class MediaKeysGetMetrics {
  STATIC_ONLY(MediaKeysGetMetrics);

 public:
  static ScriptPromise getMetrics(ScriptState* script_state,
                                  MediaKeys& media_keys,
                                  ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_ENCRYPTEDMEDIA_MEDIA_KEYS_GET_METRICS_H_
