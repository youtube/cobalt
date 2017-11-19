// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_TRACER_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_TRACER_H_

#include <vector>

#include "cobalt/script/wrappable.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Our mozjs specific implementation of |script::Tracer|. Tracing sessions
// will be initiated from a |Wrapper| of SpiderMonkey's choice, and then it
// will be the |mozjs::MozjsTracer|'s job to assist SpiderMonkey's garbage
// collector in traversing the graph of |Wrapper|s and |Wrappable|s.
// |Wrappable|s will inform us about what they can reach through their
// |TraceMembers| implementation, and then we will pass the reachable
// |Wrappable|'s |Wrapper| to SpiderMonkey GC if it exists, and otherwise
// continue traversing ourselves from |Wrappable| to (the unwrapped)
// |Wrappable|.
class MozjsTracer : public ::cobalt::script::Tracer {
 public:
  explicit MozjsTracer(JSTracer* js_tracer) : js_tracer_(js_tracer) {}
  void Trace(Traceable* traceable) override;

  void TraceFrom(Wrappable* wrappable);

 private:
  JSTracer* js_tracer_;
  // Pending |Traceable|s that we must traverse ourselves, since they did not
  // have a |WrapperPrivate|, or were not a wrapper.
  std::vector<Traceable*> frontier_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_TRACER_H_
