// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/autofill_manager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <tuple>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/translate/core/common/language_detection_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#endif

namespace autofill {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;
using ::testing::VariantWith;
using FieldTypeSource = AutofillManager::Observer::FieldTypeSource;

class MockAutofillDriver : public TestAutofillDriver {
 public:
  using TestAutofillDriver::TestAutofillDriver;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  ~MockAutofillDriver() override = default;

  MOCK_METHOD(bool, CanShowAutofillUi, (), (const));
  MOCK_METHOD(void,
              TriggerFormExtractionInAllFrames,
              (base::OnceCallback<void(bool)>),
              ());
};

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
class MockFieldClassificationModelHandler
    : public FieldClassificationModelHandler {
 public:
  explicit MockFieldClassificationModelHandler(
      optimization_guide::OptimizationGuideModelProvider* provider)
      : FieldClassificationModelHandler(
            provider,
            optimization_guide::proto::OptimizationTarget::
                OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION) {}
  ~MockFieldClassificationModelHandler() override = default;

  MOCK_METHOD(
      void,
      GetModelPredictionsForForms,
      (std::vector<std::unique_ptr<FormStructure>>,
       base::OnceCallback<void(std::vector<std::unique_ptr<FormStructure>>)>),
      (override));
};
#endif

// Creates a vector of test forms which differ in their FormGlobalIds
// and FieldGlobalIds.
std::vector<FormData> CreateTestForms(size_t num_forms) {
  std::vector<FormData> forms;
  for (size_t i = 0; i < num_forms; ++i) {
    forms.push_back(test::CreateTestAddressFormData());
  }
  return forms;
}

// Returns the FormGlobalIds of the specified |forms|.
std::vector<FormGlobalId> GetFormIds(const std::vector<FormData>& forms) {
  return base::ToVector(forms, &FormData::global_id);
}

// Matches a std::map<FormGlobalId, std::unique_ptr<FormStructure>>::value_type
// whose key is `form.global_id()`.
auto HaveSameFormIdAs(const FormData& form) {
  return Field(
      "global_id",
      &std::pair<const FormGlobalId, std::unique_ptr<FormStructure>>::first,
      form.global_id());
}

// Matches a std::map<FormGlobalId, std::unique_ptr<FormStructure>> whose
// keys are the same the FormGlobalIds of the forms in |forms|.
auto HaveSameFormIdsAs(const std::vector<FormData>& forms) {
  std::vector<decltype(HaveSameFormIdAs(forms.front()))> matchers;
  matchers.reserve(forms.size());
  std::ranges::transform(forms, std::back_inserter(matchers),
                         &HaveSameFormIdAs);
  return UnorderedElementsAreArray(matchers);
}

// Expects the calls triggered by OnFormsSeen().
void OnFormsSeenWithExpectations(MockAutofillManager& manager,
                                 const std::vector<FormData>& updated_forms,
                                 const std::vector<FormGlobalId>& removed_forms,
                                 const std::vector<FormData>& expectation) {
  size_t num =
      std::min(updated_forms.size(), kAutofillManagerMaxFormCacheSize -
                                         manager.form_structures().size());
  EXPECT_CALL(manager, ShouldParseForms).Times(1).WillOnce(Return(true));
  EXPECT_CALL(manager, OnBeforeProcessParsedForms()).Times(num > 0);
  EXPECT_CALL(manager, OnFormProcessed).Times(num);
  TestAutofillManagerWaiter waiter(manager, {AutofillManagerEvent::kFormsSeen});
  manager.OnFormsSeen(updated_forms, removed_forms);
  ASSERT_TRUE(waiter.Wait());
  EXPECT_THAT(manager.form_structures(), HaveSameFormIdsAs(expectation));
}

class AutofillManagerTest : public testing::Test {
 public:
  void SetUp() override {
    client_.SetPrefs(test::PrefServiceForTesting());
    driver_ = std::make_unique<NiceMock<MockAutofillDriver>>(&client_);
    driver_->set_autofill_manager(
        std::make_unique<MockAutofillManager>(driver_.get()));
  }

  void TearDown() override { driver_.reset(); }

