// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_dialog_models.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

class YearComboboxModelTest : public testing::Test {
 public:
  YearComboboxModelTest() {
    auto kJune2017 = base::Time::FromSecondsSinceUnixEpoch(1497552271);
    task_environment_.AdvanceClock(kJune2017 - base::Time::Now());
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(YearComboboxModelTest, ExpirationYear) {
  YearComboboxModel model;
  ASSERT_EQ(11u, model.GetItemCount());  // Placeholder + 2017-2026.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR),
      model.GetItemAt(0));
  EXPECT_EQ(u"2017", model.GetItemAt(1));
  EXPECT_EQ(u"2018", model.GetItemAt(2));
  EXPECT_EQ(u"2019", model.GetItemAt(3));
  EXPECT_EQ(u"2020", model.GetItemAt(4));
  EXPECT_EQ(u"2021", model.GetItemAt(5));
  EXPECT_EQ(u"2022", model.GetItemAt(6));
  EXPECT_EQ(u"2023", model.GetItemAt(7));
  EXPECT_EQ(u"2024", model.GetItemAt(8));
  EXPECT_EQ(u"2025", model.GetItemAt(9));
  EXPECT_EQ(u"2026", model.GetItemAt(10));
}

// Tests that we show the correct years, including an additional year.
TEST_F(YearComboboxModelTest, ShowAdditionalYear) {
  YearComboboxModel model(2016);
  ASSERT_EQ(12u, model.GetItemCount());  // Placeholder + 2016 + 2017-2026.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR),
      model.GetItemAt(0));
  EXPECT_EQ(u"2016", model.GetItemAt(1));
  EXPECT_EQ(u"2017", model.GetItemAt(2));
  EXPECT_EQ(u"2018", model.GetItemAt(3));
  EXPECT_EQ(u"2019", model.GetItemAt(4));
  EXPECT_EQ(u"2020", model.GetItemAt(5));
  EXPECT_EQ(u"2021", model.GetItemAt(6));
  EXPECT_EQ(u"2022", model.GetItemAt(7));
  EXPECT_EQ(u"2023", model.GetItemAt(8));
  EXPECT_EQ(u"2024", model.GetItemAt(9));
  EXPECT_EQ(u"2025", model.GetItemAt(10));
  EXPECT_EQ(u"2026", model.GetItemAt(11));
}

// Tests that we show the additional year, even if it is more than 10 years from
// now.
TEST_F(YearComboboxModelTest, ExpirationYear_ShowFarFutureYear) {
  YearComboboxModel model(2042);
  ASSERT_EQ(12u, model.GetItemCount());  // Placeholder + 2017-2026 + 2042.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR),
      model.GetItemAt(0));
  EXPECT_EQ(u"2017", model.GetItemAt(1));
  EXPECT_EQ(u"2018", model.GetItemAt(2));
  EXPECT_EQ(u"2019", model.GetItemAt(3));
  EXPECT_EQ(u"2020", model.GetItemAt(4));
  EXPECT_EQ(u"2021", model.GetItemAt(5));
  EXPECT_EQ(u"2022", model.GetItemAt(6));
  EXPECT_EQ(u"2023", model.GetItemAt(7));
  EXPECT_EQ(u"2024", model.GetItemAt(8));
  EXPECT_EQ(u"2025", model.GetItemAt(9));
  EXPECT_EQ(u"2026", model.GetItemAt(10));
  EXPECT_EQ(u"2042", model.GetItemAt(11));
}

TEST_F(YearComboboxModelTest, SetDefaultIndexByYear) {
  YearComboboxModel model;
  model.SetDefaultIndexByYear(2017);
  ASSERT_EQ(u"2017", model.GetItemAt(model.GetDefaultIndex().value()));
}

TEST_F(YearComboboxModelTest, SetDefaultIndexByYearOutOfRange) {
  YearComboboxModel model;
  model.SetDefaultIndexByYear(2016);
  ASSERT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR),
      model.GetItemAt(model.GetDefaultIndex().value()));
}

TEST_F(YearComboboxModelTest, SetDefaultIndexByYearAdditionalYear) {
  YearComboboxModel model(2042);
  model.SetDefaultIndexByYear(2042);
  ASSERT_EQ(u"2042", model.GetItemAt(model.GetDefaultIndex().value()));
}

TEST(MonthComboboxModelTest, SetDefaultIndexByMonth) {
  MonthComboboxModel model;
  model.SetDefaultIndexByMonth(6);
  ASSERT_EQ(u"06", model.GetItemAt(model.GetDefaultIndex().value()));
}

TEST(MonthComboboxModelTest, SetDefaultIndexByMonthOutOfRange) {
  MonthComboboxModel model;
  model.SetDefaultIndexByMonth(13);
  ASSERT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH),
      model.GetItemAt(model.GetDefaultIndex().value()));
}

}  // namespace
}  // namespace autofill
