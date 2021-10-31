// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_STYLE_SHEET_H_
#define COBALT_CSSOM_STYLE_SHEET_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/wrappable.h"
#include "url/gurl.h"

namespace cobalt {
namespace cssom {

class CSSStyleSheet;
class StyleSheetList;

// The StyleSheet interface represents an abstract, base style sheet.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#the-stylesheet-interface
class StyleSheet : public script::Wrappable,
                   public base::SupportsWeakPtr<StyleSheet> {
 public:
  // Custom, not in any spec.
  //
  int index() { return index_; }
  void set_index(int index) { index_ = index; }

  virtual void AttachToStyleSheetList(StyleSheetList* style_sheet_list) = 0;
  virtual void DetachFromStyleSheetList() = 0;
  virtual StyleSheetList* ParentStyleSheetList() const = 0;
  virtual void SetLocationUrl(const GURL& url) = 0;
  virtual const GURL& LocationUrl() const = 0;
  virtual scoped_refptr<CSSStyleSheet> AsCSSStyleSheet() = 0;

  DEFINE_WRAPPABLE_TYPE(StyleSheet);

 protected:
  StyleSheet();
  virtual ~StyleSheet() {}

 private:
  int index_;

  DISALLOW_COPY_AND_ASSIGN(StyleSheet);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_STYLE_SHEET_H_
