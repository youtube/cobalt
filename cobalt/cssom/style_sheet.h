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

#ifndef CSSOM_STYLE_SHEET_H_
#define CSSOM_STYLE_SHEET_H_

#include "base/memory/ref_counted.h"
#include "cobalt/script/wrappable.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace cssom {

class StyleSheetList;

// The StyleSheet interface represents an abstract, base style sheet.
//   http://dev.w3.org/csswg/cssom/#the-stylesheet-interface
class StyleSheet : public script::Wrappable {
 public:
  virtual ~StyleSheet() {}

  virtual void AttachToStyleSheetList(StyleSheetList* style_sheet_list) = 0;
  virtual void SetLocationUrl(const GURL& url) = 0;
  virtual GURL& LocationUrl() = 0;
  virtual StyleSheetList* ParentStyleSheetList() = 0;

  DEFINE_WRAPPABLE_TYPE(StyleSheet);

 protected:
  StyleSheet() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StyleSheet);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_STYLE_SHEET_H_
