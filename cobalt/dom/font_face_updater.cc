/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/font_face_updater.h"

#include <string>

#include "cobalt/cssom/css_font_face_rule.h"
#include "cobalt/cssom/css_media_rule.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/cssom/url_src_value.h"
#include "cobalt/cssom/url_value.h"

namespace cobalt {
namespace dom {

namespace {

//////////////////////////////////////////////////////////////////////////
// FontFaceProvider
//////////////////////////////////////////////////////////////////////////

// Visit a single font face rule, generating the FontFace from its values and
// verify that it is a valid rule.
// TODO(***REMOVED***): Handle local font faces, unicode ranges, and factor in styles.
class FontFaceProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit FontFaceProvider(const GURL& base_url) : base_url_(base_url) {}

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;
  void VisitPropertyList(cssom::PropertyListValue* property_list) OVERRIDE;
  void VisitString(cssom::StringValue* percentage) OVERRIDE;
  void VisitUrlSrc(cssom::UrlSrcValue* url_src) OVERRIDE;
  void VisitURL(cssom::URLValue* url) OVERRIDE;

  bool IsFontFaceValid() const;

  const std::string& font_family() const { return font_family_; }
  const GURL& url() const { return url_; }

 private:
  std::string font_family_;
  GURL url_;
  const GURL& base_url_;

  DISALLOW_COPY_AND_ASSIGN(FontFaceProvider);
};

void FontFaceProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kCursive:
    case cssom::KeywordValue::kFantasy:
    case cssom::KeywordValue::kMonospace:
    case cssom::KeywordValue::kSansSerif:
    case cssom::KeywordValue::kSerif:
      font_family_ = keyword->ToString();
      break;
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kAlternate:
    case cssom::KeywordValue::kAlternateReverse:
    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kBackwards:
    case cssom::KeywordValue::kBaseline:
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kBoth:
    case cssom::KeywordValue::kBottom:
    case cssom::KeywordValue::kBreakWord:
    case cssom::KeywordValue::kCenter:
    case cssom::KeywordValue::kClip:
    case cssom::KeywordValue::kContain:
    case cssom::KeywordValue::kCover:
    case cssom::KeywordValue::kEllipsis:
    case cssom::KeywordValue::kFixed:
    case cssom::KeywordValue::kForwards:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInfinite:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kLeft:
    case cssom::KeywordValue::kMiddle:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kNoRepeat:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kNoWrap:
    case cssom::KeywordValue::kPre:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kRepeat:
    case cssom::KeywordValue::kReverse:
    case cssom::KeywordValue::kRight:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kTop:
    case cssom::KeywordValue::kUppercase:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void FontFaceProvider::VisitPropertyList(
    cssom::PropertyListValue* property_list) {
  if (property_list->value().size() > 0) {
    property_list->value()[0]->Accept(this);
  }
}

void FontFaceProvider::VisitString(cssom::StringValue* string) {
  font_family_ = string->value();
}

// Check for a supported format. If no format hints are supplied, then the user
// agent should download the font resource.
//  http://www.w3.org/TR/css3-fonts/#descdef-src
void FontFaceProvider::VisitUrlSrc(cssom::UrlSrcValue* url_src) {
  if (url_src->format().empty() || url_src->format() == "truetype" ||
      url_src->format() == "opentype" || url_src->format() == "woff") {
    url_src->url()->Accept(this);
  }
}

// The URL may be relative, in which case it is resolved relative to the
// location of the style sheet containing the @font-face rule.
//   http://www.w3.org/TR/css3-fonts/#descdef-src
void FontFaceProvider::VisitURL(cssom::URLValue* url) {
  if (url->is_absolute()) {
    url_ = GURL(url->value());
  } else {
    url_ = url->Resolve(base_url_);
  }
}

// The src and font family are required for the @font-face rule to be valid.
//  http://www.w3.org/TR/css3-fonts/#descdef-font-family
//  http://www.w3.org/TR/css3-fonts/#descdef-src
bool FontFaceProvider::IsFontFaceValid() const {
  return !font_family_.empty() && url_.is_valid();
}

}  // namespace

//////////////////////////////////////////////////////////////////////////
// FontFaceUpdater
//////////////////////////////////////////////////////////////////////////

FontFaceUpdater::FontFaceUpdater(const GURL& document_base_url,
                                 FontCache* const cache)
    : document_base_url_(document_base_url),
      cache_(cache),
      font_face_map_(new FontCache::FontFaceMap()) {}

FontFaceUpdater::~FontFaceUpdater() {
  cache_->SetFontFaceMap(font_face_map_.Pass());
}

void FontFaceUpdater::ProcessCSSStyleSheet(
    const scoped_refptr<cssom::CSSStyleSheet>& style_sheet) {
  if (style_sheet->css_rules()) {
    style_sheet->css_rules()->Accept(this);
  }
}

void FontFaceUpdater::ProcessStyleSheetList(
    const scoped_refptr<cssom::StyleSheetList>& style_sheet_list) {
  for (unsigned int style_sheet_index = 0;
       style_sheet_index < style_sheet_list->length(); ++style_sheet_index) {
    scoped_refptr<cssom::CSSStyleSheet> style_sheet =
        style_sheet_list->Item(style_sheet_index)->AsCSSStyleSheet();
    if (style_sheet) {
      ProcessCSSStyleSheet(style_sheet);
    }
  }
}

void FontFaceUpdater::VisitCSSFontFaceRule(
    cssom::CSSFontFaceRule* css_font_face_rule) {
  // The URL may be relative, in which case it is resolved relative to the
  // location of the style sheet containing the @font-face rule.
  //   http://www.w3.org/TR/css3-fonts/#descdef-src
  // In the case where the style sheet URL is empty, treat the document base URL
  // as the style sheet's URL. While the spec is not clear on the expected
  // behavior for this case, this mimics WebKit's functionality.
  const GURL& style_sheet_url =
      css_font_face_rule->parent_style_sheet()->LocationUrl();
  const GURL& base_url =
      !style_sheet_url.is_empty() ? style_sheet_url : document_base_url_;

  FontFaceProvider font_face_provider(base_url);

  if (css_font_face_rule->data()->family()) {
    css_font_face_rule->data()->family()->Accept(&font_face_provider);
  }
  if (css_font_face_rule->data()->src()) {
    css_font_face_rule->data()->src()->Accept(&font_face_provider);
  }

  if (font_face_provider.IsFontFaceValid()) {
    (*font_face_map_)[font_face_provider.font_family()] =
        font_face_provider.url();
  }
}

}  // namespace dom
}  // namespace cobalt
