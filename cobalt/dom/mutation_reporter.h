// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_MUTATION_REPORTER_H_
#define COBALT_DOM_MUTATION_REPORTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/registered_observer.h"

namespace cobalt {
namespace dom {

class Node;
class NodeList;

// Helper class that reports a mutation to interested MutationObservers
// according to the "queue a mutation record" algorithm:
// https://www.w3.org/TR/dom/#queue-a-mutation-record
//
// This class is intended to be used internally in the Node class.
class MutationReporter {
 public:
  typedef std::vector<RegisteredObserver> RegisteredObserverVector;
  // Construct a MutationReporter with a list of potentially interested
  // RegisteredObservers. In practice, this list of |registered_observers| will
  // the RegisteredObservers registered to the node that is being mutated and
  // its ancestors. |registered_observers| may contain duplicates.
  MutationReporter(
      dom::Node* target,
      std::unique_ptr<RegisteredObserverVector> registered_observers);

  ~MutationReporter();

  // Implement the "queue a mutation record" algorithm for the different types
  // of mutations.
  void ReportAttributesMutation(
      const std::string& name,
      const base::Optional<std::string>& old_value) const;
  void ReportCharacterDataMutation(const std::string& old_value) const;
  void ReportChildListMutation(
      const scoped_refptr<dom::NodeList>& added_nodes,
      const scoped_refptr<dom::NodeList>& removed_nodes,
      const scoped_refptr<dom::Node>& previous_sibling,
      const scoped_refptr<dom::Node>& next_sibling) const;

 private:
  dom::Node* target_;
  std::unique_ptr<RegisteredObserverVector> observers_;
};
}  // namespace dom
}  // namespace cobalt
#endif  // COBALT_DOM_MUTATION_REPORTER_H_
