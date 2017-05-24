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

#ifndef COBALT_BROWSER_LIFECYCLE_OBSERVER_H_
#define COBALT_BROWSER_LIFECYCLE_OBSERVER_H_

#include "cobalt/render_tree/resource_provider.h"
#include "starboard/configuration.h"

namespace cobalt {
namespace browser {

// A pure virtual interface for observers of the application lifecycle.
class LifecycleObserver {
 public:
  // Start running visibly with the given graphics ResourceProvider, loading if
  // necessary. This represents a transition from Preloading to Started, so it
  // only makes sense if the object is in the Preloading state.
  virtual void Start(render_tree::ResourceProvider* resource_provider) = 0;

  // Pauses from Started, staying visible and retaining graphics resources.
  virtual void Pause() = 0;

  // Unpauses, going back to to the Started state, and continuing to
  // use the same ResourceProvider and graphics resources.
  virtual void Unpause() = 0;

  // Suspends from Paused, and releases its reference to the ResourceProvider,
  // additionally releasing all references to any resources created from
  // it. This method must only be called if the object has previously been
  // Paused.
  virtual void Suspend() = 0;

  // Resumes to Paused from Suspended, with a new ResourceProvider. This method
  // must only be called if the object has previously been Suspended.
  virtual void Resume(render_tree::ResourceProvider* resource_provider) = 0;

 protected:
  virtual ~LifecycleObserver() {}
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_LIFECYCLE_OBSERVER_H_
