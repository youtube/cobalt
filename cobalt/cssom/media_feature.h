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

#ifndef COBALT_CSSOM_MEDIA_FEATURE_H_
#define COBALT_CSSOM_MEDIA_FEATURE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/media_feature_names.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/math/size.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class PropertyValue;

enum MediaFeatureOperator {
  kNonZero,
  kEquals,
  kMinimum,
  kMaximum,
};

// A media feature is a '(keyword:value)' block of a @media conditional rule
// segment.
//   https://www.w3.org/TR/css3-mediaqueries/#media1
class MediaFeature : public script::Wrappable {
 public:
  explicit MediaFeature(int name);
  MediaFeature(int name, const scoped_refptr<PropertyValue>& value);

  DEFINE_WRAPPABLE_TYPE(MediaFeature);

  void set_operator(MediaFeatureOperator value) { operator_ = value; }

  // When given the media values for width and height, returns true if the media
  // query expression is true.
  bool EvaluateConditionValue(const ViewportSize& viewport_size);

 private:
  // The 'aspect-ratio' media feature is defined as the ratio of the value of
  // the 'width' media feature to the value of the 'height' media feature.
  //   https://www.w3.org/TR/css3-mediaqueries/#aspect-ratio
  // Returns true if the media query expression for aspect ratio is true for the
  // given media values.
  bool CompareAspectRatio(const ViewportSize& viewport_size);

  // Returns true if the media query expression for an integer value is true for
  // the given media value.
  bool CompareIntegerValue(int value);

  // Returns true if the media query expression for a length value is true for
  // the given media value.
  bool CompareLengthValue(int length_in_pixels);

  // The 'orientation' media feature is 'portrait' when the value of the
  // 'height' media feature is greater than or equal to the value of the 'width'
  // media feature. Otherwise 'orientation' is 'landscape'.
  //   https://www.w3.org/TR/css3-mediaqueries/#orientation
  bool CompareOrientation(const ViewportSize& viewport_size);

  // The 'resolution' media feature describes the resolution of the output
  // device, i.e. the density of the pixels.
  //   https://www.w3.org/TR/css3-mediaqueries/#resolution
  bool CompareResolution(const ViewportSize& viewport_size);

  // The 'scan' media feature describes the scanning process of "tv" output
  // devices.
  //   https://www.w3.org/TR/css3-mediaqueries/#scan
  bool CompareScan();

  MediaFeatureName name_;
  scoped_refptr<PropertyValue> value_;
  MediaFeatureOperator operator_;

  DISALLOW_COPY_AND_ASSIGN(MediaFeature);
};

typedef std::vector<scoped_refptr<MediaFeature> > MediaFeatures;

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_MEDIA_FEATURE_H_
