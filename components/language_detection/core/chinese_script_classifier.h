// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_CHINESE_SCRIPT_CLASSIFIER_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_CHINESE_SCRIPT_CLASSIFIER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "third_party/icu/source/common/unicode/uniset.h"

namespace language_detection {

class COMPONENT_EXPORT(LANGUAGE_DETECTION) ChineseScriptClassifier {
 public:
  // Initializes both the zh-Hans and zh-Hant UnicodeSets used for
  // lookup when Classify is called.
  ChineseScriptClassifier();
  ~ChineseScriptClassifier();

  // Given Chinese text as input, returns either zh-Hant or zh-Hans.
  // When the input is ambiguous, i.e. not completely zh-Hans and not
  // completely zh-Hant, this function returns the closest language code
  // matching the input.
  //
  // Behavior is undefined for non-Chinese input.
  std::string Classify(std::string_view input) const;
  std::string Classify(std::u16string_view input) const;

  // Returns true if the underlying transliterators were properly initialized
  // by the constructor.
  bool IsInitialized() const;

 private:
  std::string Classify(const icu::UnicodeString& input_codepoints) const;

  // Set of chars generally unique to zh-Hans.
  std::unique_ptr<icu::UnicodeSet> hans_set_;

  // Set of chars generally unique to zh-Hant.
  std::unique_ptr<icu::UnicodeSet> hant_set_;
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_CHINESE_SCRIPT_CLASSIFIER_H_
