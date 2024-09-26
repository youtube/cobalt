// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/filter_factory.h"
#include "ui/base/prediction/one_euro_filter.h"
#include "ui/base/ui_base_features.h"

namespace blink {
namespace test {

namespace {
using base::Feature;
using input_prediction::FilterType;
using input_prediction::PredictorType;
}  // namespace

class FilterFactoryTest : public testing::Test {
 public:
  FilterFactoryTest() {
    CreateNewFactory(blink::features::kFilteringScrollPrediction,
                     PredictorType::kScrollPredictorTypeKalman,
                     FilterType::kEmpty);
  }
  FilterFactoryTest(const FilterFactoryTest&) = delete;
  FilterFactoryTest& operator=(const FilterFactoryTest&) = delete;

  void GetFilterParams(const FilterType& filter_type,
                       const PredictorType& predictor_type,
                       FilterParams* filter_params) {
    factory_->GetFilterParams(filter_type, predictor_type, filter_params);
  }

  FilterType GetFilterTypeFromName(const std::string& filter_name) {
    return factory_->GetFilterTypeFromName(filter_name);
  }

  std::unique_ptr<ui::InputFilter> CreateFilter(
      const input_prediction::FilterType filter_type,
      const input_prediction::PredictorType predictor_type) {
    return factory_->CreateFilter(filter_type, predictor_type);
  }

  void CreateNewFactory(const base::Feature& feature,
                        const input_prediction::PredictorType predictor_type,
                        const input_prediction::FilterType filter_type) {
    factory_ =
        std::make_unique<FilterFactory>(feature, predictor_type, filter_type);
  }

 private:
  std::unique_ptr<FilterFactory> factory_;
};

// Check if the FilterType returned is correct
TEST_F(FilterFactoryTest, TestGetFilterType) {
  EXPECT_EQ(input_prediction::FilterType::kEmpty,
            GetFilterTypeFromName(::features::kFilterNameEmpty));

  EXPECT_EQ(input_prediction::FilterType::kOneEuro,
            GetFilterTypeFromName(::features::kFilterNameOneEuro));

  // Default type Empty
  EXPECT_EQ(input_prediction::FilterType::kEmpty, GetFilterTypeFromName(""));
}

TEST_F(FilterFactoryTest, TestCreateFilter) {
  EXPECT_STREQ(
      ::features::kFilterNameEmpty,
      CreateFilter(input_prediction::FilterType::kEmpty,
                   input_prediction::PredictorType::kScrollPredictorTypeEmpty)
          ->GetName());

  EXPECT_STREQ(
      ::features::kFilterNameOneEuro,
      CreateFilter(input_prediction::FilterType::kOneEuro,
                   input_prediction::PredictorType::kScrollPredictorTypeEmpty)
          ->GetName());
}

// Test there is no params available for OneEuro filter
TEST_F(FilterFactoryTest, TestOneEuroNoParams) {
  FilterParams filter_params;

  GetFilterParams(FilterType::kOneEuro,
                  PredictorType::kScrollPredictorTypeKalman, &filter_params);
  EXPECT_TRUE(filter_params.empty());
}

// Test we get the params sent via fieldtrials params
TEST_F(FilterFactoryTest, TestOneEuroParams) {
  FilterParams filter_params;

  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams field_trial_params;

  field_trial_params[ui::OneEuroFilter::kParamMincutoff] = "33";
  field_trial_params[ui::OneEuroFilter::kParamBeta] = "42";
  scoped_feature_list.Reset();
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kFilteringScrollPrediction, field_trial_params);

  // Create a new factory to load fieldtrials params values
  CreateNewFactory(blink::features::kFilteringScrollPrediction,
                   PredictorType::kScrollPredictorTypeKalman,
                   FilterType::kOneEuro);

  GetFilterParams(FilterType::kOneEuro,
                  PredictorType::kScrollPredictorTypeKalman, &filter_params);

  EXPECT_EQ((int)filter_params.size(), 2);
  EXPECT_EQ(filter_params.find(ui::OneEuroFilter::kParamMincutoff)->second, 33);
  EXPECT_EQ(filter_params.find(ui::OneEuroFilter::kParamBeta)->second, 42);

  // fieldtrials params shouldn't be available for another predictor
  filter_params.clear();
  GetFilterParams(FilterType::kOneEuro, PredictorType::kScrollPredictorTypeLsq,
                  &filter_params);

  EXPECT_TRUE(filter_params.empty());
}

TEST_F(FilterFactoryTest, TestGetFilter) {
  EXPECT_STREQ(
      ::features::kFilterNameEmpty,
      CreateFilter(FilterType::kEmpty, PredictorType::kScrollPredictorTypeEmpty)
          ->GetName());

  EXPECT_STREQ(::features::kFilterNameOneEuro,
               CreateFilter(FilterType::kOneEuro,
                            PredictorType::kScrollPredictorTypeEmpty)
                   ->GetName());
}

}  // namespace test
}  // namespace blink
