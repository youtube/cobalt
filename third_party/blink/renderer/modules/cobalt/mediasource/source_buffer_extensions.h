// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_MEDIASOURCE_SOURCE_BUFFER_EXTENSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_MEDIASOURCE_SOURCE_BUFFER_EXTENSIONS_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#if !BUILDFLAG(USE_STARBOARD_MEDIA)
#error "This file only works with Starboard media"
#endif  // !BUILDFLAG(USE_STARBOARD_MEDIA)

namespace blink {

class DOMArrayBuffer;
class DOMArrayBufferView;
class ExceptionState;
class SourceBuffer;

class SourceBufferExtensions {
  STATIC_ONLY(SourceBufferExtensions);

 public:
  static double writeHead(SourceBuffer&, ExceptionState&);
  static void appendTransferredBuffer(SourceBuffer&,
                                      DOMArrayBuffer* data,
                                      ExceptionState&);
  static void appendTransferredBuffer(SourceBuffer&,
                                      NotShared<DOMArrayBufferView> data,
                                      ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_MEDIASOURCE_SOURCE_BUFFER_EXTENSIONS_H_
