// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_DOCUMENT_LOAD_TIMING_INFO_H_
#define COBALT_DOM_DOCUMENT_LOAD_TIMING_INFO_H_

#include "cobalt/dom/performance_high_resolution_time.h"

namespace cobalt {
namespace dom {

// Implements the DocumentLoadTimingInfo struct for saving document loading
// related timing info.
//   https://html.spec.whatwg.org/multipage/dom.html#document-load-timing-info
struct DocumentLoadTimingInfo {
  DocumentLoadTimingInfo() = default;
  DocumentLoadTimingInfo(const DocumentLoadTimingInfo& other) = default;
  ~DocumentLoadTimingInfo() = default;

  // If the previous document and the current document have the same origin,
  // these timestamps are measured immediately after the user agent handles the
  // unload event of the previous document. If there is no previous document
  // or the previous document has a different origin than the current document,
  // these attributes will be zero.
  base::TimeTicks unload_event_start;
  base::TimeTicks unload_event_end;

  // This timestamp is measured before the user agent dispatches the
  // DOMContentLoaded event.
  DOMHighResTimeStamp dom_content_loaded_event_start = 0.0;

  // This timestamp is measured after the user agent completes handling of
  // the DOMContentLoaded event.
  DOMHighResTimeStamp dom_content_loaded_event_end = 0.0;

  // This timestamp is measured before the user agent sets the current
  // document readiness to "complete". See document readiness for a precise
  // definition.
  DOMHighResTimeStamp dom_complete = 0.0;

  // This timestamp is measured before the user agent dispatches the load
  // event for the document.
  DOMHighResTimeStamp load_event_start = 0.0;

  // This timestamp is measured after the user agent completes handling
  // the load event for the document.
  DOMHighResTimeStamp load_event_end = 0.0;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOCUMENT_LOAD_TIMING_INFO_H_
