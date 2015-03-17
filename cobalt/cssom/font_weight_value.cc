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

#include "cobalt/cssom/font_weight_value.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_checker.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

namespace {

struct NonTrivialStaticFields {
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

  base::ThreadChecker thread_checker;

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

base::LazyInstance<NonTrivialStaticFields> non_trivial_static_fields =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

const scoped_refptr<FontWeightValue>& FontWeightValue::GetThinAka100() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().thin_aka_100;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetExtraLightAka200() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().extra_light_aka_200;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetLightAka300() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().light_aka_300;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetNormalAka400() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().normal_aka_400;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetMediumAka500() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().medium_aka_500;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetSemiBoldAka600() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().semi_bold_aka_600;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetBoldAka700() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().bold_aka_700;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetExtraBoldAka800() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().extra_bold_aka_800;
}

const scoped_refptr<FontWeightValue>& FontWeightValue::GetBlackAka900() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().black_aka_900;
}

void FontWeightValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitFontWeight(this);
}

}  // namespace cssom
}  // namespace cobalt
