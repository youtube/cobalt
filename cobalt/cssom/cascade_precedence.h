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

#ifndef COBALT_CSSOM_CASCADE_PRECEDENCE_H_
#define COBALT_CSSOM_CASCADE_PRECEDENCE_H_

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

// The enum origin here includes both origin and importance in the spec. The
// origin of a declaration is based on where it comes from and its importance
// is whether or not it is declared !important.
//   https://www.w3.org/TR/css-cascade-3/#cascade-origin
// The origin with higher precedence appears later in the enum, and is therefore
// with higher value.
enum Origin {
  kNormalUserAgent,
  kNormalAuthor,
  kNormalOverride,
  kImportantMin,  // The lowest important origin.
  kImportantAuthor = kImportantMin,
  kImportantOverride,
  kImportantUserAgent,
  kImportantMax = kImportantUserAgent,
};

// Appearance is the position of the declaration in the document.
//   https://www.w3.org/TR/css-cascade-3/#cascade-order
class Appearance {
 public:
  static const int kUnattached = -1;

  Appearance() : style_sheet_index_(kUnattached), rule_index_(kUnattached) {}
  Appearance(int style_sheet_index, int rule_index)
      : style_sheet_index_(style_sheet_index), rule_index_(rule_index) {
    DCHECK_GE(style_sheet_index, kUnattached) << "Wrong style sheet index.";
    DCHECK_GE(rule_index, kUnattached) << "Wrong rule index.";
  }

  // Earlier appearance is smaller. The fields are compared lexicographically.
  bool operator<(const Appearance& rhs) const {
    return style_sheet_index_ < rhs.style_sheet_index_ ||
           (style_sheet_index_ == rhs.style_sheet_index_ &&
            rule_index_ < rhs.rule_index_);
  }
  bool operator==(const Appearance& rhs) const {
    return style_sheet_index_ == rhs.style_sheet_index_ &&
           rule_index_ == rhs.rule_index_;
  }

 private:
  int style_sheet_index_;
  int rule_index_;
};

// Cascade precedence is the value that is used in sorting the declarations in
// the cascade.
//   https://www.w3.org/TR/css-cascade-3/#cascade
// In Cobalt the cascade precedence includes origin, specificity and appearance,
// which is compared lexicographically.
class CascadePrecedence {
 public:
  explicit CascadePrecedence(Origin origin) : origin_(origin) {}
  CascadePrecedence(Origin origin, const Specificity& specificity,
                  const Appearance& appearance)
      : origin_(origin), specificity_(specificity), appearance_(appearance) {}

  Origin origin() const { return origin_; }
  const Specificity& specificity() const { return specificity_; }
  const Appearance& appearance() const { return appearance_; }

  void SetImportant() {
    switch (origin_) {
      case kNormalUserAgent: {
        origin_ = kImportantUserAgent;
        break;
      }
      case kNormalAuthor: {
        origin_ = kImportantAuthor;
        break;
      }
      case kNormalOverride: {
        origin_ = kImportantOverride;
        break;
      }
      case kImportantAuthor:
      case kImportantOverride:
      case kImportantUserAgent:
        NOTREACHED();
    }
  }

  bool operator<(const CascadePrecedence& rhs) const {
    return origin_ < rhs.origin_ ||
           (origin_ == rhs.origin_ && (specificity_ < rhs.specificity_ ||
                                       (specificity_ == rhs.specificity_ &&
                                        appearance_ < rhs.appearance_)));
  }
  bool operator==(const CascadePrecedence& rhs) const {
    return origin_ == rhs.origin_ && specificity_ == rhs.specificity_ &&
           appearance_ == rhs.appearance_;
  }

 private:
  Origin origin_;
  Specificity specificity_;
  Appearance appearance_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CASCADE_PRECEDENCE_H_
