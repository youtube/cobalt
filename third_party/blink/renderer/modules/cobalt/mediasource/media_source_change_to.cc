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

#include "media/filters/chunk_demuxer.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer.h"

namespace blink {

// static
SourceBuffer* MediaSourceChangeTo::addStandbySourceBuffer(
    MediaSource& media_source,
    const String& type,
    ExceptionState& exception_state) {
  return media_source.addStandbySourceBuffer(type, exception_state);
}

// static
void MediaSourceChangeTo::changeTo(
    MediaSource& media_source,
    const HeapVector<Member<SourceBuffer>>& source_buffers,
    ExceptionState& exception_state) {
  media_source.changeTo(source_buffers, exception_state);
}

// static
bool MediaSourceChangeTo::canChangeTo(const String& current_type,
                                      const String& next_type) {
  return ::media::ChunkDemuxer::CanChangeTo(current_type.Utf8(),
                                            next_type.Utf8());
}

}  // namespace blink
