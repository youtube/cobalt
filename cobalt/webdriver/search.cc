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

#include "cobalt/webdriver/search.h"

namespace cobalt {
namespace webdriver {

void Search::PopulateFindResults(
    const ElementVector& found_elements, ElementMapping* element_mapping,
    util::CommandResult<protocol::ElementId>* out_result) {
  DCHECK(!found_elements.empty());
  typedef util::CommandResult<protocol::ElementId> CommandResult;

  // Grab the first result from the list and return it.
  protocol::ElementId id =
      element_mapping->ElementToId(found_elements.front().get());
  *out_result = CommandResult(id);
}

void Search::PopulateFindResults(
    const ElementVector& found_elements, ElementMapping* element_mapping,
    util::CommandResult<std::vector<protocol::ElementId> >* out_result) {
  DCHECK(!found_elements.empty());
  typedef util::CommandResult<ElementIdVector> CommandResult;

  // Create a new ElementDriver for each result and return it.
  ElementIdVector id_vector;
  for (int i = 0; i < found_elements.size(); ++i) {
    protocol::ElementId id =
        element_mapping->ElementToId(found_elements[i].get());
    id_vector.push_back(id);
  }

  *out_result = CommandResult(id_vector);
}

}  // namespace webdriver
}  // namespace cobalt
