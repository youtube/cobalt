/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_STYLE_SHEET_LIST_H_
#define CSSOM_STYLE_SHEET_LIST_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSStyleSheet;

// The StyleSheetList interface represents an ordered collection of CSS
// style sheets.
//   http://dev.w3.org/csswg/cssom/#the-stylesheetlist-interface
//
// While the specification theoretically allows style sheets of type other
// than CSS, Cobalt is hard-coded to support CSS only.
class StyleSheetList : public script::Wrappable, public MutationObserver {
 public:
  // If no layout mutation reporting needed, |observer| can be null.
  explicit StyleSheetList(MutationObserver* observer);

  // Web API: StyleSheetList
  //

  // Returns the index-th CSS style sheet in the collection.
  // Returns null if there is no index-th object in the collection.
  scoped_refptr<CSSStyleSheet> Item(unsigned int index) const;

  // Returns the number of CSS style sheets represented by the collection.
  unsigned int length() const;

  // From cssom::MutationObserver.
  void OnCSSMutation();

  // Custom, not in any spec.
  //
  void Append(const scoped_refptr<CSSStyleSheet>& style_sheet);

  MutationObserver* mutation_observer() const { return mutation_observer_; }

  DEFINE_WRAPPABLE_TYPE(StyleSheetList);

 private:
  ~StyleSheetList();

  std::vector<scoped_refptr<CSSStyleSheet> > style_sheets_;
  MutationObserver* const mutation_observer_;

  DISALLOW_COPY_AND_ASSIGN(StyleSheetList);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_STYLE_SHEET_LIST_H_
