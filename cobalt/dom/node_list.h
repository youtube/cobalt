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

#ifndef COBALT_DOM_NODE_LIST_H_
#define COBALT_DOM_NODE_LIST_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Node;

// A NodeList object is a collection of nodes.
//    https://www.w3.org/TR/2015/WD-dom-20150428/#interface-nodelist
class NodeList : public script::Wrappable {
 public:
  NodeList();

  // Web API: NodeList
  //
  virtual uint32 length();

  virtual scoped_refptr<Node> Item(uint32 item);

  // Custom, not in any spec.
  //
  void Clear();

  void AppendNode(const scoped_refptr<Node>& node);

  DEFINE_WRAPPABLE_TYPE(NodeList);

 protected:
  ~NodeList() override;

  void ReserveForInternalCollection(int capacity) {
    collection_.reserve(capacity);
  }

 private:
  std::vector<scoped_refptr<Node> > collection_;

  DISALLOW_COPY_AND_ASSIGN(NodeList);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NODE_LIST_H_
