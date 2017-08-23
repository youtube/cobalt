// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/buffer_source.h"

#include "base/logging.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/array_buffer_view.h"

namespace cobalt {
namespace dom {

void GetBufferAndSize(const BufferSource& buffer_source, const uint8** buffer,
                      int* buffer_size) {
  DCHECK(buffer);
  DCHECK(buffer_size);

  if (buffer_source.IsType<scoped_refptr<ArrayBufferView> >()) {
    scoped_refptr<ArrayBufferView> array_buffer_view =
        buffer_source.AsType<scoped_refptr<ArrayBufferView> >();
    *buffer = static_cast<const uint8*>(array_buffer_view->base_address());
    *buffer_size = array_buffer_view->byte_length();
  } else if (buffer_source.IsType<scoped_refptr<ArrayBuffer> >()) {
    scoped_refptr<ArrayBuffer> array_buffer =
        buffer_source.AsType<scoped_refptr<ArrayBuffer> >();
    *buffer = array_buffer->data();
    *buffer_size = array_buffer->byte_length();
  } else {
    NOTREACHED();
    *buffer = NULL;
    *buffer_size = 0;
  }
}

}  // namespace dom
}  // namespace cobalt
