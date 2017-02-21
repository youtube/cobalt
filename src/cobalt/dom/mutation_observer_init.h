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
#include "cobalt/script/sequence.h"

namespace cobalt {
namespace dom {

// Represents the MutationObserverInit dictionary type that is used to
// initialize the MutationObserver interface.
// https://www.w3.org/TR/dom/#mutationobserver
class MutationObserverInit {
 public:
  typedef script::Sequence<std::string> StringSequence;

  MutationObserverInit()
      : child_list_(false),
        attributes_(false),
        has_attributes_(false),
        character_data_(false),
        has_character_data_(false),
        subtree_(false),
        attribute_old_value_(false),
        has_attribute_old_value_(false),
        character_data_old_value_(false),
        has_character_data_old_value_(false),
        has_attribute_filter_(false) {}

  bool child_list() const { return child_list_; }
  void set_child_list(bool value) { child_list_ = value; }

  bool attributes() const {
    DCHECK(has_attributes_);
    return attributes_;
  }
  void set_attributes(bool value) {
    has_attributes_ = true;
    attributes_ = value;
  }
  bool has_attributes() const { return has_attributes_; }

  bool character_data() const {
    DCHECK(has_character_data_);
    return character_data_;
  }
  void set_character_data(bool value) {
    has_character_data_ = true;
    character_data_ = value;
  }
  bool has_character_data() const { return has_character_data_; }

  bool subtree() const { return subtree_; }
  void set_subtree(bool value) { subtree_ = value; }

  bool attribute_old_value() const {
    DCHECK(has_attribute_old_value_);
    return attribute_old_value_;
  }
  void set_attribute_old_value(bool value) {
    has_attribute_old_value_ = true;
    attribute_old_value_ = value;
  }
  bool has_attribute_old_value() const { return has_attribute_old_value_; }

  bool character_data_old_value() const {
    DCHECK(has_character_data_old_value_);
    return character_data_old_value_;
  }
  void set_character_data_old_value(bool value) {
    has_character_data_old_value_ = true;
    character_data_old_value_ = value;
  }
  bool has_character_data_old_value() const {
    return has_character_data_old_value_;
  }

  const StringSequence& attribute_filter() const {
    DCHECK(has_attribute_filter_);
    return attribute_filter_;
  }
  void set_attribute_filter(StringSequence value) {
    has_attribute_filter_ = true;
    attribute_filter_ = value;
  }
  bool has_attribute_filter() const { return has_attribute_filter_; }

 private:
  // Dictionary members:
  bool child_list_;

  bool attributes_;
  bool has_attributes_;

  bool character_data_;
  bool has_character_data_;

  bool subtree_;

  bool attribute_old_value_;
  bool has_attribute_old_value_;

  bool character_data_old_value_;
  bool has_character_data_old_value_;

  StringSequence attribute_filter_;
  bool has_attribute_filter_;
};
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MUTATION_OBSERVER_INIT_H_
