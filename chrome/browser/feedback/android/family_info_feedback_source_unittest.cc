// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/android/family_info_feedback_source.h"

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/test/test_support_jni_headers/FamilyInfoFeedbackSourceTestBridge_jni.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace chrome::android {
namespace {

const char kTestEmail[] = "test@gmail.com";
const char kFeedbackTagFamilyMemberRole[] = "Family_Member_Role";
const char kFeedbackTagParentalControlSitesChild[] =
    "Parental_Control_Sites_Child";

kids_chrome_management::ListFamilyMembersResponse CreateFamilyWithOneMember(
    const std::string& gaia_id,
    kids_chrome_management::FamilyRole role) {
  kids_chrome_management::ListFamilyMembersResponse response;
  kids_chrome_management::FamilyMember* member = response.add_members();

  member->set_user_id(gaia_id);
  member->set_role(role);
  member->mutable_profile()->set_display_name("Name");
  member->mutable_profile()->set_email(kTestEmail);
  return response;
}
}  // namespace

// TODO(b/280772872): Integrate AsyncURLChecker to test
// supervised_user::SupervisedUserURLFilter::WebFilterType::kTryToBlockMatureSites.
class FamilyInfoFeedbackSourceForChildFilterBehaviorTest
    : public testing::TestWithParam<
          supervised_user::SupervisedUserURLFilter::FilteringBehavior> {
 public:
  FamilyInfoFeedbackSourceForChildFilterBehaviorTest()
      : env_(base::android::AttachCurrentThread()) {}

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    builder.SetIsSupervisedProfile();

    role_ = kids_chrome_management::CHILD;
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    j_feedback_source_ = CreateJavaObjectForTesting();
    supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  // Methods to access Java counterpart FamilyInfoFeedbackSource.
  std::string GetFeedbackValue(std::string feedback_tag) {
    const base::android::JavaRef<jstring>& j_value =
        Java_FamilyInfoFeedbackSourceTestBridge_getValue(
            env_,
            base::android::JavaParamRef<jobject>(env_,
                                                 j_feedback_source_.obj()),
            base::android::ConvertUTF8ToJavaString(env_, feedback_tag));
    return base::android::ConvertJavaStringToUTF8(env_, j_value);
  }

  void OnListFamilyMembersSuccess(
      base::WeakPtr<FamilyInfoFeedbackSource> feedback_source,
      const kids_chrome_management::ListFamilyMembersResponse& members) {
    feedback_source->OnSuccess(members);
  }

  // Creates a new instance of FamilyInfoFeedbackSource that is destroyed on
  // completion of OnGetFamilyMembers* methods.
  base::WeakPtr<FamilyInfoFeedbackSource> CreateFamilyInfoFeedbackSource() {
    FamilyInfoFeedbackSource* source = new FamilyInfoFeedbackSource(
        base::android::JavaParamRef<jobject>(env_, j_feedback_source_.obj()),
        profile_.get());
    return source->weak_factory_.GetWeakPtr();
  }

  kids_chrome_management::FamilyRole role_;
  raw_ptr<supervised_user::SupervisedUserService> supervised_user_service_;

 private:
  // Creates a Java instance of FamilyInfoFeedbackSource.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObjectForTesting() {
    ProfileAndroid* profile_android =
        ProfileAndroid::FromProfile(profile_.get());
    return Java_FamilyInfoFeedbackSourceTestBridge_createFamilyInfoFeedbackSource(
        env_, base::android::JavaParamRef<jobject>(
                  env_, profile_android->GetJavaObject().Release()));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::android::ScopedJavaLocalRef<jobject> j_feedback_source_;
  raw_ptr<JNIEnv> env_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
};

// Tests that the parental control sites value for a child user is recorded.
TEST_P(FamilyInfoFeedbackSourceForChildFilterBehaviorTest,
       GetChildFilteringBehaviour) {
  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

  supervised_user_service_->GetURLFilter()->SetDefaultFilteringBehavior(
      GetParam());

  kids_chrome_management::ListFamilyMembersResponse members =
      CreateFamilyWithOneMember(primary_account.gaia, role_);

  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnListFamilyMembersSuccess(feedback_source, members);

  std::string expected_feedback_value =
      GetFeedbackValue(kFeedbackTagParentalControlSitesChild);

  // Don't put logic in tests, test explicit values.
  switch (GetParam()) {
    case (supervised_user::SupervisedUserURLFilter::FilteringBehavior::BLOCK):
      EXPECT_EQ("allow_certain_sites", expected_feedback_value);
      break;
    case (supervised_user::SupervisedUserURLFilter::FilteringBehavior::ALLOW):
      EXPECT_EQ("allow_all_sites", expected_feedback_value);
      break;
    default:
      // Remaining combinations are not tested.
      NOTREACHED_NORETURN();
  }
}

INSTANTIATE_TEST_SUITE_P(
    FilterBehaviourContainer,
    FamilyInfoFeedbackSourceForChildFilterBehaviorTest,
    ::testing::Values(
        supervised_user::SupervisedUserURLFilter::FilteringBehavior::BLOCK,
        supervised_user::SupervisedUserURLFilter::FilteringBehavior::ALLOW));

class FamilyInfoFeedbackSourceTest
    : public testing::TestWithParam<kids_chrome_management::FamilyRole> {
 public:
  FamilyInfoFeedbackSourceTest() : env_(base::android::AttachCurrentThread()) {}

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));

    is_child_ = false;
    // Check if the test is parametrized and the parameter is a CHILD role.
    if (::testing::UnitTest::GetInstance()
            ->current_test_info()
            ->value_param()) {
      is_child_ = GetParam() == kids_chrome_management::CHILD;
      if (is_child_) {
        builder.SetIsSupervisedProfile();
      }
    }

    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    j_feedback_source_ = CreateJavaObjectForTesting();
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  // Methods to access Java counterpart FamilyInfoFeedbackSource.
  std::string GetFeedbackValue() {
    const base::android::JavaRef<jstring>& j_value =
        Java_FamilyInfoFeedbackSourceTestBridge_getValue(
            env_,
            base::android::JavaParamRef<jobject>(env_,
                                                 j_feedback_source_.obj()),
            base::android::ConvertUTF8ToJavaString(
                env_, kFeedbackTagFamilyMemberRole));
    return base::android::ConvertJavaStringToUTF8(env_, j_value);
  }

  void OnListFamilyMembersSuccess(
      base::WeakPtr<FamilyInfoFeedbackSource> feedback_source,
      const kids_chrome_management::ListFamilyMembersResponse& members) {
    feedback_source->OnSuccess(members);
  }

  void OnGetFamilyMembersFailure(
      base::WeakPtr<FamilyInfoFeedbackSource> feedback_source) {
    feedback_source->OnFailure(
        supervised_user::ProtoFetcherStatus::GoogleServiceAuthError(
            GoogleServiceAuthError(
                GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS)));
  }

  // Creates a new instance of FamilyInfoFeedbackSource that is destroyed on
  // completion of OnGetFamilyMembers* methods.
  base::WeakPtr<FamilyInfoFeedbackSource> CreateFamilyInfoFeedbackSource() {
    FamilyInfoFeedbackSource* source = new FamilyInfoFeedbackSource(
        base::android::JavaParamRef<jobject>(env_, j_feedback_source_.obj()),
        profile_.get());
    return source->weak_factory_.GetWeakPtr();
  }

  Profile* profile() const { return profile_.get(); }

  bool is_child() const { return is_child_; }

 private:
  // Creates a Java instance of FamilyInfoFeedbackSource.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObjectForTesting() {
    ProfileAndroid* profile_android =
        ProfileAndroid::FromProfile(profile_.get());
    return Java_FamilyInfoFeedbackSourceTestBridge_createFamilyInfoFeedbackSource(
        env_, base::android::JavaParamRef<jobject>(
                  env_, profile_android->GetJavaObject().Release()));
  }

  content::BrowserTaskEnvironment task_environment_;

  base::android::ScopedJavaLocalRef<jobject> j_feedback_source_;
  raw_ptr<JNIEnv> env_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
  bool is_child_;
};

