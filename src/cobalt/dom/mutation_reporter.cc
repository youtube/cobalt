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

#include "cobalt/dom/mutation_reporter.h"

#include "base/hash_tables.h"
#include "cobalt/dom/mutation_observer.h"
#include "cobalt/dom/mutation_observer_init.h"
#include "cobalt/dom/node.h"
#include "cobalt/dom/node_list.h"

namespace cobalt {
namespace dom {
namespace {

// Internal helper class to determine if a registered observer cares about a
// certain type of mutation, and for creating a mutation record for different
// types of mutations.
class MutationRecordBuilder {
 public:
  virtual bool IsInterested(const MutationObserverInit& options) = 0;
  virtual scoped_refptr<MutationRecord> CreateMutationRecord(
      const scoped_refptr<Node>& target) = 0;
};

// MutationRecordBuild for attribute mutations.
class AttributeMutationRecordBuilder : public MutationRecordBuilder {
 public:
  AttributeMutationRecordBuilder(const std::string& attribute_name,
                                 const base::optional<std::string>& old_value)
      : attribute_name_(attribute_name), old_value_(old_value) {}

  bool IsInterested(const MutationObserverInit& options) OVERRIDE {
    // https://www.w3.org/TR/dom/#queue-a-mutation-record
    // 2. If type is "attributes" and options's attributes is not true,
    // continue.
    if (!options.has_attributes() || !options.attributes()) {
      return false;
    }

    // 3. If type is "attributes", options's attributeFilter is present,
    //    and either options's attributeFilter does not contain name or
    //    namespace is non-null, continue.
    if (options.has_attribute_filter()) {
      // Cobalt doesn't support namespaces, so ignore that part of step (2).
      for (size_t i = 0; i < options.attribute_filter().size(); ++i) {
        if (options.attribute_filter().at(i) == attribute_name_) {
          return true;
        }
      }
      return false;
    }
    return true;
  }

  scoped_refptr<MutationRecord> CreateMutationRecord(
      const scoped_refptr<Node>& target) OVERRIDE {
    return MutationRecord::CreateAttributeMutationRecord(
        target, attribute_name_, old_value_);
  }

 private:
  std::string attribute_name_;
  base::optional<std::string> old_value_;
};

// MutationRecordBuild for character data mutations.
class CharacterDataMutationRecordBuilder : public MutationRecordBuilder {
 public:
  explicit CharacterDataMutationRecordBuilder(
      const std::string& old_character_data)
      : old_character_data_(old_character_data) {}

  bool IsInterested(const MutationObserverInit& options) OVERRIDE {
    // https://www.w3.org/TR/dom/#queue-a-mutation-record
    // 4. If type is "characterData" and options's characterData is not true,
    //    continue.
    return options.has_character_data() && options.character_data();
  }
  scoped_refptr<MutationRecord> CreateMutationRecord(
      const scoped_refptr<Node>& target) OVERRIDE {
    return MutationRecord::CreateCharacterDataMutationRecord(
        target, old_character_data_);
  }

 private:
  std::string old_character_data_;
};

// MutationRecordBuild for child list mutations.
class ChildListMutationRecordBuilder : public MutationRecordBuilder {
 public:
  ChildListMutationRecordBuilder(
      const scoped_refptr<dom::NodeList>& added_nodes,
      const scoped_refptr<dom::NodeList>& removed_nodes,
      const scoped_refptr<dom::Node>& previous_sibling,
      const scoped_refptr<dom::Node>& next_sibling)
      : added_nodes_(added_nodes),
        removed_nodes_(removed_nodes),
        previous_sibling_(previous_sibling),
        next_sibling_(next_sibling) {}

  bool IsInterested(const MutationObserverInit& options) OVERRIDE {
    // https://www.w3.org/TR/dom/#queue-a-mutation-record
    // 5. If type is "childList" and options's childList is false, continue.
    return options.child_list();
  }

  scoped_refptr<MutationRecord> CreateMutationRecord(
      const scoped_refptr<Node>& target) OVERRIDE {
    return MutationRecord::CreateChildListMutationRecord(
        target, added_nodes_, removed_nodes_, previous_sibling_, next_sibling_);
  }

 private:
  scoped_refptr<dom::NodeList> added_nodes_;
  scoped_refptr<dom::NodeList> removed_nodes_;
  scoped_refptr<dom::Node> previous_sibling_;
  scoped_refptr<dom::Node> next_sibling_;
};

// Iterate through |registered_observers| and create a new MutationRecord via
// |record_builder| for interested observers.
void ReportToInterestedObservers(
    const scoped_refptr<Node>& target,
    MutationReporter::RegisteredObserverVector* registered_observers,
    MutationRecordBuilder* record_builder) {
  typedef base::hash_set<MutationObserver*> MutationObserverSet;
  MutationObserverSet reported_observers;
  for (size_t i = 0; i < registered_observers->size(); ++i) {
    const RegisteredObserver& registered_observer = registered_observers->at(i);
    MutationObserver* observer = registered_observer.observer().get();
    const MutationObserverInit& options = registered_observer.options();

    // The mutation has already been reported to this observer, so skip it, per
    // step 3.6 (https://www.w3.org/TR/dom/#queue-a-mutation-record)
    if (reported_observers.find(observer) != reported_observers.end()) {
      continue;
    }

    // https://www.w3.org/TR/dom/#queue-a-mutation-record
    // 1. If node is not target and options's subtree is false, continue.
    if (registered_observer.target() != target && !options.subtree()) {
      continue;
    }
    // This observer doesn't care about this mutation, so skip it.
    if (!record_builder->IsInterested(options)) {
      continue;
    }
    // Queue a mutation record on the observer.
    observer->QueueMutationRecord(record_builder->CreateMutationRecord(target));
    reported_observers.insert(observer);
  }
}

}  // namespace

MutationReporter::MutationReporter(
    dom::Node* target,
    scoped_ptr<RegisteredObserverVector> registered_observers)
    : target_(target), observers_(registered_observers.Pass()) {}

MutationReporter::~MutationReporter() {}

// Implement the "queue a mutation record" algorithm.
// https://www.w3.org/TR/dom/#queue-a-mutation-record
void MutationReporter::ReportAttributesMutation(
    const std::string& name,
    const base::optional<std::string>& old_value) const {
  AttributeMutationRecordBuilder record_builder(name, old_value);
  ReportToInterestedObservers(target_, observers_.get(), &record_builder);
}

void MutationReporter::ReportCharacterDataMutation(
    const std::string& old_value) const {
  CharacterDataMutationRecordBuilder record_builder(old_value);
  ReportToInterestedObservers(target_, observers_.get(), &record_builder);
}

void MutationReporter::ReportChildListMutation(
    const scoped_refptr<dom::NodeList>& added_nodes,
    const scoped_refptr<dom::NodeList>& removed_nodes,
    const scoped_refptr<dom::Node>& previous_sibling,
    const scoped_refptr<dom::Node>& next_sibling) const {
  ChildListMutationRecordBuilder record_builder(added_nodes, removed_nodes,
                                                previous_sibling, next_sibling);
  ReportToInterestedObservers(target_, observers_.get(), &record_builder);
}
}  // namespace dom
}  // namespace cobalt
