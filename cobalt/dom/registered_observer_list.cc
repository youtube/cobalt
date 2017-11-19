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

#include "cobalt/dom/registered_observer_list.h"

namespace cobalt {
namespace dom {
namespace {
// This algorithm is part of the description of the observe() method. It may
// modify |options| and returns false if a TypeError should be thrown.
// https://www.w3.org/TR/dom/#dom-mutationobserver-observe
bool InitializeOptions(MutationObserverInit* options) {
  if (options->has_attribute_old_value() || options->has_attribute_filter()) {
    // 1. If either options' attributeOldValue or attributeFilter is present and
    //    options' attributes is omitted, set options' attributes to true.
    if (!options->has_attributes()) {
      options->set_attributes(true);
    }
    // 4. If options' attributeOldValue is true and options' attributes is
    //    false, throw a JavaScript TypeError.
    // 5. If options' attributeFilter is present and options' attributes is
    //    false, throw a JavaScript TypeError.
    if (!options->attributes()) {
      return false;
    }
  }
  if (options->has_character_data_old_value()) {
    // 2. If options' characterDataOldValue is present and options'
    //    characterData is omitted, set options' characterData to true.
    if (!options->has_character_data()) {
      options->set_character_data(true);
    }
    // 6. If options' characterDataOldValue is true and options' characterData
    //    is false, throw a JavaScript TypeError.
    if (!options->character_data()) {
      return false;
    }
  }
  // 3. If none of options' childList attributes, and characterData is true,
  // throw a TypeError.
  const bool child_list = options->child_list();
  const bool attributes = options->has_attributes() && options->attributes();
  const bool character_data =
      options->has_character_data() && options->character_data();
  if (!(child_list || attributes || character_data)) {
    return false;
  }
  return true;
}
}  // namespace

bool RegisteredObserverList::AddMutationObserver(
    const scoped_refptr<MutationObserver>& observer,
    const MutationObserverInit& const_options) {
  MutationObserverInit options(const_options);
  if (!InitializeOptions(&options)) {
    return false;
  }
  // https://www.w3.org/TR/dom/#dom-mutationobserver-observe
  // 7. For each registered observer registered in target's list of registered
  //    observers whose observer is the context object:
  //        1. Remove all transient registered observers whose source is
  //           registered.
  //        2. Replace registered's options with options.
  typedef RegisteredObserverVector::iterator RegisteredObserverIterator;
  for (RegisteredObserverIterator it = registered_observers_.begin();
       it != registered_observers_.end(); ++it) {
    // TODO: Remove transient registered observers.
    if (it->observer() == observer) {
      it->set_options(options);
      return true;
    }
  }
  // 8. Otherwise, add a new registered observer to target's list of registered
  //    observers with the context object as the observer and options as the
  //    options, and add target to context object's list of nodes on which it is
  //    registered.
  registered_observers_.push_back(
      RegisteredObserver(target_, observer, options));
  return true;
}

void RegisteredObserverList::RemoveMutationObserver(
    const scoped_refptr<MutationObserver>& observer) {
  typedef RegisteredObserverVector::iterator RegisteredObserverIterator;
  for (RegisteredObserverIterator it = registered_observers_.begin();
       it != registered_observers_.end(); ++it) {
    if (it->observer() == observer) {
      registered_observers_.erase(it);
      return;
    }
  }
  NOTREACHED() << "Did not find a mutation observer to unregister.";
}

void RegisteredObserverList::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(registered_observers_);
}

}  // namespace dom
}  // namespace cobalt
