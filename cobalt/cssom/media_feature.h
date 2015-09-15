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

#ifndef CSSOM_MEDIA_FEATURE_H_
#define CSSOM_MEDIA_FEATURE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/media_feature_names.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

enum MediaFeatureOperator {
  kNonZero,
  kEquals,
  kMinimum,
  kMaximum,
};

// A media feature is a '(keyword:value)' block of a @media conditional rule
// segment.
class MediaFeature : public script::Wrappable {
 public:
  explicit MediaFeature(int name)
      : name_(static_cast<MediaFeatureName>(name)), operator_(kNonZero) {}

  MediaFeature(int name, const scoped_refptr<cssom::PropertyValue> &value)
      : name_(static_cast<MediaFeatureName>(name)),
        value_(value),
        operator_(kEquals) {}

  DEFINE_WRAPPABLE_TYPE(MediaFeature);

  void set_operator(MediaFeatureOperator value) { operator_ = value; }

 private:
  MediaFeatureName name_;
  scoped_refptr<cssom::PropertyValue> value_;
  MediaFeatureOperator operator_;

  DISALLOW_COPY_AND_ASSIGN(MediaFeature);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_MEDIA_FEATURE_H_
