// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_MUTATION_RECORD_H_
#define COBALT_DOM_MUTATION_RECORD_H_

#include <string>

#include "base/optional.h"
#include "cobalt/base/token.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Node;
class NodeList;

// MutationRecords are used with the MutationObserver interface to describe a
// mutation on the documnet.
// https://www.w3.org/TR/dom/#mutationrecord
class MutationRecord : public script::Wrappable {
 public:
  // Web API: MutationRecord
  const base::Token& type() { return type_; }
  const scoped_refptr<dom::Node>& target() { return target_; }
  const scoped_refptr<dom::NodeList>& added_nodes() { return added_nodes_; }
  const scoped_refptr<dom::NodeList>& removed_nodes() { return removed_nodes_; }
  const scoped_refptr<dom::Node>& previous_sibling() {
    return previous_sibling_;
  }
  const scoped_refptr<dom::Node>& next_sibling() { return next_sibling_; }
  const base::optional<std::string>& attribute_name() {
    return attribute_name_;
  }
  const base::optional<std::string>& attribute_namespace() {
    return attribute_namespace_;
  }
  const base::optional<std::string>& old_value() { return old_value_; }

  // Not part of the MutationRecord interface. These create functions implement
  // the part of the "queueing a mutation record" algorithm pertaining to
  // creating a new Mutation Record (step 4).
  // https://www.w3.org/TR/dom/#queue-a-mutation-record
  static scoped_refptr<MutationRecord> CreateAttributeMutationRecord(
      const scoped_refptr<Node>& target, const std::string& attribute_name,
      const base::optional<std::string>& old_value);

  static scoped_refptr<MutationRecord> CreateCharacterDataMutationRecord(
      const scoped_refptr<Node>& target,
      const base::optional<std::string>& old_character_data);

  static scoped_refptr<MutationRecord> CreateChildListMutationRecord(
      const scoped_refptr<Node>& target,
      const scoped_refptr<dom::NodeList>& added_nodes,
      const scoped_refptr<dom::NodeList>& removed_nodes,
      const scoped_refptr<dom::Node>& previous_sibling,
      const scoped_refptr<dom::Node>& next_sibling);

  DEFINE_WRAPPABLE_TYPE(MutationRecord);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  MutationRecord(const base::Token& type, const scoped_refptr<Node>& target);
  ~MutationRecord();

  base::Token type_;
  scoped_refptr<dom::Node> target_;
  scoped_refptr<dom::NodeList> added_nodes_;
  scoped_refptr<dom::NodeList> removed_nodes_;
  scoped_refptr<dom::Node> previous_sibling_;
  scoped_refptr<dom::Node> next_sibling_;
  base::optional<std::string> attribute_name_;
  base::optional<std::string> attribute_namespace_;
  base::optional<std::string> old_value_;
};
}  // namespace dom
}  // namespace cobalt
#endif  // COBALT_DOM_MUTATION_RECORD_H_
