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

#ifndef COBALT_WEBDRIVER_SEARCH_H_
#define COBALT_WEBDRIVER_SEARCH_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/webdriver/element_mapping.h"
#include "cobalt/webdriver/protocol/search_strategy.h"
#include "cobalt/webdriver/util/command_result.h"

namespace cobalt {
namespace webdriver {

class Search {
 public:
  template <typename T, typename N>
  static util::CommandResult<T> FindElementsUnderNode(
      const protocol::SearchStrategy& strategy, N* node,
      ElementMapping* element_mapping) {
    typedef util::CommandResult<T> CommandResult;
    ElementVector found_elements;
    switch (strategy.strategy()) {
      case protocol::SearchStrategy::kClassName: {
        scoped_refptr<dom::HTMLCollection> collection =
            node->GetElementsByClassName(strategy.parameter());
        HTMLCollectionToElementVector(collection, &found_elements);
        break;
      }
      case protocol::SearchStrategy::kTagName: {
        scoped_refptr<dom::HTMLCollection> collection =
            node->GetElementsByTagName(strategy.parameter());
        HTMLCollectionToElementVector(collection, &found_elements);
        break;
      }
      case protocol::SearchStrategy::kCssSelector: {
        scoped_refptr<dom::NodeList> node_list =
            node->QuerySelectorAll(strategy.parameter());
        NodeListToElementVector(node_list, &found_elements);
        break;
      }
      case protocol::SearchStrategy::kId:
      case protocol::SearchStrategy::kLinkText:
      case protocol::SearchStrategy::kName:
      case protocol::SearchStrategy::kPartialLinkText:
      case protocol::SearchStrategy::kXPath:
        NOTIMPLEMENTED();
    }
    return PopulateFindResults<T>(found_elements, element_mapping);
  }

 private:
  typedef std::vector<scoped_refptr<dom::Element> > ElementVector;
  typedef std::vector<protocol::ElementId> ElementIdVector;

  template <typename T>
  static util::CommandResult<T> PopulateFindResults(
      const ElementVector& found_elements, ElementMapping* element_mapping);

  static void HTMLCollectionToElementVector(
      const scoped_refptr<dom::HTMLCollection>& html_collection,
      ElementVector* element_vector);
  static void NodeListToElementVector(
      const scoped_refptr<dom::NodeList>& node_list,
      ElementVector* element_vector);
};

}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_SEARCH_H_
