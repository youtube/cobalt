// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_FETCH_FETCH_INTERNAL_H_
#define COBALT_FETCH_FETCH_INTERNAL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/blob.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/typed_arrays.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace fetch {

// Wrapper for utility functions for use with the fetch polyfill. This is
// specific to the fetch polyfill and may change as the implementation changes.
// This is not meant to be public and should not be used outside of the fetch
// implementation.
class FetchInternal : public script::Wrappable {
 public:
  // Return whether the given URL is valid.
  static bool IsUrlValid(script::EnvironmentSettings* settings,
                         const std::string& url, bool allow_credentials);

  // Return a Uint8Array representing the given text as UTF-8 encoded data.
  static script::Handle<script::Uint8Array> EncodeToUTF8(
      script::EnvironmentSettings* settings, const std::string& text,
      script::ExceptionState* exception_state);

  // Return a UTF-8 encoded string representing the given data.
  static std::string DecodeFromUTF8(
      const script::Handle<script::Uint8Array>& data,
      script::ExceptionState* exception_state);

  // Translate a dom Blob to ArrayBuffer.
  static script::Handle<script::ArrayBuffer> BlobToArrayBuffer(
      script::EnvironmentSettings* settings,
      const scoped_refptr<dom::Blob>& blob);

  DEFINE_WRAPPABLE_TYPE(FetchInternal);
};

}  // namespace fetch
}  // namespace cobalt

#endif  // COBALT_FETCH_FETCH_INTERNAL_H_
