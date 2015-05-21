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

#ifndef DOM_MEDIA_SOURCE_H_
#define DOM_MEDIA_SOURCE_H_

#include <string>

#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {

// TODO(***REMOVED***): This is a stub so we can implement URL.createObjectURL().
// Proper implementation will be completed in follow up CLs.
class MediaSource : public EventTarget {
 public:
  // This class manages a registry of MediaSource objects.  Its user can
  // associate a MediaSource object to a blob url as well as to retrieve a
  // MediaSource object by a blob url.  This is because to assign a MediaSource
  // object to the src of an HTMLMediaElement, we have to first convert the
  // MediaSource object into a blob url by calling URL.createObjectURL().
  // And eventually the HTMLMediaElement have to retrieve the MediaSource object
  // from the blob url.
  // Note: It is unsafe to directly encode the pointer to the MediaSource object
  // in the url as the url is assigned from JavaScript.
  class Registry {
   public:
    static void Register(const std::string& blob_url,
                         const scoped_refptr<MediaSource>& media_source);
    static scoped_refptr<MediaSource> Retrieve(const std::string& blob_url);
    static void Unregister(const std::string& blob_url);
  };

  DEFINE_WRAPPABLE_TYPE(MediaSource);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_MEDIA_SOURCE_H_
