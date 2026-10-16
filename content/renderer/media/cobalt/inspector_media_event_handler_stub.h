// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef CONTENT_RENDERER_MEDIA_COBALT_INSPECTOR_MEDIA_EVENT_HANDLER_STUB_H_
#define CONTENT_RENDERER_MEDIA_COBALT_INSPECTOR_MEDIA_EVENT_HANDLER_STUB_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/renderer/media/batching_media_log.h"

namespace blink {
class MediaInspectorContext;
}

namespace content {

class CONTENT_EXPORT InspectorMediaEventHandler
    : public BatchingMediaLog::EventHandler {
 public:
  explicit InspectorMediaEventHandler(blink::MediaInspectorContext*) {}
  ~InspectorMediaEventHandler() override = default;
  void SendQueuedMediaEvents(std::vector<media::MediaLogRecord>) override {}
  void OnWebMediaPlayerDestroyed() override {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_COBALT_INSPECTOR_MEDIA_EVENT_HANDLER_STUB_H_
