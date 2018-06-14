// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_URL_REGISTRY_H_
#define COBALT_DOM_URL_REGISTRY_H_

#include <string>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace dom {

// This class manages a registry of objects.  Its user can associate a
// object to a blob url as well as to retrieve a object by a blob url.
// This is because to assign a object to the src of an HTMLMediaElement
// (when the object is MediaSource) or to another kind of element or CSS
// property (when the object is a Blob), we have to first convert the
// object into a blob url by calling URL.createObjectURL(). And eventually
// the element or property has to retrieve the object from the blob url.
// Note: It is unsafe to directly encode the pointer to the object in the
// url as the url is assigned from JavaScript.
template <typename ObjectType>
class UrlRegistry : public script::Traceable {
 public:
  void Register(const std::string& blob_url,
                const scoped_refptr<ObjectType>& object);
  scoped_refptr<ObjectType> Retrieve(const std::string& blob_url);
  bool Unregister(const std::string& blob_url);

  void TraceMembers(script::Tracer* tracer) override {
    tracer->TraceValues(object_registry_);
  }

 private:
  typedef base::hash_map<std::string, scoped_refptr<ObjectType> > UrlMap;
  UrlMap object_registry_;
};

template <typename ObjectType>
void UrlRegistry<ObjectType>::Register(
    const std::string& blob_url, const scoped_refptr<ObjectType>& object) {
  DCHECK(object);
  DCHECK(object_registry_.find(blob_url) == object_registry_.end());
  object_registry_.insert(std::make_pair(blob_url, object));
}

template <typename ObjectType>
scoped_refptr<ObjectType> UrlRegistry<ObjectType>::Retrieve(
    const std::string& blob_url) {
  typename UrlMap::iterator iter = object_registry_.find(blob_url);
  if (iter == object_registry_.end()) {
    DLOG(WARNING) << "Cannot find object for blob url " << blob_url;
    return NULL;
  }

  return iter->second;
}

template <typename ObjectType>
bool UrlRegistry<ObjectType>::Unregister(const std::string& blob_url) {
  typename UrlMap::iterator iter = object_registry_.find(blob_url);
  if (iter == object_registry_.end()) {
    return false;
  }

  object_registry_.erase(iter);
  return true;
}

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_URL_REGISTRY_H_
