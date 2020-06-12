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

#include "cobalt/cssom/media_feature.h"

#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/ratio_value.h"
#include "cobalt/cssom/resolution_value.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/math/safe_integer_conversions.h"

namespace cobalt {
namespace cssom {
namespace {

// The constants below are for the media features that are constant for Cobalt.

// The 'color-index' media feature describes the number of entries in the color
// lookup table of the output device. If the device does not use a color lookup
// table, the value is zero.
//   https://www.w3.org/TR/css3-mediaqueries/#color-index
static const int kColorIndexMediaFeatureValue = 0;

// The 'color' media feature describes the number of bits per color component of
// the output device. If the device is not a color device, the value is zero.
//   https://www.w3.org/TR/css3-mediaqueries/#color
static const int kColorMediaFeatureValue = 8;

// The 'monochrome' media feature describes the number of bits per pixel in a
// monochrome frame buffer. If the device is not a monochrome device, the output
// device value will be 0.
//   https://www.w3.org/TR/css3-mediaqueries/#monochrome
static const int kMonochromeMediaFeatureValue = 0;

// The 'grid' media feature is used to query whether the output device is grid
// or bitmap. If the output device is grid-based (e.g., a "tty" terminal, or a
// phone display with only one fixed font), the value will be 1. Otherwise, the
// value will be 0.
//   https://www.w3.org/TR/css3-mediaqueries/#grid
static const int kGridMediaFeatureValue = 0;

// The 'scan' media feature describes the scanning process of "tv" output
// devices.
//   https://www.w3.org/TR/css3-mediaqueries/#scan
static const MediaFeatureKeywordValue::Value kScanMediaFeatureValue =
    MediaFeatureKeywordValue::kProgressive;
}  // namespace

MediaFeature::MediaFeature(int name)
    : name_(static_cast<MediaFeatureName>(name)), operator_(kNonZero) {}

MediaFeature::MediaFeature(int name, const scoped_refptr<PropertyValue>& value)
    : name_(static_cast<MediaFeatureName>(name)),
      value_(value),
      operator_(kEquals) {}

bool MediaFeature::CompareAspectRatio(
    const cssom::ViewportSize& viewport_size) {
  math::Rational media_ratio(viewport_size.width(), viewport_size.height());
  RatioValue* specified_ratio =
      base::polymorphic_downcast<RatioValue*>(value_.get());

  switch (operator_) {
    case kNonZero:
      return true;
    case kEquals:
      return specified_ratio->value() == media_ratio;
    case kMinimum:
      return specified_ratio->value() <= media_ratio;
    case kMaximum:
      return specified_ratio->value() >= media_ratio;
  }
  NOTREACHED();
  return false;
}

bool MediaFeature::CompareIntegerValue(int media_value) {
  IntegerValue* specified_value =
      base::polymorphic_downcast<IntegerValue*>(value_.get());

  switch (operator_) {
    case kNonZero:
      return 0 != media_value;
    case kEquals:
      return *specified_value == media_value;
    case kMinimum:
      return *specified_value <= media_value;
    case kMaximum:
      return *specified_value >= media_value;
  }
  NOTREACHED();
  return false;
}

bool MediaFeature::CompareLengthValue(int length_in_pixels) {
  if (operator_ == kNonZero) {
    return 0 != length_in_pixels;
  } else {
    LengthValue* specified_value =
        base::polymorphic_downcast<LengthValue*>(value_.get());
    // TODO: Support 'em' units for media features.
    // https://www.w3.org/TR/css3-mediaqueries/#units
    if (kPixelsUnit != specified_value->unit()) {
      DLOG(WARNING)
          << "Media feature length value only supported in pixel units.";
      return false;
    }

    switch (operator_) {
      case kEquals:
        return specified_value->value() == static_cast<float>(length_in_pixels);
      case kMinimum:
        return specified_value->value() <= static_cast<float>(length_in_pixels);
      case kMaximum:
        return specified_value->value() >= static_cast<float>(length_in_pixels);
      case kNonZero:
        break;
    }
    NOTREACHED();
    return false;
  }
}

bool MediaFeature::CompareOrientation(
    const cssom::ViewportSize& viewport_size) {
  MediaFeatureKeywordValue* specified_value =
      base::polymorphic_downcast<MediaFeatureKeywordValue*>(value_.get());

  MediaFeatureKeywordValue::Value media_value =
      viewport_size.height() >= viewport_size.width()
          ? MediaFeatureKeywordValue::kPortrait
          : MediaFeatureKeywordValue::kLandscape;

  switch (operator_) {
    case kNonZero:
      return true;
    case kEquals:
      return specified_value->value() == media_value;
    case kMinimum:
    case kMaximum:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

bool MediaFeature::CompareResolution(const cssom::ViewportSize& viewport_size) {
  ResolutionValue* specified_value =
      base::polymorphic_downcast<ResolutionValue*>(value_.get());
  float media_dpi = viewport_size.device_pixel_ratio() * 96;

  switch (operator_) {
    case kNonZero:
      return 0.0f != media_dpi;
    case kEquals:
      return specified_value->dpi_value() == media_dpi;
    case kMinimum:
      return specified_value->dpi_value() <= media_dpi;
    case kMaximum:
      return specified_value->dpi_value() >= media_dpi;
  }
  NOTREACHED();
  return false;
}

bool MediaFeature::CompareScan() {
  MediaFeatureKeywordValue* specified_value =
      base::polymorphic_downcast<MediaFeatureKeywordValue*>(value_.get());

  switch (operator_) {
    case kNonZero:
      return true;
    case kEquals:
      return specified_value->value() == kScanMediaFeatureValue;
    case kMinimum:
    case kMaximum:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

bool MediaFeature::EvaluateConditionValue(const ViewportSize& viewport_size) {
  switch (name_) {
    case kInvalidFeature:
      break;
    case kAspectRatioMediaFeature:
    case kDeviceAspectRatioMediaFeature:
      return CompareAspectRatio(viewport_size);
    case kColorIndexMediaFeature:
      return CompareIntegerValue(kColorIndexMediaFeatureValue);
    case kColorMediaFeature:
      return CompareIntegerValue(kColorMediaFeatureValue);
    case kDeviceHeightMediaFeature:
    case kHeightMediaFeature:
      return CompareLengthValue(viewport_size.height());
    case kDeviceWidthMediaFeature:
    case kWidthMediaFeature:
      return CompareLengthValue(viewport_size.width());
    case kGridMediaFeature:
      return CompareIntegerValue(kGridMediaFeatureValue);
    case kMonochromeMediaFeature:
      return CompareIntegerValue(kMonochromeMediaFeatureValue);
    case kOrientationMediaFeature:
      return CompareOrientation(viewport_size);
    case kResolutionMediaFeature:
      return CompareResolution(viewport_size);
    case kScanMediaFeature:
      return CompareScan();
  }
  NOTREACHED();
  return false;
}

}  // namespace cssom
}  // namespace cobalt
