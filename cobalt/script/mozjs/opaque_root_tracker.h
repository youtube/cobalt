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
#ifndef COBALT_SCRIPT_MOZJS_OPAQUE_ROOT_TRACKER_H_
#define COBALT_SCRIPT_MOZJS_OPAQUE_ROOT_TRACKER_H_

#include "base/hash_tables.h"
#include "cobalt/script/mozjs/referenced_object_map.h"
#include "cobalt/script/mozjs/wrapper_factory.h"
#include "cobalt/script/mozjs/wrapper_private.h"

namespace cobalt {
namespace script {
namespace mozjs {

// This class manages lifetime of structures containing objects that are
// reachable from through the interface implementation, but not through
// JavaScript, by ensuring that the root of the structure is not garbage
// collected if any of the members of the structure are reachable.
//
// The implementation of the structure in Cobalt is typically reference counted.
// The |root| of that structure is a single node that transitively holds a
// reference to every other node in the structure. It may be the case that the
// only reference to that root node is from a JS object's WrapperPrivate. The
// OpaqueRootTracker class ensures that if some node in the structure is
// reachable from JavaScript, the JS object holding the reference to the root
// of the structure is marked as reachable as well, which preserves the final
// reference to the root of the structure, keeping the entire structure alive.
//
// For example, any DOM Node can be reached from any other DOM Node object in
// the same tree. If an arbitary internal Node is reachable from JavaScript,
// this class will ensure that the root of the tree will also be kept alive,
// preserving the entire tree until no nodes in the tree are reachable from
// JavaScript.
//
// An object's opaque root can change throughout the object's lifetime, so the
// root needs to be recalculated every garbage collection phase.
class OpaqueRootTracker {
 public:
  // Callers do not need to operate on this class. They just need to manage its
  // lifetime appropriately as described below.
  class OpaqueRootState {
   protected:
    OpaqueRootState() {}
    virtual ~OpaqueRootState() {}
    friend class scoped_ptr<OpaqueRootState>;
  };

  OpaqueRootTracker(JSContext* context,
                    ReferencedObjectMap* referenced_object_map,
                    WrapperFactory* wrapper_factory);

  // All objects that implement this functionality must be registered to this
  // class.
  void AddObjectWithOpaqueRoot(WrapperPrivate* wrapper_private);
  void RemoveObjectWithOpaqueRoot(WrapperPrivate* wrapper_private);

  // Get the current state of opaque roots. This should be called when garbage
  // collection begins before marking has begun. Once garbage collection is
  // complete, this should be released.
  scoped_ptr<OpaqueRootState> GetCurrentOpaqueRootState();

 private:
  void TrackReachabilityToOpaqueRoot(OpaqueRootState* state,
                                     WrapperPrivate* wrapper_private);
  void TrackReachableWrappables(OpaqueRootState* state,
                                WrapperPrivate* wrapper_private);
  typedef base::hash_set<WrapperPrivate*> WrapperPrivateSet;

  JSContext* context_;
  ReferencedObjectMap* referenced_object_map_;
  WrapperFactory* wrapper_factory_;
  // list of objects that are potentially reachable from an opaque root
  WrapperPrivateSet all_objects_;
};
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
#endif  // COBALT_SCRIPT_MOZJS_OPAQUE_ROOT_TRACKER_H_
