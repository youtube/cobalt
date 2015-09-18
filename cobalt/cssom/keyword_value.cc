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

#include "cobalt/cssom/keyword_value.h"

#include "base/lazy_instance.h"
#include "cobalt/cssom/keyword_names.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

struct KeywordValue::NonTrivialStaticFields {
  NonTrivialStaticFields()
      : absolute_value(new KeywordValue(KeywordValue::kAbsolute)),
        auto_value(new KeywordValue(KeywordValue::kAuto)),
        baseline_value(new KeywordValue(KeywordValue::kBaseline)),
        block_value(new KeywordValue(KeywordValue::kBlock)),
        break_word_value(new KeywordValue(KeywordValue::kBreakWord)),
        center_value(new KeywordValue(KeywordValue::kCenter)),
        clip_value(new KeywordValue(KeywordValue::kClip)),
        contain_value(new KeywordValue(KeywordValue::kContain)),
        cover_value(new KeywordValue(KeywordValue::kCover)),
        cursive_value(new KeywordValue(KeywordValue::kCursive)),
        ellipsis_value(new KeywordValue(KeywordValue::kEllipsis)),
        fantasy_value(new KeywordValue(KeywordValue::kFantasy)),
        hidden_value(new KeywordValue(KeywordValue::kHidden)),
        inherit_value(new KeywordValue(KeywordValue::kInherit)),
        initial_value(new KeywordValue(KeywordValue::kInitial)),
        inline_block_value(new KeywordValue(KeywordValue::kInlineBlock)),
        inline_value(new KeywordValue(KeywordValue::kInline)),
        left_value(new KeywordValue(KeywordValue::kLeft)),
        middle_value(new KeywordValue(KeywordValue::kMiddle)),
        monospace_value(new KeywordValue(KeywordValue::kMonospace)),
        none_value(new KeywordValue(KeywordValue::kNone)),
        no_repeat_value(new KeywordValue(KeywordValue::kNoRepeat)),
        normal_value(new KeywordValue(KeywordValue::kNormal)),
        nowrap_value(new KeywordValue(KeywordValue::kNoWrap)),
        pre_value(new KeywordValue(KeywordValue::kPre)),
        relative_value(new KeywordValue(KeywordValue::kRelative)),
        repeat_value(new KeywordValue(KeywordValue::kRepeat)),
        right_value(new KeywordValue(KeywordValue::kRight)),
        sans_serif_value(new KeywordValue(KeywordValue::kSansSerif)),
        serif_value(new KeywordValue(KeywordValue::kSerif)),
        static_value(new KeywordValue(KeywordValue::kStatic)),
        top_value(new KeywordValue(KeywordValue::kTop)),
        uppercase_value(new KeywordValue(KeywordValue::kUppercase)),
        visible_value(new KeywordValue(KeywordValue::kVisible)) {}

