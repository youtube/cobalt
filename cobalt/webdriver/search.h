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

#ifndef WEBDRIVER_SEARCH_H_
#define WEBDRIVER_SEARCH_H_

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
        if (collection) {
          for (uint32 i = 0; i < collection->length(); ++i) {
            found_elements.push_back(collection->Item(i).get());
          }
        }
        break;
      }
      case protocol::SearchStrategy::kCssSelector: {
        scoped_refptr<dom::NodeList> node_list =
            node->QuerySelectorAll(strategy.parameter());
        if (node_list) {
          for (uint32 i = 0; i < node_list->length(); ++i) {
            scoped_refptr<dom::Element> element =
                node_list->Item(i)->AsElement();
            if (element) {
              found_elements.push_back(element.get());
            }
          }
        }
        break;
      }
      default:
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
};

}  // namespace webdriver
}  // namespace cobalt
#endif  // WEBDRIVER_SEARCH_H_
