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

#ifndef COBALT_CSSOM_MEDIA_FEATURE_KEYWORD_VALUE_H_
#define COBALT_CSSOM_MEDIA_FEATURE_KEYWORD_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

class MediaFeatureKeywordValue : public PropertyValue {
 public:
  enum Value {
    // "interlace" is a value of the "scan" media feature which indicates that
    // the display is interlaced.
    //   https://www.w3.org/TR/css3-mediaqueries/#scan
    kInterlace,

    // "landscape" is a value of the "orientation" media feature which indicates
    // that the display is oriented in landscape mode.
    //   https://www.w3.org/TR/css3-mediaqueries/#orientation
    kLandscape,

    // "portrait" is a value of the "orientation" media feature which indicates
    // that the display is oriented in portrait mode.
    //   https://www.w3.org/TR/css3-mediaqueries/#orientation
    kPortrait,

    // "progressive" is a value of the "scan" media feature which indicates that
    // the display is progressive (not interlaced).
    //   https://www.w3.org/TR/css3-mediaqueries/#scan
    kProgressive,
  };

  // Since keyword values do not hold additional information and some of them
  // (namely "inherit" and "initial") are used extensively, for the sake of
  // saving memory an explicit instantiation of this class is disallowed.
  // Use factory methods below to obtain shared instances.
  static const scoped_refptr<MediaFeatureKeywordValue>& GetInterlace();
  static const scoped_refptr<MediaFeatureKeywordValue>& GetLandscape();
  static const scoped_refptr<MediaFeatureKeywordValue>& GetPortrait();
  static const scoped_refptr<MediaFeatureKeywordValue>& GetProgressive();

  void Accept(PropertyValueVisitor* visitor) override;

  Value value() const { return value_; }

  std::string ToString() const override;
  bool operator==(const MediaFeatureKeywordValue& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(MediaFeatureKeywordValue);

  // Implementation detail, has to be public in order to be constructible.
  struct NonTrivialStaticFields;

 private:
  explicit MediaFeatureKeywordValue(Value value) : value_(value) {}
  ~MediaFeatureKeywordValue() override {}

  const Value value_;

  DISALLOW_COPY_AND_ASSIGN(MediaFeatureKeywordValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_MEDIA_FEATURE_KEYWORD_VALUE_H_
