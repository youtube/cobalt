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

#ifndef COBALT_DOM_NODE_LIST_LIVE_H_
#define COBALT_DOM_NODE_LIST_LIVE_H_

#include "cobalt/dom/node_list.h"

#include "base/memory/ref_counted.h"

namespace cobalt {
namespace dom {

class Node;

// A NodeListLive is a live NodeList, which is a collection of nodes.
//    https://www.w3.org/TR/2015/WD-dom-20150428/#interface-nodelist
class NodeListLive : public NodeList {
 public:
  // Create a live collection of all first-level child nodes.
  static scoped_refptr<NodeListLive> CreateWithChildren(
      const scoped_refptr<const Node>& base);

  // Web API: NodeList
  //
  unsigned int length() override;

  scoped_refptr<Node> Item(unsigned int item) override;

  // Custom, not in any spec.
  //
  void MaybeRefreshCollection();

 private:
  explicit NodeListLive(const scoped_refptr<const Node>& base);
  ~NodeListLive() override {}

  // Base node that was used to generate the collection.
  const scoped_refptr<const Node> base_;
  // Generation of the base node that was used to create the cache.
  uint32_t base_node_generation_;

  DISALLOW_COPY_AND_ASSIGN(NodeListLive);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NODE_LIST_LIVE_H_
