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

#include "cobalt/dom/font_face_updater.h"

#include <string>

#include "cobalt/cssom/css_font_face_rule.h"
#include "cobalt/cssom/css_media_rule.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/local_src_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/cssom/url_src_value.h"
#include "cobalt/cssom/url_value.h"

namespace cobalt {
namespace dom {

namespace {

////////////////////////////////////////////////////////////////////////////////
// FontFaceProvider
////////////////////////////////////////////////////////////////////////////////

// Visit a single font face rule, generating a FontFaceStyleSet::Entry from its
// values and verifying that it is valid.
// TODO: Handle unicode ranges.
class FontFaceProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit FontFaceProvider(const GURL& base_url) : base_url_(base_url) {}

  void VisitFontStyle(cssom::FontStyleValue* font_style) override;
  void VisitFontWeight(cssom::FontWeightValue* font_weight) override;
  void VisitKeyword(cssom::KeywordValue* keyword) override;
  void VisitLocalSrc(cssom::LocalSrcValue* local_src) override;
  void VisitPropertyList(cssom::PropertyListValue* property_list) override;
  void VisitString(cssom::StringValue* percentage) override;
  void VisitUnicodeRange(cssom::UnicodeRangeValue* unicode_range) override;
  void VisitUrlSrc(cssom::UrlSrcValue* url_src) override;
  void VisitURL(cssom::URLValue* url) override;

  bool IsFontFaceValid() const;

  const std::string& font_family() const { return font_family_; }
  const FontFaceStyleSet::Entry& style_set_entry() const {
    return style_set_entry_;
  }

 private:
  const GURL& base_url_;

  std::string font_family_;
  FontFaceStyleSet::Entry style_set_entry_;

  DISALLOW_COPY_AND_ASSIGN(FontFaceProvider);
};

void FontFaceProvider::VisitFontStyle(cssom::FontStyleValue* font_style) {
  style_set_entry_.style.slant =
      font_style->value() == cssom::FontStyleValue::kItalic
          ? render_tree::FontStyle::kItalicSlant
          : render_tree::FontStyle::kUprightSlant;
}

void FontFaceProvider::VisitFontWeight(cssom::FontWeightValue* font_weight) {
  switch (font_weight->value()) {
    case cssom::FontWeightValue::kThinAka100:
      style_set_entry_.style.weight = render_tree::FontStyle::kThinWeight;
      break;
    case cssom::FontWeightValue::kExtraLightAka200:
      style_set_entry_.style.weight = render_tree::FontStyle::kExtraLightWeight;
      break;
    case cssom::FontWeightValue::kLightAka300:
      style_set_entry_.style.weight = render_tree::FontStyle::kLightWeight;
      break;
    case cssom::FontWeightValue::kNormalAka400:
      style_set_entry_.style.weight = render_tree::FontStyle::kNormalWeight;
      break;
    case cssom::FontWeightValue::kMediumAka500:
      style_set_entry_.style.weight = render_tree::FontStyle::kMediumWeight;
      break;
    case cssom::FontWeightValue::kSemiBoldAka600:
      style_set_entry_.style.weight = render_tree::FontStyle::kSemiBoldWeight;
      break;
    case cssom::FontWeightValue::kBoldAka700:
      style_set_entry_.style.weight = render_tree::FontStyle::kBoldWeight;
      break;
    case cssom::FontWeightValue::kExtraBoldAka800:
      style_set_entry_.style.weight = render_tree::FontStyle::kExtraBoldWeight;
      break;
    case cssom::FontWeightValue::kBlackAka900:
      style_set_entry_.style.weight = render_tree::FontStyle::kBlackWeight;
      break;
  }
}

void FontFaceProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kCursive:
    case cssom::KeywordValue::kFantasy:
    case cssom::KeywordValue::kMonospace:
    case cssom::KeywordValue::kSansSerif:
    case cssom::KeywordValue::kSerif:
      font_family_ = keyword->ToString();
      break;
    // Inherit and Initial are valid font-family values. However, they are
    // meaningless in the context of an @font-face rule, as a font-family will
    // never attempt a lookup with that value. Clear the font-family name if
    // they are encountered so that the font face rule won't be created.
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
      font_family_.clear();
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
    case cssom::KeywordValue::kCurrentColor:
    case cssom::KeywordValue::kEllipsis:
    case cssom::KeywordValue::kEnd:
    case cssom::KeywordValue::kEquirectangular:
    case cssom::KeywordValue::kFixed:
    case cssom::KeywordValue::kForwards:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInfinite:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kLeft:
    case cssom::KeywordValue::kLineThrough:
    case cssom::KeywordValue::kMiddle:
    case cssom::KeywordValue::kMonoscopic:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kNoRepeat:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kNoWrap:
    case cssom::KeywordValue::kPre:
    case cssom::KeywordValue::kPreLine:
    case cssom::KeywordValue::kPreWrap:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kRepeat:
    case cssom::KeywordValue::kReverse:
    case cssom::KeywordValue::kRight:
    case cssom::KeywordValue::kSolid:
    case cssom::KeywordValue::kStart:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kStereoscopicLeftRight:
    case cssom::KeywordValue::kStereoscopicTopBottom:
    case cssom::KeywordValue::kTop:
    case cssom::KeywordValue::kUppercase:
    case cssom::KeywordValue::kVisible:
      NOTREACHED();
  }
}

