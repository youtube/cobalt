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

#include "cobalt/cssom/keyword_value.h"

#include "base/lazy_instance.h"
#include "cobalt/cssom/keyword_names.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

struct KeywordValue::NonTrivialStaticFields {
  NonTrivialStaticFields()
      : absolute_value(new KeywordValue(KeywordValue::kAbsolute)),
        alternate_value(new KeywordValue(KeywordValue::kAlternate)),
        alternate_reverse_value(
            new KeywordValue(KeywordValue::kAlternateReverse)),
        auto_value(new KeywordValue(KeywordValue::kAuto)),
        backwards_value(new KeywordValue(KeywordValue::kBackwards)),
        baseline_value(new KeywordValue(KeywordValue::kBaseline)),
        block_value(new KeywordValue(KeywordValue::kBlock)),
        both_value(new KeywordValue(KeywordValue::kBoth)),
        bottom_value(new KeywordValue(KeywordValue::kBottom)),
        break_word_value(new KeywordValue(KeywordValue::kBreakWord)),
        center_value(new KeywordValue(KeywordValue::kCenter)),
        clip_value(new KeywordValue(KeywordValue::kClip)),
        collapse_value(new KeywordValue(KeywordValue::kCollapse)),
        column_value(new KeywordValue(KeywordValue::kColumn)),
        column_reverse_value(new KeywordValue(KeywordValue::kColumnReverse)),
        contain_value(new KeywordValue(KeywordValue::kContain)),
        content_value(new KeywordValue(KeywordValue::kContent)),
        cover_value(new KeywordValue(KeywordValue::kCover)),
        current_color_value(new KeywordValue(KeywordValue::kCurrentColor)),
        cursive_value(new KeywordValue(KeywordValue::kCursive)),
        ellipsis_value(new KeywordValue(KeywordValue::kEllipsis)),
        end_value(new KeywordValue(KeywordValue::kEnd)),
        equirectangular_value(new KeywordValue(KeywordValue::kEquirectangular)),
        fantasy_value(new KeywordValue(KeywordValue::kFantasy)),
        fixed_value(new KeywordValue(KeywordValue::kFixed)),
        flex_value(new KeywordValue(KeywordValue::kFlex)),
        flex_end_value(new KeywordValue(KeywordValue::kFlexEnd)),
        flex_start_value(new KeywordValue(KeywordValue::kFlexStart)),
        forwards_value(new KeywordValue(KeywordValue::kForwards)),
        hidden_value(new KeywordValue(KeywordValue::kHidden)),
        infinite_value(new KeywordValue(KeywordValue::kInfinite)),
        inherit_value(new KeywordValue(KeywordValue::kInherit)),
        initial_value(new KeywordValue(KeywordValue::kInitial)),
        inline_block_value(new KeywordValue(KeywordValue::kInlineBlock)),
        inline_flex_value(new KeywordValue(KeywordValue::kInlineFlex)),
        inline_value(new KeywordValue(KeywordValue::kInline)),
        left_value(new KeywordValue(KeywordValue::kLeft)),
        line_through_value(new KeywordValue(KeywordValue::kLineThrough)),
        middle_value(new KeywordValue(KeywordValue::kMiddle)),
        monoscopic_value(new KeywordValue(KeywordValue::kMonoscopic)),
        monospace_value(new KeywordValue(KeywordValue::kMonospace)),
        none_value(new KeywordValue(KeywordValue::kNone)),
        no_repeat_value(new KeywordValue(KeywordValue::kNoRepeat)),
        normal_value(new KeywordValue(KeywordValue::kNormal)),
        nowrap_value(new KeywordValue(KeywordValue::kNowrap)),
        pre_value(new KeywordValue(KeywordValue::kPre)),
        pre_line_value(new KeywordValue(KeywordValue::kPreLine)),
        pre_wrap_value(new KeywordValue(KeywordValue::kPreWrap)),
        relative_value(new KeywordValue(KeywordValue::kRelative)),
        repeat_value(new KeywordValue(KeywordValue::kRepeat)),
        reverse_value(new KeywordValue(KeywordValue::kReverse)),
        right_value(new KeywordValue(KeywordValue::kRight)),
        row_value(new KeywordValue(KeywordValue::kRow)),
        row_reverse_value(new KeywordValue(KeywordValue::kRowReverse)),
        sans_serif_value(new KeywordValue(KeywordValue::kSansSerif)),
        scroll_value(new KeywordValue(KeywordValue::kScroll)),
        serif_value(new KeywordValue(KeywordValue::kSerif)),
        solid_value(new KeywordValue(KeywordValue::kSolid)),
        space_around_value(new KeywordValue(KeywordValue::kSpaceAround)),
        space_between_value(new KeywordValue(KeywordValue::kSpaceBetween)),
        start_value(new KeywordValue(KeywordValue::kStart)),
        static_value(new KeywordValue(KeywordValue::kStatic)),
        stereoscopic_left_right_value(
            new KeywordValue(KeywordValue::kStereoscopicLeftRight)),
        stereoscopic_top_bottom_value(
            new KeywordValue(KeywordValue::kStereoscopicTopBottom)),
        stretch_value(new KeywordValue(KeywordValue::kStretch)),
        top_value(new KeywordValue(KeywordValue::kTop)),
        uppercase_value(new KeywordValue(KeywordValue::kUppercase)),
        visible_value(new KeywordValue(KeywordValue::kVisible)),
        wrap_value(new KeywordValue(KeywordValue::kWrap)),
        wrap_reverse_value(new KeywordValue(KeywordValue::kWrapReverse)) {}

