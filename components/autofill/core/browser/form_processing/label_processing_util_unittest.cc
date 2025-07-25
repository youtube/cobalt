// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/label_processing_util.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(LabelProcessingUtil, GetParseableNameStringPieces) {
  std::vector<base::StringPiece16> labels{u"City", u"Street & House Number",
                                          u"", u"Zip"};
  auto expectation = absl::make_optional(
      std::vector<std::u16string>{u"City", u"Street", u"House Number", u"Zip"});
  EXPECT_EQ(GetParseableLabels(labels), expectation);

  // The label is also split when consecutive fields share the same label.
  labels[2] = labels[1];
  EXPECT_EQ(GetParseableLabels(labels), expectation);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_ThreeComponents) {
  EXPECT_EQ(GetParseableLabels(
                {u"City", u"Street & House Number & Floor", u"", u"", u"Zip"}),
            absl::make_optional(std::vector<std::u16string>{
                u"City", u"Street", u"House Number", u"Floor", u"Zip"}));
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_TooManyComponents) {
  EXPECT_EQ(
      GetParseableLabels({u"City", u"Street & House Number & Floor & Stairs",
                          u"", u"", u"", u"Zip"}),
      absl::nullopt);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_UnmachtingComponents) {
  EXPECT_EQ(GetParseableLabels(
                {u"City", u"Street & House Number & Floor", u"", u"Zip"}),
            absl::nullopt);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_SplitableLabelAtEnd) {
  EXPECT_EQ(GetParseableLabels(
                {u"City", u"", u"Zip", u"Street & House Number & Floor"}),
            absl::nullopt);
}

TEST(LabelProcessingUtil, GetParseableNameStringPieces_TooLongLabel) {
  EXPECT_EQ(GetParseableLabels({u"City",
                                u"Street & House Number with a lot of "
                                u"additional text that exceeds 40 "
                                u"characters by far",
                                u"", u"Zip"}),
            absl::nullopt);
}

}  // namespace autofill
