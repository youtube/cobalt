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

#ifndef GLIMP_GLES_REF_COUNTED_RESOURCE_MAP_H_
#define GLIMP_GLES_REF_COUNTED_RESOURCE_MAP_H_

#include <map>

#include "glimp/gles/unique_id_generator.h"
#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

// This class manages a mapping between an integer Id value and a reference
// counted object.  It allocates Ids to resources upon registration, maintains
// a reference to the objects while registered, and facilitates retrieving
// objects by Id.
template <typename T>
class RefCountedResourceMap {
 public:
  // Registers a resource into the map, and returns the id value that it is
  // assigned.
  uint32_t RegisterResource(const nb::scoped_refptr<T>& resource) {
    uint32_t id = id_generator_.AcquireId();
    resources_.insert(std::make_pair(id, resource));
    return id;
  }

  // Returns a reference to the resource specified by id.  If the resource
  // does not exist in the map, scoped_refptr<T>() is returned.
  nb::scoped_refptr<T> GetResource(uint32_t id) {
    typename InternalResourceMap::iterator found = resources_.find(id);
    return found == resources_.end() ? nb::scoped_refptr<T>() : found->second;
  }

  // Removes the resource with the specified id from the map and returns it.
  // If the resource is not in the map, scoped_refptr<T>() is returned.
  nb::scoped_refptr<T> DeregisterResource(uint32_t id) {
    typename InternalResourceMap::iterator found = resources_.find(id);
    if (found == resources_.end()) {
      return nb::scoped_refptr<T>();
    }

    id_generator_.ReleaseId(found->first);
    nb::scoped_refptr<T> resource(found->second);

    resources_.erase(found);

    return resource;
  }

  // Returns whether the map contains any entries or not.
  bool empty() const { return resources_.empty(); }

 private:
  UniqueIdGenerator id_generator_;
  typedef std::map<uint32_t, nb::scoped_refptr<T> > InternalResourceMap;
  InternalResourceMap resources_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_REF_COUNTED_RESOURCE_MAP_H_