  const scoped_refptr<KeywordValue> absolute_value;
  const scoped_refptr<KeywordValue> alternate_value;
  const scoped_refptr<KeywordValue> alternate_reverse_value;
  const scoped_refptr<KeywordValue> auto_value;
  const scoped_refptr<KeywordValue> backwards_value;
  const scoped_refptr<KeywordValue> baseline_value;
  const scoped_refptr<KeywordValue> block_value;
  const scoped_refptr<KeywordValue> both_value;
  const scoped_refptr<KeywordValue> bottom_value;
  const scoped_refptr<KeywordValue> break_word_value;
  const scoped_refptr<KeywordValue> center_value;
  const scoped_refptr<KeywordValue> clip_value;
  const scoped_refptr<KeywordValue> collapse_value;
  const scoped_refptr<KeywordValue> column_value;
  const scoped_refptr<KeywordValue> column_reverse_value;
  const scoped_refptr<KeywordValue> contain_value;
  const scoped_refptr<KeywordValue> content_value;
  const scoped_refptr<KeywordValue> cover_value;
  const scoped_refptr<KeywordValue> current_color_value;
  const scoped_refptr<KeywordValue> cursive_value;
  const scoped_refptr<KeywordValue> ellipsis_value;
  const scoped_refptr<KeywordValue> end_value;
  const scoped_refptr<KeywordValue> equirectangular_value;
  const scoped_refptr<KeywordValue> fantasy_value;
  const scoped_refptr<KeywordValue> fixed_value;
  const scoped_refptr<KeywordValue> flex_value;
  const scoped_refptr<KeywordValue> flex_end_value;
  const scoped_refptr<KeywordValue> flex_start_value;
  const scoped_refptr<KeywordValue> forwards_value;
  const scoped_refptr<KeywordValue> hidden_value;
  const scoped_refptr<KeywordValue> infinite_value;
  const scoped_refptr<KeywordValue> inherit_value;
  const scoped_refptr<KeywordValue> initial_value;
  const scoped_refptr<KeywordValue> inline_block_value;
  const scoped_refptr<KeywordValue> inline_flex_value;
  const scoped_refptr<KeywordValue> inline_value;
  const scoped_refptr<KeywordValue> left_value;
  const scoped_refptr<KeywordValue> line_through_value;
  const scoped_refptr<KeywordValue> middle_value;
  const scoped_refptr<KeywordValue> monoscopic_value;
  const scoped_refptr<KeywordValue> monospace_value;
  const scoped_refptr<KeywordValue> none_value;
  const scoped_refptr<KeywordValue> no_repeat_value;
  const scoped_refptr<KeywordValue> normal_value;
  const scoped_refptr<KeywordValue> nowrap_value;
  const scoped_refptr<KeywordValue> pre_value;
  const scoped_refptr<KeywordValue> pre_line_value;
  const scoped_refptr<KeywordValue> pre_wrap_value;
  const scoped_refptr<KeywordValue> relative_value;
  const scoped_refptr<KeywordValue> repeat_value;
  const scoped_refptr<KeywordValue> reverse_value;
  const scoped_refptr<KeywordValue> right_value;
  const scoped_refptr<KeywordValue> row_value;
  const scoped_refptr<KeywordValue> row_reverse_value;
  const scoped_refptr<KeywordValue> sans_serif_value;
  const scoped_refptr<KeywordValue> scroll_value;
  const scoped_refptr<KeywordValue> serif_value;
  const scoped_refptr<KeywordValue> solid_value;
  const scoped_refptr<KeywordValue> space_around_value;
  const scoped_refptr<KeywordValue> space_between_value;
  const scoped_refptr<KeywordValue> start_value;
  const scoped_refptr<KeywordValue> static_value;
  const scoped_refptr<KeywordValue> stereoscopic_left_right_value;
  const scoped_refptr<KeywordValue> stereoscopic_top_bottom_value;
  const scoped_refptr<KeywordValue> stretch_value;
  const scoped_refptr<KeywordValue> top_value;
  const scoped_refptr<KeywordValue> uppercase_value;
  const scoped_refptr<KeywordValue> visible_value;
  const scoped_refptr<KeywordValue> wrap_value;
  const scoped_refptr<KeywordValue> wrap_reverse_value;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

namespace {

base::LazyInstance<KeywordValue::NonTrivialStaticFields>::DestructorAtExit
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetAbsolute() {
  return non_trivial_static_fields.Get().absolute_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetAlternate() {
  return non_trivial_static_fields.Get().alternate_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetAlternateReverse() {
  return non_trivial_static_fields.Get().alternate_reverse_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetAuto() {
  return non_trivial_static_fields.Get().auto_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetBackwards() {
  return non_trivial_static_fields.Get().backwards_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetBaseline() {
  return non_trivial_static_fields.Get().baseline_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetBlock() {
  return non_trivial_static_fields.Get().block_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetBoth() {
  return non_trivial_static_fields.Get().both_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetBottom() {
  return non_trivial_static_fields.Get().bottom_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetBreakWord() {
  return non_trivial_static_fields.Get().break_word_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetCenter() {
  return non_trivial_static_fields.Get().center_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetClip() {
  return non_trivial_static_fields.Get().clip_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetCollapse() {
  return non_trivial_static_fields.Get().collapse_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetColumn() {
  return non_trivial_static_fields.Get().column_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetColumnReverse() {
  return non_trivial_static_fields.Get().column_reverse_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetContain() {
  return non_trivial_static_fields.Get().contain_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetContent() {
  return non_trivial_static_fields.Get().content_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetCover() {
  return non_trivial_static_fields.Get().cover_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetCurrentColor() {
  return non_trivial_static_fields.Get().current_color_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetCursive() {
  return non_trivial_static_fields.Get().cursive_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetEllipsis() {
  return non_trivial_static_fields.Get().ellipsis_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetEnd() {
  return non_trivial_static_fields.Get().end_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetEquirectangular() {
  return non_trivial_static_fields.Get().equirectangular_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetFantasy() {
  return non_trivial_static_fields.Get().fantasy_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetFixed() {
  return non_trivial_static_fields.Get().fixed_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetFlex() {
  return non_trivial_static_fields.Get().flex_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetFlexEnd() {
  return non_trivial_static_fields.Get().flex_end_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetFlexStart() {
  return non_trivial_static_fields.Get().flex_start_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetForwards() {
  return non_trivial_static_fields.Get().forwards_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetHidden() {
  return non_trivial_static_fields.Get().hidden_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetInfinite() {
  return non_trivial_static_fields.Get().infinite_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetInherit() {
  return non_trivial_static_fields.Get().inherit_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetInitial() {
  return non_trivial_static_fields.Get().initial_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetInline() {
  return non_trivial_static_fields.Get().inline_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetInlineBlock() {
  return non_trivial_static_fields.Get().inline_block_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetInlineFlex() {
  return non_trivial_static_fields.Get().inline_flex_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetLeft() {
  return non_trivial_static_fields.Get().left_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetLineThrough() {
  return non_trivial_static_fields.Get().line_through_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetMiddle() {
  return non_trivial_static_fields.Get().middle_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetMonoscopic() {
  return non_trivial_static_fields.Get().monoscopic_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetMonospace() {
  return non_trivial_static_fields.Get().monospace_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetNone() {
  return non_trivial_static_fields.Get().none_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetNoRepeat() {
  return non_trivial_static_fields.Get().no_repeat_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetNormal() {
  return non_trivial_static_fields.Get().normal_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetNowrap() {
  return non_trivial_static_fields.Get().nowrap_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetPre() {
  return non_trivial_static_fields.Get().pre_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetPreLine() {
  return non_trivial_static_fields.Get().pre_line_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetPreWrap() {
  return non_trivial_static_fields.Get().pre_wrap_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetRelative() {
  return non_trivial_static_fields.Get().relative_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetRepeat() {
  return non_trivial_static_fields.Get().repeat_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetReverse() {
  return non_trivial_static_fields.Get().reverse_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetRight() {
  return non_trivial_static_fields.Get().right_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetRow() {
  return non_trivial_static_fields.Get().row_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetRowReverse() {
  return non_trivial_static_fields.Get().row_reverse_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetSansSerif() {
  return non_trivial_static_fields.Get().sans_serif_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetScroll() {
  return non_trivial_static_fields.Get().scroll_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetSerif() {
  return non_trivial_static_fields.Get().serif_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetSolid() {
  return non_trivial_static_fields.Get().solid_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetSpaceAround() {
  return non_trivial_static_fields.Get().space_around_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetSpaceBetween() {
  return non_trivial_static_fields.Get().space_between_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetStart() {
  return non_trivial_static_fields.Get().start_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetStatic() {
  return non_trivial_static_fields.Get().static_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetStereoscopicLeftRight() {
  return non_trivial_static_fields.Get().stereoscopic_left_right_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetStereoscopicTopBottom() {
  return non_trivial_static_fields.Get().stereoscopic_top_bottom_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetStretch() {
  return non_trivial_static_fields.Get().stretch_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetTop() {
  return non_trivial_static_fields.Get().top_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetUppercase() {
  return non_trivial_static_fields.Get().uppercase_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetVisible() {
  return non_trivial_static_fields.Get().visible_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetWrap() {
  return non_trivial_static_fields.Get().wrap_value;
}

// static
const scoped_refptr<KeywordValue>& KeywordValue::GetWrapReverse() {
  return non_trivial_static_fields.Get().wrap_reverse_value;
}

void KeywordValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitKeyword(this);
}

std::string KeywordValue::ToString() const {
  switch (value_) {
    case kAbsolute:
      return kAbsoluteKeywordName;
    case kAlternate:
      return kAlternateKeywordName;
    case kAlternateReverse:
      return kAlternateReverseKeywordName;
    case kAuto:
      return kAutoKeywordName;
    case kBackwards:
      return kBackwardsKeywordName;
    case kBaseline:
      return kBaselineKeywordName;
    case kBlock:
      return kBlockKeywordName;
    case kBoth:
      return kBothKeywordName;
    case kBottom:
      return kBottomKeywordName;
    case kBreakWord:
      return kBreakWordKeywordName;
    case kCenter:
      return kCenterKeywordName;
    case kClip:
      return kClipKeywordName;
    case kCollapse:
      return kCollapseKeywordName;
    case kColumn:
      return kColumnKeywordName;
    case kColumnReverse:
      return kColumnReverseKeywordName;
    case kContain:
      return kContainKeywordName;
    case kContent:
      return kContentKeywordName;
    case kCover:
      return kCoverKeywordName;
    case kCurrentColor:
      return kCurrentColorKeywordName;
    case kCursive:
      return kCursiveKeywordName;
    case kEllipsis:
      return kEllipsisKeywordName;
    case kEnd:
      return kEndKeywordName;
    case kEquirectangular:
      return kEquirectangularKeywordName;
    case kFantasy:
      return kFantasyKeywordName;
    case kForwards:
      return kForwardsKeywordName;
    case kFixed:
      return kFixedKeywordName;
    case kFlex:
      return kFlexKeywordName;
    case kFlexEnd:
      return kFlexEndKeywordName;
    case kFlexStart:
      return kFlexStartKeywordName;
    case kHidden:
      return kHiddenKeywordName;
    case kInfinite:
      return kInfiniteKeywordName;
    case kInherit:
      return kInheritKeywordName;
    case kInitial:
      return kInitialKeywordName;
    case kInline:
      return kInlineKeywordName;
    case kInlineBlock:
      return kInlineBlockKeywordName;
    case kInlineFlex:
      return kInlineFlexKeywordName;
    case kLeft:
      return kLeftKeywordName;
    case kLineThrough:
      return kLineThroughKeywordName;
    case kMiddle:
      return kMiddleKeywordName;
    case kMonospace:
      return kMonospaceKeywordName;
    case kMonoscopic:
      return kMonoscopicKeywordName;
    case kNone:
      return kNoneKeywordName;
    case kNoRepeat:
      return kNoRepeatKeywordName;
    case kNormal:
      return kNormalKeywordName;
    case kNowrap:
      return kNowrapKeywordName;
    case kPre:
      return kPreKeywordName;
    case kPreLine:
      return kPreLineKeywordName;
    case kPreWrap:
      return kPreWrapKeywordName;
    case kRelative:
      return kRelativeKeywordName;
    case kRepeat:
      return kRepeatKeywordName;
    case kReverse:
      return kReverseKeywordName;
    case kRight:
      return kRightKeywordName;
    case kRow:
      return kRowKeywordName;
    case kRowReverse:
      return kRowReverseKeywordName;
    case kScroll:
      return kScrollKeywordName;
    case kSansSerif:
      return kSansSerifKeywordName;
    case kSerif:
      return kSerifKeywordName;
    case kSolid:
      return kSolidKeywordName;
    case kSpaceAround:
      return kSpaceAroundKeywordName;
    case kSpaceBetween:
      return kSpaceBetweenKeywordName;
    case kStart:
      return kStartKeywordName;
    case kStatic:
      return kStaticKeywordName;
    case kStereoscopicLeftRight:
      return kStereoscopicLeftRightKeywordName;
    case kStereoscopicTopBottom:
      return kStereoscopicTopBottomKeywordName;
    case kStretch:
      return kStretchKeywordName;
    case kTop:
      return kTopKeywordName;
    case kUppercase:
      return kUppercaseKeywordName;
    case kVisible:
      return kVisibleKeywordName;
    case kWrap:
      return kWrapKeywordName;
    case kWrapReverse:
      return kWrapReverseKeywordName;
  }
  NOTREACHED();
  return "";
}

}  // namespace cssom
}  // namespace cobalt
