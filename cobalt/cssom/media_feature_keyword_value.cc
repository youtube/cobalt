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

#include "cobalt/cssom/media_feature_keyword_value.h"

#include "base/lazy_instance.h"
#include "cobalt/cssom/media_feature_keyword_value_names.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

struct MediaFeatureKeywordValue::NonTrivialStaticFields {
  NonTrivialStaticFields()
      : interlace_value(
            new MediaFeatureKeywordValue(MediaFeatureKeywordValue::kInterlace)),
        landscape_value(
            new MediaFeatureKeywordValue(MediaFeatureKeywordValue::kLandscape)),
        portrait_value(
            new MediaFeatureKeywordValue(MediaFeatureKeywordValue::kPortrait)),
        progressive_value(new MediaFeatureKeywordValue(
            MediaFeatureKeywordValue::kProgressive)) {}

  const scoped_refptr<MediaFeatureKeywordValue> interlace_value;
  const scoped_refptr<MediaFeatureKeywordValue> landscape_value;
  const scoped_refptr<MediaFeatureKeywordValue> portrait_value;
  const scoped_refptr<MediaFeatureKeywordValue> progressive_value;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

namespace {

base::LazyInstance<MediaFeatureKeywordValue::NonTrivialStaticFields>::
    DestructorAtExit non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

const scoped_refptr<MediaFeatureKeywordValue>&
MediaFeatureKeywordValue::GetInterlace() {
  return non_trivial_static_fields.Get().interlace_value;
}

const scoped_refptr<MediaFeatureKeywordValue>&
MediaFeatureKeywordValue::GetLandscape() {
  return non_trivial_static_fields.Get().landscape_value;
}

const scoped_refptr<MediaFeatureKeywordValue>&
MediaFeatureKeywordValue::GetPortrait() {
  return non_trivial_static_fields.Get().portrait_value;
}

const scoped_refptr<MediaFeatureKeywordValue>&
MediaFeatureKeywordValue::GetProgressive() {
  return non_trivial_static_fields.Get().progressive_value;
}

void MediaFeatureKeywordValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitMediaFeatureKeywordValue(this);
}

std::string MediaFeatureKeywordValue::ToString() const {
  switch (value_) {
    case kInterlace:
      return kInterlaceMediaFeatureKeywordValueName;
    case kLandscape:
      return kLandscapeMediaFeatureKeywordValueName;
    case kPortrait:
      return kPortraitMediaFeatureKeywordValueName;
    case kProgressive:
      return kProgressiveMediaFeatureKeywordValueName;
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace cssom
}  // namespace cobalt
