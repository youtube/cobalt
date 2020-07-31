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

#ifndef COBALT_LOADER_MESH_MESH_CACHE_H_
#define COBALT_LOADER_MESH_MESH_CACHE_H_

#include <memory>
#include <string>

#include "cobalt/loader/loader_factory.h"
#include "cobalt/loader/mesh/mesh_projection.h"
#include "cobalt/loader/resource_cache.h"

namespace cobalt {
namespace loader {
namespace mesh {

// |MeshResourceCacheType| provides the types and implements the functions
// required by |ResourceCache<MeshResourceCacheType>|
struct MeshResourceCacheType {
  typedef MeshProjection ResourceType;

  static uint32 GetEstimatedSizeInBytes(
      const scoped_refptr<ResourceType>& resource) {
    return resource->GetEstimatedSizeInBytes();
  }
};

typedef CachedResource<MeshResourceCacheType> CachedMesh;
typedef CachedResourceReferenceWithCallbacks CachedMeshReferenceWithCallbacks;
typedef CachedMeshReferenceWithCallbacks::CachedResourceReferenceVector
    CachedMeshReferenceVector;

typedef ResourceCache<MeshResourceCacheType> MeshCache;

// CreateMeshCache() provides a mechanism for creating an |MeshCache|.
inline static std::unique_ptr<MeshCache> CreateMeshCache(
    const std::string& name, const base::DebuggerHooks& debugger_hooks,
    uint32 cache_capacity, loader::LoaderFactory* loader_factory) {
  return std::unique_ptr<MeshCache>(
      new MeshCache(name, debugger_hooks, cache_capacity,
                    false /*are_loading_retries_enabled*/,
                    base::Bind(&loader::LoaderFactory::CreateMeshLoader,
                               base::Unretained(loader_factory))));
}

}  // namespace mesh
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_MESH_MESH_CACHE_H_
