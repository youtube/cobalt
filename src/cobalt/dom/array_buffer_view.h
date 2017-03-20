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

#ifndef COBALT_DOM_ARRAY_BUFFER_VIEW_H_
#define COBALT_DOM_ARRAY_BUFFER_VIEW_H_

#include <string>

#include "cobalt/dom/array_buffer.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class ArrayBufferView : public script::Wrappable {
 public:
  // Web API: ArrayBufferView
  const scoped_refptr<ArrayBuffer>& buffer() { return buffer_; }
  uint32 byte_offset() const { return byte_offset_; }
  uint32 byte_length() const { return byte_length_; }

  // Custom, not in any spec.
  void* base_address();
  const void* base_address() const;

  DEFINE_WRAPPABLE_TYPE(ArrayBufferView);

 protected:
  explicit ArrayBufferView(const scoped_refptr<ArrayBuffer>& buffer);
  ArrayBufferView(const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset,
                  uint32 byte_length);
  ~ArrayBufferView();

 private:
  scoped_refptr<ArrayBuffer> buffer_;
  uint32 byte_offset_;
  uint32 byte_length_;

  DISALLOW_COPY_AND_ASSIGN(ArrayBufferView);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ARRAY_BUFFER_VIEW_H_
