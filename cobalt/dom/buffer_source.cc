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

#include "cobalt/dom/buffer_source.h"

#include "base/logging.h"

namespace cobalt {
namespace dom {

void GetBufferAndSize(const BufferSource& buffer_source, const uint8** buffer,
                      int* buffer_size) {
  DCHECK(buffer);
  DCHECK(buffer_size);

  if (buffer_source.IsType<script::Handle<script::ArrayBufferView> >()) {
    script::Handle<script::ArrayBufferView> array_buffer_view =
        buffer_source.AsType<script::Handle<script::ArrayBufferView> >();
    *buffer = static_cast<const uint8*>(array_buffer_view->RawData());
    DCHECK_LT(array_buffer_view->ByteLength(),
              static_cast<const size_t>(INT_MAX));
    *buffer_size = static_cast<int>(array_buffer_view->ByteLength());
  } else if (buffer_source.IsType<script::Handle<script::ArrayBuffer> >()) {
    script::Handle<script::ArrayBuffer> array_buffer =
        buffer_source.AsType<script::Handle<script::ArrayBuffer> >();
    *buffer = static_cast<const uint8*>(array_buffer->Data());
    DCHECK_LT(array_buffer->ByteLength(), static_cast<const size_t>(INT_MAX));
    *buffer_size = static_cast<int>(array_buffer->ByteLength());
  } else {
    NOTREACHED();
    *buffer = NULL;
    *buffer_size = 0;
  }
}

}  // namespace dom
}  // namespace cobalt
