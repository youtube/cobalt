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

#include "cobalt/cssom/font_weight_value.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_checker.h"
#include "cobalt/cssom/keyword_names.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

struct FontWeightValue::NonTrivialStaticFields {
  NonTrivialStaticFields()
      : thin_aka_100(new FontWeightValue(FontWeightValue::kThinAka100)),
        extra_light_aka_200(
            new FontWeightValue(FontWeightValue::kExtraLightAka200)),
        light_aka_300(new FontWeightValue(FontWeightValue::kLightAka300)),
        normal_aka_400(new FontWeightValue(FontWeightValue::kNormalAka400)),
        medium_aka_500(new FontWeightValue(FontWeightValue::kMediumAka500)),
        semi_bold_aka_600(
            new FontWeightValue(FontWeightValue::kSemiBoldAka600)),
        bold_aka_700(new FontWeightValue(FontWeightValue::kBoldAka700)),
        extra_bold_aka_800(
            new FontWeightValue(FontWeightValue::kExtraBoldAka800)),
        black_aka_900(new FontWeightValue(FontWeightValue::kBlackAka900)) {}

  const scoped_refptr<FontWeightValue> thin_aka_100;
  const scoped_refptr<FontWeightValue> extra_light_aka_200;
  const scoped_refptr<FontWeightValue> light_aka_300;
  const scoped_refptr<FontWeightValue> normal_aka_400;
  const scoped_refptr<FontWeightValue> medium_aka_500;
  const scoped_refptr<FontWeightValue> semi_bold_aka_600;
  const scoped_refptr<FontWeightValue> bold_aka_700;
  const scoped_refptr<FontWeightValue> extra_bold_aka_800;
  const scoped_refptr<FontWeightValue> black_aka_900;
};

namespace {

base::LazyInstance<FontWeightValue::NonTrivialStaticFields>::DestructorAtExit
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

const scoped_refptr<FontWeightValue>& FontWeightValue::GetThinAka100() {
  return non_trivial_static_fields.Get().thin_aka_100;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetExtraLightAka200() {
  return non_trivial_static_fields.Get().extra_light_aka_200;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetLightAka300() {
  return non_trivial_static_fields.Get().light_aka_300;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetNormalAka400() {
  return non_trivial_static_fields.Get().normal_aka_400;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetMediumAka500() {
  return non_trivial_static_fields.Get().medium_aka_500;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetSemiBoldAka600() {
  return non_trivial_static_fields.Get().semi_bold_aka_600;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetBoldAka700() {
  return non_trivial_static_fields.Get().bold_aka_700;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetExtraBoldAka800() {
  return non_trivial_static_fields.Get().extra_bold_aka_800;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetBlackAka900() {
  return non_trivial_static_fields.Get().black_aka_900;
}

void FontWeightValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitFontWeight(this);
}

std::string FontWeightValue::ToString() const {
  switch (value_) {
    case kNormalAka400:
      return kNormalKeywordName;
    case kBoldAka700:
      return kBoldKeywordName;
    case kThinAka100:
    case kExtraLightAka200:
    case kLightAka300:
    case kMediumAka500:
    case kSemiBoldAka600:
    case kExtraBoldAka800:
    case kBlackAka900:
    default:
      // These values are not implemented by the scanner/parser.
      NOTIMPLEMENTED();
      return "";
  }
}

}  // namespace cssom
}  // namespace cobalt
