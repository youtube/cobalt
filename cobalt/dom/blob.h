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

#ifndef COBALT_DOM_BLOB_H_
#define COBALT_DOM_BLOB_H_

#include <algorithm>
#include <string>
#include <vector>

#include "cobalt/dom/url_registry.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/data_view.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/wrappable.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

// This ensures this header can be included without depending on generated
// dictionaries.
class BlobPropertyBag;

// A Blob object refers to a byte sequence, and has a size attribute which is
// the total number of bytes in the byte sequence, and a type attribute, which
// is an ASCII-encoded string in lower case representing the media type of the
// byte sequence.
//    https://www.w3.org/TR/FileAPI/#dfn-Blob
class Blob : public script::Wrappable {
 public:
  typedef UrlRegistry<Blob> Registry;
  typedef script::UnionType5<script::Handle<script::ArrayBuffer>,
                             script::Handle<script::ArrayBufferView>,
                             script::Handle<script::DataView>,
                             scoped_refptr<Blob>, std::string>
      BlobPart;

  // settings is non-nullable.
  Blob(script::EnvironmentSettings* settings,
       const script::Handle<script::ArrayBuffer>& buffer =
           script::Handle<script::ArrayBuffer>(),
       const BlobPropertyBag& options = EmptyBlobPropertyBag());

  // settings is non-nullable.
  Blob(script::EnvironmentSettings* settings,
       const script::Sequence<BlobPart>& blob_parts,
       const BlobPropertyBag& options = EmptyBlobPropertyBag());

  const uint8* data() {
    return static_cast<uint8*>(buffer_reference_.value().Data());
  }

  uint64 size() const {
    return static_cast<uint64>(buffer_reference_.value().ByteLength());
  }

  const std::string& type() const { return type_; }

  DEFINE_WRAPPABLE_TYPE(Blob);
  JSObjectType GetJSObjectType() override { return JSObjectType::kBlob; }

 private:
  static const BlobPropertyBag& EmptyBlobPropertyBag();

  script::ScriptValue<script::ArrayBuffer>::Reference buffer_reference_;
  std::string type_;

  DISALLOW_COPY_AND_ASSIGN(Blob);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_BLOB_H_
