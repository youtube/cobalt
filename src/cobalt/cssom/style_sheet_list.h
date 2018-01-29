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

#ifndef COBALT_CSSOM_STYLE_SHEET_LIST_H_
#define COBALT_CSSOM_STYLE_SHEET_LIST_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class StyleSheet;

typedef std::vector<scoped_refptr<StyleSheet> > StyleSheetVector;

// The StyleSheetList interface represents an ordered collection of CSS
// style sheets.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#the-stylesheetlist-interface
class StyleSheetList : public script::Wrappable, public MutationObserver {
 public:
  StyleSheetList();
  StyleSheetList(const StyleSheetVector& style_sheets,
                 MutationObserver* observer);

  // Web API: StyleSheetList
  //

  // Returns the index-th CSS style sheet in the collection.
  // Returns null if there is no index-th object in the collection.
  scoped_refptr<StyleSheet> Item(unsigned int index) const;

  // Returns the number of CSS style sheets represented by the collection.
  unsigned int length() const;

  // Custom, not in any spec.
  //
  // From MutationObserver.
  void OnCSSMutation() override;

  MutationObserver* mutation_observer() const { return mutation_observer_; }

  DEFINE_WRAPPABLE_TYPE(StyleSheetList);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~StyleSheetList();

  const StyleSheetVector style_sheets_;
  MutationObserver* const mutation_observer_;

  DISALLOW_COPY_AND_ASSIGN(StyleSheetList);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_STYLE_SHEET_LIST_H_
