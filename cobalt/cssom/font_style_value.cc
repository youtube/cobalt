// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/font_style_value.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_checker.h"
#include "cobalt/cssom/keyword_names.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

struct FontStyleValue::NonTrivialStaticFields {
  NonTrivialStaticFields()
      : italic(new FontStyleValue(FontStyleValue::kItalic)),
        normal(new FontStyleValue(FontStyleValue::kNormal)),
        oblique(new FontStyleValue(FontStyleValue::kOblique)) {}

  const scoped_refptr<FontStyleValue> italic;
  const scoped_refptr<FontStyleValue> normal;
  const scoped_refptr<FontStyleValue> oblique;
};

namespace {

base::LazyInstance<FontStyleValue::NonTrivialStaticFields>::DestructorAtExit
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

const scoped_refptr<FontStyleValue>& FontStyleValue::GetItalic() {
  return non_trivial_static_fields.Get().italic;
}

const scoped_refptr<FontStyleValue>& FontStyleValue::GetNormal() {
  return non_trivial_static_fields.Get().normal;
}

const scoped_refptr<FontStyleValue>& FontStyleValue::GetOblique() {
  return non_trivial_static_fields.Get().oblique;
}

void FontStyleValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitFontStyle(this);
}

std::string FontStyleValue::ToString() const {
  switch (value_) {
    case kItalic:
      return kItalicKeywordName;
    case kNormal:
      return kNormalKeywordName;
    case kOblique:
      return kObliqueKeywordName;
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace cssom
}  // namespace cobalt
