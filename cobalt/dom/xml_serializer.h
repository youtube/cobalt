// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_XML_SERIALIZER_H_
#define COBALT_DOM_XML_SERIALIZER_H_

#include <string>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Node;

// XMLSerializer can be used to convert DOM subtree or DOM document into text.
// XMLSerializer is available to unprivileged scripts.
//   https://www.w3.org/TR/DOM-Parsing/#the-domparser-interface
class XMLSerializer : public script::Wrappable {
 public:
  XMLSerializer() {}

  // Web API: XMLSerializer
  std::string SerializeToString(const scoped_refptr<Node>& root);

  DEFINE_WRAPPABLE_TYPE(XMLSerializer);

 private:
  DISALLOW_COPY_AND_ASSIGN(XMLSerializer);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_XML_SERIALIZER_H_
