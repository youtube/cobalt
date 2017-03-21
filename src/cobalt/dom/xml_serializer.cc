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

#include "cobalt/dom/xml_serializer.h"

#include <sstream>

#include "cobalt/dom/node.h"
#include "cobalt/dom/serializer.h"

namespace cobalt {
namespace dom {

std::string XMLSerializer::SerializeToString(const scoped_refptr<Node>& root) {
  std::ostringstream oss;
  Serializer serializer(&oss);
  serializer.Serialize(root);
  return oss.str();
}

}  // namespace dom
}  // namespace cobalt
