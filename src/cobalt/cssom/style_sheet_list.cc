// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/style_sheet_list.h"

#include <limits>

#include "cobalt/cssom/style_sheet.h"

namespace cobalt {
namespace cssom {

StyleSheetList::StyleSheetList(MutationObserver* observer)
    : mutation_observer_(observer) {}

scoped_refptr<StyleSheet> StyleSheetList::Item(unsigned int index) const {
  return index < style_sheets_.size() ? style_sheets_[index] : NULL;
}

unsigned int StyleSheetList::length() const {
  CHECK_LE(style_sheets_.size(), std::numeric_limits<unsigned int>::max());
  return static_cast<unsigned int>(style_sheets_.size());
}

void StyleSheetList::OnCSSMutation() {
  if (mutation_observer_) {
    mutation_observer_->OnCSSMutation();
  }
}

void StyleSheetList::Append(const scoped_refptr<StyleSheet>& style_sheet) {
  style_sheet->AttachToStyleSheetList(this);
  style_sheet->set_index(static_cast<int>(style_sheets_.size()));
  style_sheets_.push_back(style_sheet);
  if (mutation_observer_) {
    mutation_observer_->OnCSSMutation();
  }
}

StyleSheetList::~StyleSheetList() {}

}  // namespace cssom
}  // namespace cobalt