  MockAutofillManager& manager() {
    return static_cast<MockAutofillManager&>(driver_->GetAutofillManager());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient client_;
  std::unique_ptr<MockAutofillDriver> driver_;
};

// The test parameter sets the number of forms to be generated.
class AutofillManagerTest_WithIntParam
    : public AutofillManagerTest,
      public ::testing::WithParamInterface<size_t> {
 public:
  size_t num_forms() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(AutofillManagerTest,
                         AutofillManagerTest_WithIntParam,
                         testing::Values(0, 1, 5, 10, 100, 110));

class AutofillManagerTest_OnLoadedServerPredictionsObserver
    : public AutofillManagerTest {
 public:
  using QueryResponse = AutofillCrowdsourcingManager::QueryResponse;

  void SetUp() override {
    AutofillManagerTest::SetUp();
    manager().AddObserver(&observer_);
  }

  void TearDown() override {
    manager().RemoveObserver(&observer_);
    AutofillManagerTest::TearDown();
  }

  MockAutofillCrowdsourcingManager& crowdsourcing_manager() {
    return static_cast<MockAutofillCrowdsourcingManager&>(
        client_.GetCrowdsourcingManager());
  }

  MockAutofillManagerObserver observer_;
};

// Tests that the cache size is bounded by kAutofillManagerMaxFormCacheSize.
TEST_P(AutofillManagerTest_WithIntParam, CacheBoundFormsSeen) {
  size_t num_exp_forms =
      std::min(num_forms(), kAutofillManagerMaxFormCacheSize);
  std::vector<FormData> forms = CreateTestForms(num_forms());
  std::vector<FormData> exp_forms(forms.begin(), forms.begin() + num_exp_forms);
  OnFormsSeenWithExpectations(manager(), forms, {}, exp_forms);
}

// Tests that removing unseen forms has no effect.
TEST_F(AutofillManagerTest, RemoveUnseenForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  OnFormsSeenWithExpectations(manager(), {}, GetFormIds(forms), {});
}

// Tests that all forms can be removed at once.
TEST_F(AutofillManagerTest, RemoveAllForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  OnFormsSeenWithExpectations(manager(), forms, {}, forms);
  OnFormsSeenWithExpectations(manager(), {}, GetFormIds(forms), {});
}

// Tests that removing some forms leaves the other forms untouched.
TEST_F(AutofillManagerTest, RemoveSomeForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  auto range = [&](size_t begin, size_t end) {
    return std::vector<FormData>(forms.begin() + begin, forms.begin() + end);
  };
  OnFormsSeenWithExpectations(manager(), range(0, 6), {}, range(0, 6));
  OnFormsSeenWithExpectations(manager(), range(6, 9), GetFormIds(range(0, 3)),
                              range(3, 9));
}

// Tests that adding and removing the same forms has no effect.
TEST_F(AutofillManagerTest, UpdateAndRemoveSameForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  OnFormsSeenWithExpectations(manager(), forms, GetFormIds(forms), forms);
  OnFormsSeenWithExpectations(manager(), forms, GetFormIds(forms), forms);
}

// Tests that events update the form cache. Since there are so many events and
// so many properties of forms that may change, the test only covers a small
// fraction:
// - Events: OnFormsSeen(), OnTextFieldValueChanged(), OnFocusOnFormField()
// - Properties: AutofillField::value()
TEST_F(AutofillManagerTest, FormCacheUpdatesValue) {
  EXPECT_CALL(manager(), ShouldParseForms)
      .Times(AtLeast(0))
      .WillRepeatedly(Return(true));
  TestAutofillManagerWaiter waiter(manager());

  FormData form = test::CreateTestAddressFormData();
  FormGlobalId form_id = form.global_id();
  auto current_cached_value = [this,
                               &form_id]() -> std::optional<std::u16string> {
    FormStructure* cached_form = manager().FindCachedFormById(form_id);
    if (!cached_form || cached_form->fields().empty()) {
      return std::nullopt;
    }
    return cached_form->fields()[0]->value();
  };

  EXPECT_EQ(current_cached_value(), std::nullopt);

  // Triggers a parse.
  test_api(form).field(0).set_value(u"first seen value");
  manager().OnFormsSeen({form}, {});
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"first seen value");

  // Triggers a reparse.
  test_api(form).field(0).set_value(u"second seen value");
  manager().OnFormsSeen({form}, {});
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"second seen value");

  // Triggers no reparse.
  test_api(form).field(0).set_value(u"first changed value");
  manager().OnTextFieldValueChanged(form, form.fields()[0].global_id(), {});
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"first changed value");