// Tests that the family role for a user in a Family Group is recorded.
TEST_P(FamilyInfoFeedbackSourceTest, GetFamilyMembersSignedIn) {
  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

  kids_chrome_management::FamilyRole role = GetParam();
  kids_chrome_management::ListFamilyMembersResponse members =
      CreateFamilyWithOneMember(primary_account.gaia, role);

  if (is_child()) {
    supervised_user::SupervisedUserService* supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(profile());
    // Set some filtering behavior for the user, as ListFamilyMembers
    // will try to obtain this along with the family role (and crush otherwise).
    supervised_user_service_->GetURLFilter()->SetDefaultFilteringBehavior(
        supervised_user::SupervisedUserURLFilter::FilteringBehavior::ALLOW);
  }

  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnListFamilyMembersSuccess(feedback_source, members);

  // Don't put logic in tests, test explicit values.
  switch (role) {
    case kids_chrome_management::HEAD_OF_HOUSEHOLD:
      EXPECT_EQ("family_manager", GetFeedbackValue());
      break;
    case kids_chrome_management::PARENT:
      EXPECT_EQ("parent", GetFeedbackValue());
      break;
    case kids_chrome_management::MEMBER:
      EXPECT_EQ("member", GetFeedbackValue());
      break;
    case kids_chrome_management::CHILD:
      EXPECT_EQ("child", GetFeedbackValue());
      break;
    default:
      NOTREACHED_NORETURN();
  }
}

// Tests that a user that is not in a Family group is not processed.
TEST_F(FamilyInfoFeedbackSourceTest, GetFamilyMembersSignedInNoFamily) {
  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  kids_chrome_management::ListFamilyMembersResponse members;
  OnListFamilyMembersSuccess(feedback_source, members);

  EXPECT_EQ("", GetFeedbackValue());
}

// Tests that a signed-in user that fails its request to the server is not
// processed.
TEST_F(FamilyInfoFeedbackSourceTest, GetFamilyMembersOnFailure) {
  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnGetFamilyMembersFailure(feedback_source);

  EXPECT_EQ("", GetFeedbackValue());
}

TEST_F(FamilyInfoFeedbackSourceTest, FeedbackSourceDestroyedOnCompletion) {
  kids_chrome_management::ListFamilyMembersResponse members;
  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnListFamilyMembersSuccess(feedback_source, members);

  EXPECT_TRUE(feedback_source.WasInvalidated());
}

TEST_F(FamilyInfoFeedbackSourceTest, FeedbackSourceDestroyedOnFailure) {
  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnGetFamilyMembersFailure(feedback_source);

  EXPECT_TRUE(feedback_source.WasInvalidated());
}

INSTANTIATE_TEST_SUITE_P(
    AllFamilyMemberRoles,
    FamilyInfoFeedbackSourceTest,
    ::testing::Values(kids_chrome_management::HEAD_OF_HOUSEHOLD,
                      kids_chrome_management::CHILD,
                      kids_chrome_management::MEMBER,
                      kids_chrome_management::PARENT));

}  // namespace chrome::android
