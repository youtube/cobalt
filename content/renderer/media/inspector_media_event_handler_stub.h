// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_INSPECTOR_MEDIA_EVENT_HANDLER_STUB_H_
#define CONTENT_RENDERER_MEDIA_INSPECTOR_MEDIA_EVENT_HANDLER_STUB_H_

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


#endif  // CONTENT_RENDERER_MEDIA_INSPECTOR_MEDIA_EVENT_HANDLER_STUB_H_