void FontFaceProvider::VisitLocalSrc(cssom::LocalSrcValue* local_src) {
  style_set_entry_.sources.push_back(FontFaceSource(local_src->value()));
}

void FontFaceProvider::VisitPropertyList(
    cssom::PropertyListValue* property_list) {
  for (size_t i = 0; i < property_list->value().size(); ++i) {
    property_list->value()[i]->Accept(this);
  }
}

void FontFaceProvider::VisitString(cssom::StringValue* string) {
  font_family_ = string->value();
}

void FontFaceProvider::VisitUnicodeRange(
    cssom::UnicodeRangeValue* /*unicode_range*/) {
  NOTIMPLEMENTED()
      << "FontFaceProvider::UnicodeRange support not implemented yet.";
}

// Check for a supported format. If no format hints are supplied, then the user
// agent should download the font resource.
//  https://www.w3.org/TR/css3-fonts/#descdef-src
void FontFaceProvider::VisitUrlSrc(cssom::UrlSrcValue* url_src) {
  if (url_src->format().empty() || url_src->format() == "truetype" ||
      url_src->format() == "opentype" || url_src->format() == "woff") {
    url_src->url()->Accept(this);
  }
}

// The URL may be relative, in which case it is resolved relative to the
// location of the style sheet containing the @font-face rule.
//   https://www.w3.org/TR/css3-fonts/#descdef-src
void FontFaceProvider::VisitURL(cssom::URLValue* url) {
  GURL gurl = url->is_absolute() ? GURL(url->value()) : url->Resolve(base_url_);
  if (gurl.is_valid()) {
    style_set_entry_.sources.push_back(FontFaceSource(gurl));
  }
}

// The font family and src are required for the @font-face rule to be valid.
//  https://www.w3.org/TR/css3-fonts/#descdef-font-family
//  https://www.w3.org/TR/css3-fonts/#descdef-src
bool FontFaceProvider::IsFontFaceValid() const {
  return !font_family_.empty() && !style_set_entry_.sources.empty();
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
  if (style_sheet && style_sheet->css_rules_same_origin()) {
    style_sheet->css_rules_same_origin()->Accept(this);
  }
}

void FontFaceUpdater::ProcessStyleSheetList(
    const scoped_refptr<cssom::StyleSheetList>& style_sheet_list) {
  for (unsigned int style_sheet_index = 0;
       style_sheet_index < style_sheet_list->length(); ++style_sheet_index) {
    scoped_refptr<cssom::CSSStyleSheet> style_sheet =
        style_sheet_list->Item(style_sheet_index)->AsCSSStyleSheet();
    // ProcessCSSStyleSheet handles a NULL CSSStyleSheet.
    ProcessCSSStyleSheet(style_sheet);
  }
}

void FontFaceUpdater::VisitCSSFontFaceRule(
    cssom::CSSFontFaceRule* css_font_face_rule) {
  // The URL may be relative, in which case it is resolved relative to the
  // location of the style sheet containing the @font-face rule.
  //   https://www.w3.org/TR/css3-fonts/#descdef-src
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
  if (css_font_face_rule->data()->style()) {
    css_font_face_rule->data()->style()->Accept(&font_face_provider);
  }
  if (css_font_face_rule->data()->weight()) {
    css_font_face_rule->data()->weight()->Accept(&font_face_provider);
  }

  if (font_face_provider.IsFontFaceValid()) {
    (*font_face_map_)[font_face_provider.font_family()].AddEntry(
        font_face_provider.style_set_entry());
  }
}

}  // namespace dom
}  // namespace cobalt
