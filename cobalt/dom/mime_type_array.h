// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_MIME_TYPE_ARRAY_H_
#define COBALT_DOM_MIME_TYPE_ARRAY_H_

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// A MimeTypeArray object represents the MIME types explicitly supported by
// plugins supported by the user agent, each of which is represented by a
// MimeType object. Only stub support is currently required.
// https://www.w3.org/html/wg/drafts/html/master/webappapis.html#mimetypearray
class MimeTypeArray : public script::Wrappable {
 public:
  MimeTypeArray();

  // Web API: PluginArray
  int length() const;

  DEFINE_WRAPPABLE_TYPE(MimeTypeArray);

 private:
  ~MimeTypeArray() override {}

  DISALLOW_COPY_AND_ASSIGN(MimeTypeArray);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MIME_TYPE_ARRAY_H_
