// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "base/observer_list_types.h"
#include "cobalt/render_tree/resource_provider.h"
#include "starboard/configuration.h"

namespace cobalt {
namespace browser {

// A pure virtual interface for observers of the application lifecycle.
class LifecycleObserver : public base::CheckedObserver {
 public:
  // Blurs from Started, staying visible and retaining graphics resources.
  virtual void Blur(SbTimeMonotonic timestamp) = 0;

  // Conceals from Blurred, transitioning to invisible but background tasks can
  // still be running.
  virtual void Conceal(render_tree::ResourceProvider* resource_provider,
                       SbTimeMonotonic timestamp) = 0;

  // Freezes from Concealed, and releases its reference to the ResourceProvider,
  // additionally releasing all references to any resources created from
  // it. This method must only be called if the object has previously been
  // Concealed.
  virtual void Freeze(SbTimeMonotonic timestamp) = 0;

  // Unfreezes from Frozen, with a new ResourceProvider. This method must only
  // be called if the object has previously been Frozen.
  virtual void Unfreeze(render_tree::ResourceProvider* resource_provider,
                        SbTimeMonotonic timestamp) = 0;

  // Reveals from Concealed, going back into partially-obscured state. This
  // method must only be called if the object has previously been Concealed.
  virtual void Reveal(render_tree::ResourceProvider* resource_provider,
                      SbTimeMonotonic timestamp) = 0;

  // Focuses, going back to the Started state, and continuing to use the same
  // ResourceProvider and graphics resources.
  virtual void Focus(SbTimeMonotonic timestamp) = 0;

 protected:
  virtual ~LifecycleObserver() {}
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_LIFECYCLE_OBSERVER_H_