  const scoped_refptr<KeywordValue> absolute_value;
  const scoped_refptr<KeywordValue> auto_value;
  const scoped_refptr<KeywordValue> baseline_value;
  const scoped_refptr<KeywordValue> block_value;
  const scoped_refptr<KeywordValue> break_word_value;
  const scoped_refptr<KeywordValue> center_value;
  const scoped_refptr<KeywordValue> clip_value;
  const scoped_refptr<KeywordValue> contain_value;
  const scoped_refptr<KeywordValue> cover_value;
  const scoped_refptr<KeywordValue> cursive_value;
  const scoped_refptr<KeywordValue> ellipsis_value;
  const scoped_refptr<KeywordValue> fantasy_value;
  const scoped_refptr<KeywordValue> hidden_value;
  const scoped_refptr<KeywordValue> inherit_value;
  const scoped_refptr<KeywordValue> initial_value;
  const scoped_refptr<KeywordValue> inline_block_value;
  const scoped_refptr<KeywordValue> inline_value;
  const scoped_refptr<KeywordValue> left_value;
  const scoped_refptr<KeywordValue> middle_value;
  const scoped_refptr<KeywordValue> monospace_value;
  const scoped_refptr<KeywordValue> none_value;
  const scoped_refptr<KeywordValue> no_repeat_value;
  const scoped_refptr<KeywordValue> normal_value;
  const scoped_refptr<KeywordValue> nowrap_value;
  const scoped_refptr<KeywordValue> pre_value;
  const scoped_refptr<KeywordValue> relative_value;
  const scoped_refptr<KeywordValue> repeat_value;
  const scoped_refptr<KeywordValue> right_value;
  const scoped_refptr<KeywordValue> sans_serif_value;
  const scoped_refptr<KeywordValue> serif_value;
  const scoped_refptr<KeywordValue> static_value;
  const scoped_refptr<KeywordValue> top_value;
  const scoped_refptr<KeywordValue> uppercase_value;
  const scoped_refptr<KeywordValue> visible_value;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

namespace {

base::LazyInstance<KeywordValue::NonTrivialStaticFields>
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace


const scoped_refptr<KeywordValue>& KeywordValue::GetAbsolute() {
  return non_trivial_static_fields.Get().absolute_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetAuto() {
  return non_trivial_static_fields.Get().auto_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetBaseline() {
  return non_trivial_static_fields.Get().baseline_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetBlock() {
  return non_trivial_static_fields.Get().block_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetBreakWord() {
  return non_trivial_static_fields.Get().break_word_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetCenter() {
  return non_trivial_static_fields.Get().center_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetClip() {
  return non_trivial_static_fields.Get().clip_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetContain() {
  return non_trivial_static_fields.Get().contain_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetCover() {
  return non_trivial_static_fields.Get().cover_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetCursive() {
  return non_trivial_static_fields.Get().cursive_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetEllipsis() {
  return non_trivial_static_fields.Get().ellipsis_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetFantasy() {
  return non_trivial_static_fields.Get().fantasy_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetHidden() {
  return non_trivial_static_fields.Get().hidden_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInherit() {
  return non_trivial_static_fields.Get().inherit_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInitial() {
  return non_trivial_static_fields.Get().initial_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInline() {
  return non_trivial_static_fields.Get().inline_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInlineBlock() {
  return non_trivial_static_fields.Get().inline_block_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetLeft() {
  return non_trivial_static_fields.Get().left_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetMiddle() {
  return non_trivial_static_fields.Get().middle_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetMonospace() {
  return non_trivial_static_fields.Get().monospace_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetNone() {
  return non_trivial_static_fields.Get().none_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetNoRepeat() {
  return non_trivial_static_fields.Get().no_repeat_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetNormal() {
  return non_trivial_static_fields.Get().normal_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetNoWrap() {
  return non_trivial_static_fields.Get().nowrap_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetPre() {
  return non_trivial_static_fields.Get().pre_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetRelative() {
  return non_trivial_static_fields.Get().relative_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetRepeat() {
  return non_trivial_static_fields.Get().repeat_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetRight() {
  return non_trivial_static_fields.Get().right_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetSansSerif() {
  return non_trivial_static_fields.Get().sans_serif_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetSerif() {
  return non_trivial_static_fields.Get().serif_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetStatic() {
  return non_trivial_static_fields.Get().static_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetTop() {
  return non_trivial_static_fields.Get().top_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetUppercase() {
  return non_trivial_static_fields.Get().uppercase_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetVisible() {
  return non_trivial_static_fields.Get().visible_value;
}

void KeywordValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitKeyword(this);
}

std::string KeywordValue::ToString() const {
  switch (value_) {
    case kAbsolute:
      return kAbsoluteKeywordName;
    case kAuto:
      return kAutoKeywordName;
    case kBaseline:
      return kBaselineKeywordName;
    case kBlock:
      return kBlockKeywordName;
    case kBreakWord:
      return kBreakWordKeywordName;
    case kCenter:
      return kCenterKeywordName;
    case kClip:
      return kClipKeywordName;
    case kContain:
      return kContainKeywordName;
    case kCover:
      return kCoverKeywordName;
    case kCursive:
      return kCursiveKeywordName;
    case kEllipsis:
      return kEllipsisKeywordName;
    case kFantasy:
      return kFantasyKeywordName;
    case kHidden:
      return kHiddenKeywordName;
    case kInherit:
      return kInheritKeywordName;
    case kInitial:
      return kInitialKeywordName;
    case kInline:
      return kInlineKeywordName;
    case kInlineBlock:
      return kInlineBlockKeywordName;
    case kLeft:
      return kLeftKeywordName;
    case kMiddle:
      return kMiddleKeywordName;
    case kMonospace:
      return kMonospaceKeywordName;
    case kNone:
      return kNoneKeywordName;
    case kNoRepeat:
      return kNoRepeatKeywordName;
    case kNormal:
      return kNormalKeywordName;
    case kNoWrap:
      return kNoWrapKeywordName;
    case kPre:
      return kPreKeywordName;
    case kRelative:
      return kRelativeKeywordName;
    case kRepeat:
      return kRepeatKeywordName;
    case kRight:
      return kRightKeywordName;
    case kSansSerif:
      return kSansSerifKeywordName;
    case kSerif:
      return kSerifKeywordName;
    case kStatic:
      return kStaticKeywordName;
    case kTop:
      return kTopKeywordName;
    case kUppercase:
      return kUppercaseKeywordName;
    case kVisible:
      return kVisibleKeywordName;
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace cssom
}  // namespace cobalt
