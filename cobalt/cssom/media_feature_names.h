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

#ifndef COBALT_CSSOM_MEDIA_FEATURE_NAMES_H_
#define COBALT_CSSOM_MEDIA_FEATURE_NAMES_H_

namespace cobalt {
namespace cssom {

// Lower-case names of CSS media features.
//   https://www.w3.org/TR/css3-mediaqueries/#media1
extern const char kAspectRatioMediaFeatureName[];
extern const char kColorIndexMediaFeatureName[];
extern const char kColorMediaFeatureName[];
extern const char kDeviceAspectRatioMediaFeatureName[];
extern const char kDeviceHeightMediaFeatureName[];
extern const char kDeviceWidthMediaFeatureName[];
extern const char kGridMediaFeatureName[];
extern const char kHeightMediaFeatureName[];
extern const char kMonochromeMediaFeatureName[];
extern const char kOrientationMediaFeatureName[];
extern const char kResolutionMediaFeatureName[];
extern const char kScanMediaFeatureName[];
extern const char kWidthMediaFeatureName[];

enum MediaFeatureName {
  kInvalidFeature,
  kAspectRatioMediaFeature,
  kColorIndexMediaFeature,
  kColorMediaFeature,
  kDeviceAspectRatioMediaFeature,
  kDeviceHeightMediaFeature,
  kDeviceWidthMediaFeature,
  kGridMediaFeature,
  kHeightMediaFeature,
  kMonochromeMediaFeature,
  kOrientationMediaFeature,
  kResolutionMediaFeature,
  kScanMediaFeature,
  kWidthMediaFeature,
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_MEDIA_FEATURE_NAMES_H_
