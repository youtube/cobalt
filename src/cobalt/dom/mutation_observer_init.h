/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_MUTATION_OBSERVER_INIT_H_
#define COBALT_DOM_MUTATION_OBSERVER_INIT_H_

#include <string>
#include <vector>

#include "base/optional.h"

namespace cobalt {
namespace dom {

// Represents the MutationObserverInit dictionary type that is used to
// initialize the MutationObserver interface.
// https://www.w3.org/TR/dom/#mutationobserver
struct MutationObserverInit {
  typedef std::vector<std::string> StringSequence;

  // Default values as specified in the IDL.
  MutationObserverInit() : child_list(false), subtree(false) {}

  // Dictionary members:
  bool child_list;
  base::optional<bool> attributes;
  base::optional<bool> character_data;
  bool subtree;
  base::optional<bool> attribute_old_value;
  base::optional<bool> character_data_old_value;
  base::optional<StringSequence> attribute_filter;
};
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MUTATION_OBSERVER_INIT_H_
