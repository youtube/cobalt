/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/script/mozjs/opaque_root_tracker.h"

#include <utility>
#include <vector>

#include "cobalt/script/mozjs/weak_heap_object.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace {
// Implementation of OpaqueRootTracker::OpaqueRootState.
// On creation, this class will register reachability between objects and their
// roots in |ReferencedObjectMap|. On destruction, the reachability
// relationship will be removed.
class OpaqueRootStateImpl : public OpaqueRootTracker::OpaqueRootState {
 public:
  OpaqueRootStateImpl(JSContext* context,
                      ReferencedObjectMap* referenced_object_map)
      : context_(context), referenced_object_map_(referenced_object_map) {}

  void TrackReachability(WrapperPrivate* from, WrapperPrivate* to) {
    intptr_t from_key = ReferencedObjectMap::GetKeyForWrappable(
        from->wrappable<Wrappable>().get());
    JS::RootedObject to_object(context_, to->js_object_proxy());
    referenced_objects_.push_back(
        std::make_pair(from_key, WeakHeapObject(context_, to_object)));
    referenced_object_map_->AddReferencedObject(from_key, to_object);
  }

  ~OpaqueRootStateImpl() {
    JSAutoRequest auto_request(context_);
    for (ReferencedObjectPairVector::iterator it = referenced_objects_.begin();
         it != referenced_objects_.end(); ++it) {
      if (it->second.Get()) {
        JS::RootedObject reachable_object(context_, it->second.Get());
        referenced_object_map_->RemoveReferencedObject(it->first,
                                                       reachable_object);
      }
    }
  }

 private:
  typedef std::vector<std::pair<intptr_t, WeakHeapObject> >
      ReferencedObjectPairVector;

  JSContext* context_;
  ReferencedObjectMap* referenced_object_map_;
  ReferencedObjectPairVector referenced_objects_;
};
}  // namespace

OpaqueRootTracker::OpaqueRootTracker(JSContext* context,
                                     ReferencedObjectMap* referenced_object_map,
                                     WrapperFactory* wrapper_factory)
    : context_(context),
      referenced_object_map_(referenced_object_map),
      wrapper_factory_(wrapper_factory) {}

void OpaqueRootTracker::AddObjectWithOpaqueRoot(
    WrapperPrivate* wrapper_private) {
  all_objects_.insert(wrapper_private);
}

void OpaqueRootTracker::RemoveObjectWithOpaqueRoot(
    WrapperPrivate* wrapper_private) {
  all_objects_.erase(wrapper_private);
}

scoped_ptr<OpaqueRootTracker::OpaqueRootState>
OpaqueRootTracker::GetCurrentOpaqueRootState() {
  scoped_ptr<OpaqueRootStateImpl> state(
      new OpaqueRootStateImpl(context_, referenced_object_map_));
  // Get the current opaque root for all objects that are being tracked.
  for (WrapperPrivateSet::iterator it = all_objects_.begin();
       it != all_objects_.end(); ++it) {
    WrapperPrivate* wrapper_private = *it;
    TrackReachabilityToOpaqueRoot(state.get(), wrapper_private);
    TrackReachableWrappables(state.get(), wrapper_private);
  }
  return state.PassAs<OpaqueRootState>();
}

void OpaqueRootTracker::TrackReachabilityToOpaqueRoot(
    OpaqueRootState* state, WrapperPrivate* wrapper_private) {
  OpaqueRootStateImpl* state_impl =
      base::polymorphic_downcast<OpaqueRootStateImpl*>(state);
  // If this wrappable has an opaque root, track reachability between this
  // wrappable and its root.
  Wrappable* opaque_root = wrapper_private->GetOpaqueRoot();
  if (opaque_root) {
    WrapperPrivate* opaque_root_private = WrapperPrivate::GetFromWrappable(
        opaque_root, context_, wrapper_factory_);
    // Always mark the root as reachable from the non-root object.
    state_impl->TrackReachability(wrapper_private, opaque_root_private);

    // Only mark the non-root object as reachable if we need to keep the
    // wrapper alive for some reason. In general it's okay for a wrapper to
    // get GC'd because the Cobalt object will still be kept alive, and a new
    // JS object can be created if needed again.
    if (wrapper_private->ShouldKeepWrapperAliveIfReachable()) {
      state_impl->TrackReachability(opaque_root_private, wrapper_private);
    }
  }
}

void OpaqueRootTracker::TrackReachableWrappables(
    OpaqueRootState* state, WrapperPrivate* wrapper_private) {
  OpaqueRootStateImpl* state_impl =
      base::polymorphic_downcast<OpaqueRootStateImpl*>(state);
  // Track any wrappables that are explicitly marked as reachable from
  // this wrappable.
  typedef std::vector<Wrappable*> WrappableVector;
  WrappableVector reachable_objects;
  wrapper_private->GetReachableWrappables(&reachable_objects);
  for (size_t i = 0; i < reachable_objects.size(); ++i) {
    WrapperPrivate* reachable_object_private = WrapperPrivate::GetFromWrappable(
        reachable_objects[i], context_, wrapper_factory_);
    state_impl->TrackReachability(wrapper_private, reachable_object_private);
  }
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
