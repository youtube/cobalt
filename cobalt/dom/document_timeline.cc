// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "base/memory/weak_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/document_timeline.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace dom {

namespace {

scoped_refptr<base::BasicClock> CreateOffsetClock(Document* document,
                                                  double origin_time) {
  return document->navigation_start_clock() ?
      new base::OffsetClock(
        document->navigation_start_clock(),
        base::TimeDelta::FromMillisecondsD(origin_time)) :
      NULL;
}
}  // namespace

DocumentTimeline::DocumentTimeline(Document* document, double origin_time)
    : AnimationTimeline(CreateOffsetClock(document, origin_time)) {
  document_ = base::AsWeakPtr<Document>(document);
}

namespace {
dom::Document* DocumentFromEnvironmentSettings(
    script::EnvironmentSettings* settings) {
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);
  return dom_settings->window()->document().get();
}
}  // namespace

DocumentTimeline::DocumentTimeline(script::EnvironmentSettings* settings,
                                   double origin_time)
    : AnimationTimeline(CreateOffsetClock(
          DocumentFromEnvironmentSettings(settings), origin_time)) {
  document_ =
      base::AsWeakPtr<Document>(DocumentFromEnvironmentSettings(settings));
}

DocumentTimeline::~DocumentTimeline() {}

}  // namespace dom
}  // namespace cobalt
