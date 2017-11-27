// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_FONT_FACE_UPDATER_H_
#define COBALT_DOM_FONT_FACE_UPDATER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/dom/font_cache.h"
#include "googleurl/src/gurl.h"

namespace cobalt {

namespace cssom {

class CSSStyleSheet;
class StyleSheetList;

}  // cssom

namespace dom {

// Collect valid FontFaces from CSSStyleSheets and StyleSheetLists and provide
// them to the FontCache, which replaces its previous set with them.
class FontFaceUpdater : public cssom::CSSRuleVisitor {
 public:
  FontFaceUpdater(const GURL& document_base_url, FontCache* const cache);
  ~FontFaceUpdater();

  void ProcessCSSStyleSheet(
      const scoped_refptr<cssom::CSSStyleSheet>& style_sheet);
  void ProcessStyleSheetList(
      const scoped_refptr<cssom::StyleSheetList>& style_sheet_list);

 private:
  void VisitCSSStyleRule(cssom::CSSStyleRule* /*css_style_rule*/) override {}
  void VisitCSSFontFaceRule(
      cssom::CSSFontFaceRule* css_font_face_rule) override;
  void VisitCSSMediaRule(cssom::CSSMediaRule* /*css_media_rule*/) override {}
  void VisitCSSKeyframeRule(
      cssom::CSSKeyframeRule* /*css_keyframe_rule*/) override {}
  void VisitCSSKeyframesRule(
      cssom::CSSKeyframesRule* /*css_keyframes_rule*/) override {}

  const GURL& document_base_url_;
  FontCache* const cache_;
  scoped_ptr<FontCache::FontFaceMap> font_face_map_;

  DISALLOW_COPY_AND_ASSIGN(FontFaceUpdater);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_FONT_FACE_UPDATER_H_
