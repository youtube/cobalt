/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/document_timeline.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom/document.h"

namespace cobalt {
namespace dom {

DocumentTimeline::DocumentTimeline(Document* document, double origin_time)
    : origin_time_(origin_time) {
  document_ = base::AsWeakPtr<Document>(document);
}

DocumentTimeline::DocumentTimeline(script::EnvironmentSettings* settings,
                                   double origin_time)
    : origin_time_(origin_time) {
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);
  document_ = base::AsWeakPtr<Document>(dom_settings->window()->document());
}

DocumentTimeline::~DocumentTimeline() {}

// Returns the current time for the document.  This is based off of the last
// sampled time.
// http://www.w3.org/TR/web-animations-1/#the-animationtimeline-interface
base::optional<double> DocumentTimeline::current_time() const {
  base::optional<double> document_sample_time;
  if (document_.get()) {
    document_sample_time = document_->timeline_sample_time()->InMillisecondsF();
  }

  if (document_sample_time) {
    return *document_sample_time - origin_time_;
  } else {
    return base::nullopt;
  }
}

}  // namespace dom
}  // namespace cobalt
