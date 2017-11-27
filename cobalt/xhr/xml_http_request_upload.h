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

#ifndef COBALT_XHR_XML_HTTP_REQUEST_UPLOAD_H_
#define COBALT_XHR_XML_HTTP_REQUEST_UPLOAD_H_

#include "cobalt/xhr/xml_http_request_event_target.h"

namespace cobalt {
namespace xhr {

class XMLHttpRequestUpload : public xhr::XMLHttpRequestEventTarget {
 public:
  XMLHttpRequestUpload() {}

  DEFINE_WRAPPABLE_TYPE(XMLHttpRequestUpload);

 protected:
  ~XMLHttpRequestUpload() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(XMLHttpRequestUpload);
};

}  // namespace xhr
}  // namespace cobalt

#endif  // COBALT_XHR_XML_HTTP_REQUEST_UPLOAD_H_
