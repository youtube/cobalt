// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <vector>

#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/url_registry.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// A Blob object refers to a byte sequence, and has a size attribute which is
// the total number of bytes in the byte sequence, and a type attribute, which
// is an ASCII-encoded string in lower case representing the media type of the
// byte sequence.
//    https://www.w3.org/TR/2015/WD-FileAPI-20150421/#dfn-Blob
//
// Note: Cobalt currently does not implement nor need the type attribute.
class Blob : public script::Wrappable {
 public:
  typedef UrlRegistry<Blob> Registry;

  Blob(script::EnvironmentSettings* settings,
       const scoped_refptr<ArrayBuffer>& buffer = NULL)
      : buffer_(
            buffer ? buffer->Slice(settings, 0)
                   : scoped_refptr<ArrayBuffer>(new ArrayBuffer(settings, 0))) {
  }

  const uint8* data() { return buffer_->data(); }

  uint64 size() { return static_cast<uint64>(buffer_->byte_length()); }

  DEFINE_WRAPPABLE_TYPE(Blob);

 private:
  scoped_refptr<ArrayBuffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(Blob);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_BLOB_H_
