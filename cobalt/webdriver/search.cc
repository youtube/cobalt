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

#include "cobalt/webdriver/search.h"

namespace cobalt {
namespace webdriver {

template <>
util::CommandResult<protocol::ElementId> Search::PopulateFindResults(
    const ElementVector& found_elements, ElementMapping* element_mapping) {
  typedef util::CommandResult<protocol::ElementId> CommandResult;
  // Return NoSuchElement if there are no results.
  if (found_elements.empty()) {
    return CommandResult(protocol::Response::kNoSuchElement);
  }

  // Grab the first result from the list and return it.
  protocol::ElementId id =
      element_mapping->ElementToId(found_elements.front().get());
  return CommandResult(id);
}

template <>
util::CommandResult<std::vector<protocol::ElementId> >
Search::PopulateFindResults(const ElementVector& found_elements,
                            ElementMapping* element_mapping) {
  typedef util::CommandResult<ElementIdVector> CommandResult;

  // Create a new ElementDriver for each result and return it.
  ElementIdVector id_vector;
  for (int i = 0; i < found_elements.size(); ++i) {
    protocol::ElementId id =
        element_mapping->ElementToId(found_elements[i].get());
    id_vector.push_back(id);
  }

  return CommandResult(id_vector);
}

void Search::HTMLCollectionToElementVector(
    const scoped_refptr<dom::HTMLCollection>& html_collection,
    ElementVector* element_vector) {
  if (html_collection) {
    for (uint32 i = 0; i < html_collection->length(); ++i) {
      element_vector->push_back(html_collection->Item(i).get());
    }
  }
}

void Search::NodeListToElementVector(
    const scoped_refptr<dom::NodeList>& node_list,
    ElementVector* element_vector) {
  if (node_list) {
    for (uint32 i = 0; i < node_list->length(); ++i) {
      scoped_refptr<dom::Element> element = node_list->Item(i)->AsElement();
      if (element) {
        element_vector->push_back(element.get());
      }
    }
  }
}

}  // namespace webdriver
}  // namespace cobalt
