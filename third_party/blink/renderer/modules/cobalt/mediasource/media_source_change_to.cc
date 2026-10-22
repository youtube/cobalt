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

#include "third_party/blink/renderer/modules/cobalt/mediasource/media_source_change_to.h"

#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer.h"

namespace blink {

// static
SourceBuffer* MediaSourceChangeTo::addSourceBuffer(
    MediaSource& media_source,
    const String& type,
    const SourceBufferInit* init,
    ExceptionState& exception_state) {
  return media_source.AddSourceBufferWithInit(type, init, exception_state);
}

// static
ScriptPromise<IDLUndefined> MediaSourceChangeTo::changeTo(
    ScriptState* script_state,
    MediaSource& media_source,
    const HeapVector<Member<SourceBuffer>>& source_buffers,
    ExceptionState& exception_state) {
  return media_source.changeTo(script_state, source_buffers, exception_state);
}

}  // namespace blink
