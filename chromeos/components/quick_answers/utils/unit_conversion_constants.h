// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_UNIT_CONVERSION_CONSTANTS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_UNIT_CONVERSION_CONSTANTS_H_

#include <string>

namespace quick_answers {

inline constexpr char kRuleSetPath[] = "unitConversionResult.conversions";
inline constexpr char kSourceUnitPath[] = "unitConversionResult.sourceUnit";
inline constexpr char kSourceAmountPath[] = "unitConversionResult.sourceAmount";
inline constexpr char kDestAmountPath[] = "unitConversionResult.destAmount";
inline constexpr char kDestTextPath[] =
    "unitConversionResult.destination.valueAndUnit.rawText";

inline constexpr char kCategoryPath[] = "category";
inline constexpr char kConversionRateAPath[] = "conversionToSiA";
inline constexpr char kResultValueTemplate[] = "%.3f";
inline constexpr char kNamePath[] = "name";
inline constexpr char kUnitsPath[] = "units";

std::string GetUnitDisplayText(const std::string& name);

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_UNIT_CONVERSION_CONSTANTS_H_
