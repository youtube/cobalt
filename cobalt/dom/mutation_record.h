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

#ifndef COBALT_DOM_MUTATION_RECORD_H_
#define COBALT_DOM_MUTATION_RECORD_H_

#include <string>

#include "base/optional.h"
#include "cobalt/dom/node.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// MutationRecords are used with the MutationObserver interface to describe a
// mutation on the documnet.
// https://www.w3.org/TR/dom/#mutationrecord
class MutationRecord : public script::Wrappable {
 public:
  // Web API: MutationRecord
  std::string type() { return ""; }
  scoped_refptr<dom::Node> target() { return NULL; }
  scoped_refptr<dom::NodeList> added_nodes() { return NULL; }
  scoped_refptr<dom::NodeList> removed_nodes() { return NULL; }
  scoped_refptr<dom::Node> previous_sibling() { return NULL; }
  scoped_refptr<dom::Node> next_sibling() { return NULL; }
  base::optional<std::string> attribute_name() { return base::nullopt; }
  base::optional<std::string> attribute_namespace() { return base::nullopt; }
  base::optional<std::string> old_value() { return base::nullopt; }

  DEFINE_WRAPPABLE_TYPE(MutationRecord);
};
}  // namespace dom
}  // namespace cobalt
#endif  // COBALT_DOM_MUTATION_RECORD_H_
