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

#include "cobalt/dom/media_source.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/logging.h"

namespace cobalt {
namespace dom {

namespace {

typedef std::map<std::string, scoped_refptr<MediaSource> > MediaSourceRegistry;

base::LazyInstance<MediaSourceRegistry> s_media_source_registry =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void MediaSource::Registry::Register(
    const std::string& blob_url,
    const scoped_refptr<MediaSource>& media_source) {
  MediaSourceRegistry& registry = s_media_source_registry.Get();
  DCHECK(registry.find(blob_url) == registry.end());
  registry.insert(std::make_pair(blob_url, media_source));
}

// static
scoped_refptr<MediaSource> MediaSource::Registry::Retrieve(
    const std::string& blob_url) {
  MediaSourceRegistry& registry = s_media_source_registry.Get();
  MediaSourceRegistry::iterator iter = registry.find(blob_url);
  if (iter == registry.end()) {
    DLOG(WARNING) << "Cannot find MediaSource object for blob url " << blob_url;
    return NULL;
  }

  return iter->second;
}

// static
void MediaSource::Registry::Unregister(const std::string& blob_url) {
  MediaSourceRegistry& registry = s_media_source_registry.Get();
  MediaSourceRegistry::iterator iter = registry.find(blob_url);
  if (iter == registry.end()) {
    DLOG(WARNING) << "Cannot find MediaSource object for blob url " << blob_url;
    return;
  }

  registry.erase(iter);
}

}  // namespace dom
}  // namespace cobalt