  // Triggers no reparse.
  test_api(form).field(0).set_value(u"second changed value");
  manager().OnTextFieldValueChanged(form, form.fields()[0].global_id(), {});
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"second changed value");

  // Triggers a reparse.
  test_api(form).Remove(-1);
  test_api(form).field(0).set_value(u"first reparse value");
  manager().OnFocusOnFormField(form, form.fields()[0].global_id());
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"first reparse value");

  // Triggers a second reparse.
  test_api(form).Remove(-1);
  test_api(form).field(0).set_value(u"second reparse value");
  manager().OnFocusOnFormField(form, form.fields()[0].global_id());
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"second reparse value");
}

TEST_F(AutofillManagerTest, ObserverReceiveCalls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillPageLanguageDetection},
      /*disabled_features=*/{});

  std::vector<FormData> forms = CreateTestForms(2);
  FormData form = forms[0];
  FormData other_form = forms[1];
  FormFieldData field = form.fields().front();
  FormGlobalId id_to_remove = test::MakeFormGlobalId();

  // Shorthands for matchers to reduce visual noise.
  using enum AutofillManager::LifecycleState;
  auto m = Ref(manager());
  auto f = Eq(form.global_id());
  auto g = Eq(other_form.global_id());
  auto ff = Eq(field.global_id());
  auto heuristics = Eq(FieldTypeSource::kHeuristicsOrAutocomplete);

  MockAutofillManagerObserver observer;
  base::ScopedObservation<AutofillManager, MockAutofillManagerObserver>
      observation{&observer};
  observation.Observe(&manager());

  // This test should have no unexpected calls of observer events.
  EXPECT_CALL(observer, OnAutofillManagerStateChanged).Times(0);
  EXPECT_CALL(observer, OnBeforeLanguageDetermined).Times(0);
  EXPECT_CALL(observer, OnAfterLanguageDetermined).Times(0);
  EXPECT_CALL(observer, OnBeforeFormsSeen).Times(0);
  EXPECT_CALL(observer, OnAfterFormsSeen).Times(0);
  EXPECT_CALL(observer, OnBeforeTextFieldValueChanged).Times(0);
  EXPECT_CALL(observer, OnAfterTextFieldValueChanged).Times(0);
  EXPECT_CALL(observer, OnBeforeTextFieldDidScroll).Times(0);
  EXPECT_CALL(observer, OnAfterTextFieldDidScroll).Times(0);
  EXPECT_CALL(observer, OnBeforeSelectControlSelectionChanged).Times(0);
  EXPECT_CALL(observer, OnAfterSelectControlSelectionChanged).Times(0);
  EXPECT_CALL(observer, OnBeforeDidFillAutofillFormData).Times(0);
  EXPECT_CALL(observer, OnAfterDidFillAutofillFormData).Times(0);
  EXPECT_CALL(observer, OnBeforeAskForValuesToFill).Times(0);
  EXPECT_CALL(observer, OnAfterAskForValuesToFill).Times(0);
  EXPECT_CALL(observer, OnBeforeFocusOnFormField).Times(0);
  EXPECT_CALL(observer, OnAfterFocusOnFormField).Times(0);
  EXPECT_CALL(observer, OnBeforeJavaScriptChangedAutofilledValue).Times(0);
  EXPECT_CALL(observer, OnAfterJavaScriptChangedAutofilledValue).Times(0);
  EXPECT_CALL(observer, OnBeforeLoadedServerPredictions).Times(0);
  EXPECT_CALL(observer, OnAfterLoadedServerPredictions).Times(0);
  EXPECT_CALL(observer, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer, OnFillOrPreviewDataModelForm).Times(0);
  EXPECT_CALL(observer, OnFormSubmitted).Times(0);

  EXPECT_CALL(manager(), ShouldParseForms)
      .Times(AtLeast(0))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(manager(), OnFocusOnNonFormFieldImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnDidFillAutofillFormDataImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnDidEndTextFieldEditingImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnSelectFieldOptionsDidChangeImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnJavaScriptChangedAutofilledValueImpl)
      .Times(AtLeast(0));
  EXPECT_CALL(manager(), OnFormSubmittedImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnTextFieldValueChangedImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnTextFieldDidScrollImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnAskForValuesToFillImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnFocusOnFormFieldImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnSelectControlSelectionChangedImpl).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnBeforeProcessParsedForms).Times(AtLeast(0));
  EXPECT_CALL(manager(), OnFormProcessed).Times(AtLeast(0));

  // Reset the manager and transition from kInactive to kActive. The observers
  // should stick around.
  EXPECT_CALL(observer,
              OnAutofillManagerStateChanged(m, kInactive, kPendingReset));
  EXPECT_CALL(observer,
              OnAutofillManagerStateChanged(m, kPendingReset, kInactive));
  test_api(client_.GetAutofillDriverFactory()).Reset(*driver_);
  EXPECT_CALL(observer, OnAutofillManagerStateChanged(m, kInactive, kActive));
  test_api(client_.GetAutofillDriverFactory())
      .SetLifecycleStateAndNotifyObservers(*driver_, kActive);

  // Test that not changing the LifecycleState does not fire events.
  test_api(*driver_).SetLifecycleStateAndNotifyObservers(kActive);

  // Test that even if the vector of new forms is too large, events are still
  // fired for the removed form.
  EXPECT_CALL(observer,
              OnBeforeFormsSeen(m, IsEmpty(), ElementsAre(id_to_remove)));
  EXPECT_CALL(observer,
              OnAfterFormsSeen(m, IsEmpty(), ElementsAre(id_to_remove)));
  manager().OnFormsSeen(std::vector<FormData>(kMaxListSize + 10),
                        {id_to_remove});

  // Test that seeing a normal amount of new forms triggers all events. Parsing
  // is asynchronous, so the OnAfterFoo() events are asynchronous, too.
  EXPECT_CALL(observer, OnBeforeFormsSeen(m, ElementsAre(f, g),
                                          ElementsAre(id_to_remove)));
  EXPECT_CALL(observer, OnBeforeLoadedServerPredictions(m));
  manager().OnFormsSeen(forms, {id_to_remove});
  EXPECT_CALL(observer, OnAfterLoadedServerPredictions(m));
  EXPECT_CALL(observer, OnAfterFormsSeen(m, ElementsAre(f, g),
                                         ElementsAre(id_to_remove)));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, f, heuristics));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, g, heuristics));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(m));
  manager().OnLanguageDetermined([] {
    translate::LanguageDetectionDetails details;
    details.adopted_language = "en";
    return details;
  }());
  EXPECT_CALL(observer, OnAfterLanguageDetermined(m));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, f, heuristics));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, g, heuristics));
  task_environment_.RunUntilIdle();

  test_api(form).Append(form.fields().back());
  test_api(form).field(-1).set_renderer_id(test::MakeFieldRendererId());

  // The form was just changed, which causes a reparse. The reparse is
  // asynchronous, so OnAfterTextFieldValueChanged() is asynchronous, too.
  EXPECT_CALL(observer, OnBeforeTextFieldValueChanged(m, f, ff));
  manager().OnTextFieldValueChanged(form, field.global_id(), {});
  EXPECT_CALL(observer,
              OnAfterTextFieldValueChanged(m, f, ff, std::u16string()));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, f, heuristics));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(observer, OnBeforeTextFieldDidScroll(m, f, ff));
  EXPECT_CALL(observer, OnAfterTextFieldDidScroll(m, f, ff));
  manager().OnTextFieldDidScroll(form, field.global_id());

  EXPECT_CALL(observer, OnBeforeDidFillAutofillFormData(m, f));
  EXPECT_CALL(observer, OnAfterDidFillAutofillFormData(m, f));
  manager().OnDidFillAutofillFormData(form, {});

  EXPECT_CALL(observer, OnBeforeAskForValuesToFill(m, f, ff, Ref(form)));
  EXPECT_CALL(observer, OnAfterAskForValuesToFill(m, f, ff));
  manager().OnAskForValuesToFill(form, field.global_id(), gfx::Rect(),
                                 AutofillSuggestionTriggerSource::kUnspecified,
                                 std::nullopt);

  EXPECT_CALL(observer, OnBeforeFocusOnFormField(m, f, ff));
  EXPECT_CALL(observer, OnAfterFocusOnFormField(m, f, ff));
  manager().OnFocusOnFormField(form, field.global_id());

  EXPECT_CALL(observer, OnBeforeJavaScriptChangedAutofilledValue(m, f, ff));
  EXPECT_CALL(observer, OnAfterJavaScriptChangedAutofilledValue(m, f, ff));
  manager().OnJavaScriptChangedAutofilledValue(form, field.global_id(), {});

  // TODO(crbug.com/) Test in browser_autofill_manager_unittest.cc that
  // FillOrPreviewForm() triggers OnFillOrPreviewDataModelForm().

  EXPECT_CALL(observer, OnFormSubmitted(m, Ref(form)));
  manager().OnFormSubmitted(form, mojom::SubmissionSource::FORM_SUBMISSION);

  // Reset the manager, the observers should stick around.
  EXPECT_CALL(observer,
              OnAutofillManagerStateChanged(m, kActive, kPendingDeletion))
      .WillOnce(Invoke([&] { observation.Reset(); }));
  test_api(*driver_).SetLifecycleStateAndNotifyObservers(kPendingDeletion);
  driver_.reset();
}

