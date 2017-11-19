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

#include "cobalt/dom/mutation_record.h"

#include "cobalt/dom/node.h"
#include "cobalt/dom/node_list.h"

namespace cobalt {
namespace dom {

scoped_refptr<MutationRecord> MutationRecord::CreateAttributeMutationRecord(
    const scoped_refptr<Node>& target, const std::string& attribute_name,
    const base::optional<std::string>& old_value) {
  scoped_refptr<MutationRecord> record =
      new MutationRecord(base::Tokens::attributes(), target);
  record->attribute_name_ = attribute_name;
  record->old_value_ = old_value;
  return record;
}

scoped_refptr<MutationRecord> MutationRecord::CreateCharacterDataMutationRecord(
    const scoped_refptr<Node>& target,
    const base::optional<std::string>& old_character_data) {
  scoped_refptr<MutationRecord> record =
      new MutationRecord(base::Tokens::characterData(), target);
  record->old_value_ = old_character_data;
  return record;
}

scoped_refptr<MutationRecord> MutationRecord::CreateChildListMutationRecord(
    const scoped_refptr<Node>& target,
    const scoped_refptr<dom::NodeList>& added_nodes,
    const scoped_refptr<dom::NodeList>& removed_nodes,
    const scoped_refptr<dom::Node>& previous_sibling,
    const scoped_refptr<dom::Node>& next_sibling) {
  scoped_refptr<MutationRecord> record =
      new MutationRecord(base::Tokens::childList(), target);
  if (added_nodes) {
    record->added_nodes_ = added_nodes;
  }
  if (removed_nodes) {
    record->removed_nodes_ = removed_nodes;
  }
  record->previous_sibling_ = previous_sibling;
  record->next_sibling_ = next_sibling;
  return record;
}

void MutationRecord::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(target_);
  tracer->Trace(added_nodes_);
  tracer->Trace(removed_nodes_);
  tracer->Trace(previous_sibling_);
  tracer->Trace(next_sibling_);
}

MutationRecord::MutationRecord(const base::Token& type,
                               const scoped_refptr<Node>& target)
    : type_(type),
      target_(target),
      added_nodes_(new NodeList()),
      removed_nodes_(new NodeList()) {}

MutationRecord::~MutationRecord() {}

}  // namespace dom
}  // namespace cobalt
