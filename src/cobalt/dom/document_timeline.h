// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_DOCUMENT_TIMELINE_H_
#define COBALT_DOM_DOCUMENT_TIMELINE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web_animations/animation_timeline.h"

namespace cobalt {

namespace script {
class EnvironmentSettings;
}

namespace dom {

class Document;

// Implements the DocumentTimeline IDL interface.
// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-documenttimeline-interface
class DocumentTimeline : public web_animations::AnimationTimeline {
 public:
  DocumentTimeline(Document* document, double origin_time);
  DocumentTimeline(script::EnvironmentSettings*, double origin_time);

  // Custom, not in any spec.
  DEFINE_WRAPPABLE_TYPE(DocumentTimeline);

 private:
  ~DocumentTimeline() override;

  base::WeakPtr<Document> document_;

  DISALLOW_COPY_AND_ASSIGN(DocumentTimeline);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOCUMENT_TIMELINE_H_