TEST_F(AutofillManagerTest, CanShowAutofillUi) {
  EXPECT_CALL(*driver_, CanShowAutofillUi).WillOnce(Return(true));
  EXPECT_TRUE(manager().CanShowAutofillUi());
}

TEST_F(AutofillManagerTest, TriggerFormExtractionInAllFrames) {
  EXPECT_CALL(*driver_, TriggerFormExtractionInAllFrames);
  manager().TriggerFormExtractionInAllFrames(base::DoNothing());
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
// Ensure that `FieldClassificationModelHandler`s are called when parsing the
// form in `ParseFormsAsync()`. These tests intentionally don't associate
// predictions to the `FormStructure`, they only verify that
// `GetModelPredictionsForForms` gets called.
class AutofillManagerTestForModelPredictions : public AutofillManagerTest {
 public:
  void SetUp() override {
    AutofillManagerTest::SetUp();
    observation_.Observe(&manager());
  }

  void TearDown() override {
    // The handler owns the model executor, which operates on a background
    // thread and is destroyed asynchronously. Resetting handlers owned by the
    // client triggers their destruction. Wait for it to complete. This needs to
    // happen before the `provider` is destroyed.
    observation_.Reset();
    client_.set_autofill_ml_prediction_model_handler(nullptr);
    client_.set_password_ml_prediction_model_handler(nullptr);
    task_environment_.RunUntilIdle();
    AutofillManagerTest::TearDown();
  }

  // Creates a model handler that is just runs the final callback upon receiving
  // forms that need to be parsed.
  std::unique_ptr<MockFieldClassificationModelHandler>
  CreateMockModelHandler() {
    auto handler =
        std::make_unique<MockFieldClassificationModelHandler>(&model_provider_);
    ON_CALL(*handler, GetModelPredictionsForForms)
        .WillByDefault(
            [](std::vector<std::unique_ptr<FormStructure>> forms,
               base::OnceCallback<void(
                   std::vector<std::unique_ptr<FormStructure>>)> callback) {
              std::move(callback).Run(std::move(forms));
            });
    return handler;
  }

  MockAutofillManagerObserver& observer() { return observer_; }

 private:
  optimization_guide::TestOptimizationGuideModelProvider model_provider_;
  MockAutofillManagerObserver observer_;
  base::ScopedObservation<AutofillManager, MockAutofillManagerObserver>
      observation_{&observer_};
};

TEST_F(AutofillManagerTestForModelPredictions,
       GetMlModelPredictionsForForm_AutofillOnly) {
  client_.set_autofill_ml_prediction_model_handler(CreateMockModelHandler());

  EXPECT_CALL(static_cast<MockFieldClassificationModelHandler&>(
                  *client_.GetAutofillFieldClassificationModelHandler()),
              GetModelPredictionsForForms);
  // Check that AutofillManager's observer is notified after parsing using
  // models is done.
  EXPECT_CALL(observer(), OnFieldTypesDetermined);
  FormData form = test::CreateTestAddressFormData();
  OnFormsSeenWithExpectations(manager(), /*updated_forms=*/{form},
                              /*removed_forms=*/{}, /*expectation=*/{form});
}

TEST_F(AutofillManagerTestForModelPredictions,
       GetMlModelPredictionsForForm_PasswordManagerOnly) {
  client_.set_password_ml_prediction_model_handler(CreateMockModelHandler());

  EXPECT_CALL(static_cast<MockFieldClassificationModelHandler&>(
                  *client_.GetPasswordManagerFieldClassificationModelHandler()),
              GetModelPredictionsForForms);
  // Check that AutofillManager's observer is notified after parsing using
  // models is done.
  EXPECT_CALL(observer(), OnFieldTypesDetermined);
  FormData form = test::CreateTestAddressFormData();
  OnFormsSeenWithExpectations(manager(), /*updated_forms=*/{form},
                              /*removed_forms=*/{}, /*expectation=*/{form});
}

TEST_F(AutofillManagerTestForModelPredictions,
       GetMlModelPredictionsForForm_BothAutofillAndPasswordManager) {
  client_.set_autofill_ml_prediction_model_handler(CreateMockModelHandler());
  client_.set_password_ml_prediction_model_handler(CreateMockModelHandler());

  EXPECT_CALL(static_cast<MockFieldClassificationModelHandler&>(
                  *client_.GetAutofillFieldClassificationModelHandler()),
              GetModelPredictionsForForms);
  EXPECT_CALL(static_cast<MockFieldClassificationModelHandler&>(
                  *client_.GetPasswordManagerFieldClassificationModelHandler()),
              GetModelPredictionsForForms);
  // Check that AutofillManager's observer is notified after parsing using
  // models is done.
  EXPECT_CALL(observer(), OnFieldTypesDetermined);
  FormData form = test::CreateTestAddressFormData();
  OnFormsSeenWithExpectations(manager(), /*updated_forms=*/{form},
                              /*removed_forms=*/{}, /*expectation=*/{form});
}
#endif

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnFormsSeen_SuccessfulQueryRequest_NotifiesBeforeLoadedServerPredictionsObserver) {
  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(Ref(manager())));
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(manager()), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete));
  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [](const auto&, const auto&,
             base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
            std::move(callback).Run(QueryResponse("", {}));
            return true;
          });
  EXPECT_CALL(observer_, OnAfterLoadedServerPredictions(Ref(manager())));
  OnFormsSeenWithExpectations(manager(), forms, {}, forms);
  task_environment_.RunUntilIdle();
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnFormsSeen_FailedQueryRequest_NotifiesBothLoadedServerPredictionsObservers) {
  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(Ref(manager())));
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(manager()), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete));
  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [](const auto&, const auto&,
             base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
            std::move(callback).Run(std::nullopt);
            return true;
          });
  EXPECT_CALL(observer_, OnAfterLoadedServerPredictions(Ref(manager())));
  OnFormsSeenWithExpectations(manager(), forms, {}, forms);
  task_environment_.RunUntilIdle();
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnLoadedServerPredictions_EmptyQueriedFormSignatures_NotifiesAfterLoadedServerPredictionsObserver) {
  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(Ref(manager())));
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(manager()), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete));
  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [&](const auto&, const auto&,
              base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
            std::move(callback).Run(QueryResponse("", {}));
            return true;
          });
  EXPECT_CALL(observer_, OnAfterLoadedServerPredictions(Ref(manager())));
  OnFormsSeenWithExpectations(manager(), forms, {}, forms);
  task_environment_.RunUntilIdle();
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnLoadedServerPredictions_NonEmptyQueriedFormSignatures_NotifiesAfterLoadedServerPredictionsObserver) {
  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(Ref(manager())));
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(manager()), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete));
  EXPECT_CALL(observer_,
              OnFieldTypesDetermined(Ref(manager()), forms[0].global_id(),
                                     FieldTypeSource::kAutofillServer));
  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [&](const auto&, const auto&,
              base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
            std::move(callback).Run(
                QueryResponse("", {manager()
                                       .FindCachedFormById(forms[0].global_id())
                                       ->form_signature()}));
            return true;
          });
  EXPECT_CALL(observer_, OnAfterLoadedServerPredictions(Ref(manager())));
  OnFormsSeenWithExpectations(manager(), forms, {}, forms);
  task_environment_.RunUntilIdle();
}

TEST_F(AutofillManagerTest, GetHeursticPredictionForForm) {
  FormData seen_form = test::CreateTestAddressFormData();
  OnFormsSeenWithExpectations(manager(), {seen_form}, {}, {seen_form});

  FormData unseen_form = test::CreateTestAddressFormData();
  ASSERT_NE(seen_form.global_id(), unseen_form.global_id());

  // Check that predictions are returned for the form that was seen.
  EXPECT_EQ(manager()
                .GetHeursticPredictionForForm(
                    autofill::HeuristicSource::kPasswordManagerMachineLearning,
                    seen_form.global_id(),
                    base::ToVector(seen_form.fields(),
                                   &autofill::FormFieldData::global_id))
                .size(),
            seen_form.fields().size());

  // Check that no predictions are returned for the unseen form.
  EXPECT_TRUE(
      manager()
          .GetHeursticPredictionForForm(
              autofill::HeuristicSource::kPasswordManagerMachineLearning,
              unseen_form.global_id(),
              base::ToVector(unseen_form.fields(),
                             &autofill::FormFieldData::global_id))
          .empty());
}

}  // namespace
}  // namespace autofill
