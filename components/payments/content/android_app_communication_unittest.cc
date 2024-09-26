// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "components/payments/content/android_app_communication_test_support.h"
#include "components/payments/core/android_app_description.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace payments {
namespace {

std::vector<std::unique_ptr<AndroidAppDescription>> createApp(
    const std::vector<std::string>& activity_names,
    const std::string& default_payment_method,
    const std::vector<std::string>& service_names) {
  auto app = std::make_unique<AndroidAppDescription>();

  for (const auto& activity_name : activity_names) {
    auto activity = std::make_unique<AndroidActivityDescription>();
    activity->name = activity_name;
    activity->default_payment_method = default_payment_method;
    app->activities.emplace_back(std::move(activity));
  }

  app->package = "com.example.app";
  app->service_names = service_names;

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::move(app));

  return apps;
}

class AndroidAppCommunicationTest : public testing::Test {
 public:
  AndroidAppCommunicationTest()
      : support_(AndroidAppCommunicationTestSupport::Create()),
        web_contents_(
            web_contents_factory_.CreateWebContents(support_->context())) {}
  ~AndroidAppCommunicationTest() override = default;

  AndroidAppCommunicationTest(const AndroidAppCommunicationTest& other) =
      delete;
  AndroidAppCommunicationTest& operator=(
      const AndroidAppCommunicationTest& other) = delete;

  void OnGetAppDescriptionsResponse(
      const absl::optional<std::string>& error,
      std::vector<std::unique_ptr<AndroidAppDescription>> apps) {
    error_ = error;
    apps_ = std::move(apps);
  }

  void OnIsReadyToPayResponse(const absl::optional<std::string>& error,
                              bool is_ready_to_pay) {
    error_ = error;
    is_ready_to_pay_ = is_ready_to_pay;
  }

  void OnPaymentAppResponse(const absl::optional<std::string>& error,
                            bool is_activity_result_ok,
                            const std::string& payment_method_identifier,
                            const std::string& stringified_details) {
    error_ = error;
    is_activity_result_ok_ = is_activity_result_ok;
    payment_method_identifier_ = payment_method_identifier;
    stringified_details_ = stringified_details;
  }

  std::unique_ptr<AndroidAppCommunicationTestSupport> support_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  absl::optional<std::string> error_;
  std::vector<std::unique_ptr<AndroidAppDescription>> apps_;
  bool is_ready_to_pay_ = false;
  bool is_activity_result_ok_ = false;
  std::string payment_method_identifier_;
  std::string stringified_details_;
};

TEST_F(AndroidAppCommunicationTest, OneInstancePerBrowserContext) {
  auto communication_one =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  auto communication_two =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  EXPECT_EQ(communication_one.get(), communication_two.get());
}

TEST_F(AndroidAppCommunicationTest, NoArcForGetAppDescriptions) {
  // Intentionally do not set an instance.

  support_->ExpectNoListOfPaymentAppsQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  } else {
    EXPECT_FALSE(error_.has_value());
  }

  EXPECT_TRUE(apps_.empty());
}

TEST_F(AndroidAppCommunicationTest, NoAppDescriptions) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond({});

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(error_.has_value());
  EXPECT_TRUE(apps_.empty());
}

TEST_F(AndroidAppCommunicationTest, TwoActivitiesInPackage) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(
      createApp({"com.example.app.ActivityOne", "com.example.app.ActivityTwo"},
                "https://play.google.com/billing", {}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ(
        "Found more than one PAY activity in the Trusted Web Activity, but at "
        "most one activity is supported.",
        error_.value());
  } else {
    EXPECT_FALSE(error_.has_value());
  }
  EXPECT_TRUE(apps_.empty());
}

TEST_F(AndroidAppCommunicationTest, TwoServicesInPackage) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(
      createApp({"com.example.app.Activity"}, "https://play.google.com/billing",
                {"com.example.app.ServiceOne", "com.example.app.ServiceTwo"}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(error_.has_value());
  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_EQ(1u, apps_.size());
    ASSERT_NE(nullptr, apps_.front().get());
    EXPECT_EQ("com.example.app", apps_.front()->package);

    // The logic for checking for multiple services is cross-platform in
    // android_payment_app_factory.cc, so the platform-specific implementations
    // of android_app_communication.h do not check for this error condition.
    std::vector<std::string> expected_service_names = {
        "com.example.app.ServiceOne", "com.example.app.ServiceTwo"};
    EXPECT_EQ(expected_service_names, apps_.front()->service_names);

    ASSERT_EQ(1u, apps_.front()->activities.size());
    ASSERT_NE(nullptr, apps_.front()->activities.front().get());
    EXPECT_EQ("com.example.app.Activity",
              apps_.front()->activities.front()->name);
    EXPECT_EQ("https://play.google.com/billing",
              apps_.front()->activities.front()->default_payment_method);
  } else {
    EXPECT_TRUE(apps_.empty());
  }
}

TEST_F(AndroidAppCommunicationTest, ActivityAndService) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(
      createApp({"com.example.app.Activity"}, "https://play.google.com/billing",
                {"com.example.app.Service"}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(error_.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_EQ(1u, apps_.size());
    ASSERT_NE(nullptr, apps_.front().get());
    EXPECT_EQ("com.example.app", apps_.front()->package);
    EXPECT_EQ(std::vector<std::string>{"com.example.app.Service"},
              apps_.front()->service_names);
    ASSERT_EQ(1u, apps_.front()->activities.size());
    ASSERT_NE(nullptr, apps_.front()->activities.front().get());
    EXPECT_EQ("com.example.app.Activity",
              apps_.front()->activities.front()->name);
    EXPECT_EQ("https://play.google.com/billing",
              apps_.front()->activities.front()->default_payment_method);
  } else {
    EXPECT_TRUE(apps_.empty());
  }
}

TEST_F(AndroidAppCommunicationTest, OnlyActivity) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(createApp(
      {"com.example.app.Activity"}, "https://play.google.com/billing", {}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      "com.example.app",
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(error_.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_EQ(1u, apps_.size());
    ASSERT_NE(nullptr, apps_.front().get());
    EXPECT_EQ("com.example.app", apps_.front()->package);
    EXPECT_TRUE(apps_.front()->service_names.empty());
    ASSERT_EQ(1u, apps_.front()->activities.size());
    ASSERT_NE(nullptr, apps_.front()->activities.front().get());
    EXPECT_EQ("com.example.app.Activity",
              apps_.front()->activities.front()->name);
    EXPECT_EQ("https://play.google.com/billing",
              apps_.front()->activities.front()->default_payment_method);
  } else {
    EXPECT_TRUE(apps_.empty());
  }
}

TEST_F(AndroidAppCommunicationTest, OutsideOfTwa) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoListOfPaymentAppsQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();
  communication->GetAppDescriptions(
      /*twa_package_name=*/"",  // Empty string means this is not TWA.
      base::BindOnce(&AndroidAppCommunicationTest::OnGetAppDescriptionsResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(error_.has_value());
  EXPECT_TRUE(apps_.empty());
}

TEST_F(AndroidAppCommunicationTest, NoArcForIsReadyToPay) {
  // Intentionally do not set an instance.

  support_->ExpectNoIsReadyToPayQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  EXPECT_FALSE(is_ready_to_pay_);
}

TEST_F(AndroidAppCommunicationTest, TwaIsReadyToPayOnlyWithPlayBilling) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoIsReadyToPayQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://example.test"].insert("{}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }

  EXPECT_FALSE(is_ready_to_pay_);
}

TEST_F(AndroidAppCommunicationTest, MoreThanOnePaymentMethodDataNotReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoIsReadyToPayQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"1\"}");
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"2\"}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  ASSERT_TRUE(error_.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_EQ("At most one payment method specific data is supported.",
              error_.value());
  } else {
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }

  EXPECT_FALSE(is_ready_to_pay_);
}

TEST_F(AndroidAppCommunicationTest, EmptyMethodDataIsReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryIsReadyToPayAndRespond(true);

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data.insert(std::make_pair(
      "https://play.google.com/billing", std::set<std::string>()));
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
    EXPECT_TRUE(is_ready_to_pay_);
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
    EXPECT_FALSE(is_ready_to_pay_);
  }
}

TEST_F(AndroidAppCommunicationTest, NotReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryIsReadyToPayAndRespond(false);

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }

  EXPECT_FALSE(is_ready_to_pay_);
}

TEST_F(AndroidAppCommunicationTest, ReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryIsReadyToPayAndRespond(true);

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");
  communication->IsReadyToPay(
      "com.example.app", "com.example.app.Service", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::BindOnce(&AndroidAppCommunicationTest::OnIsReadyToPayResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
    EXPECT_TRUE(is_ready_to_pay_);
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
    EXPECT_FALSE(is_ready_to_pay_);
  }
}

TEST_F(AndroidAppCommunicationTest, NoArcForInvokePaymentApp) {
  // Intentionally do not set an instance.

  support_->ExpectNoPaymentAppInvoke();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_,
      base::BindOnce(&AndroidAppCommunicationTest::OnPaymentAppResponse,
                     base::Unretained(this)));

  EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  EXPECT_FALSE(is_activity_result_ok_);
  EXPECT_TRUE(payment_method_identifier_.empty());
  EXPECT_EQ("{}", stringified_details_);
}

TEST_F(AndroidAppCommunicationTest, TwaPaymentOnlyWithPlayBilling) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoPaymentAppInvoke();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://example.test"].insert("{}");
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_,
      base::BindOnce(&AndroidAppCommunicationTest::OnPaymentAppResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
    EXPECT_FALSE(is_activity_result_ok_);
    EXPECT_TRUE(payment_method_identifier_.empty());
    EXPECT_EQ("{}", stringified_details_);
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }
}

TEST_F(AndroidAppCommunicationTest, NoPaymentWithMoreThanOnePaymentMethodData) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoPaymentAppInvoke();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"1\"}");
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"2\"}");
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_,
      base::BindOnce(&AndroidAppCommunicationTest::OnPaymentAppResponse,
                     base::Unretained(this)));

  EXPECT_FALSE(is_activity_result_ok_);
  EXPECT_EQ("{}", stringified_details_);
  EXPECT_TRUE(payment_method_identifier_.empty());
  ASSERT_TRUE(error_.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_EQ("At most one payment method specific data is supported.",
              error_.value());
  } else {
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }
}

TEST_F(AndroidAppCommunicationTest, PaymentWithEmptyMethodData) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectInvokePaymentAppAndRespond(
      /*is_activity_result_ok=*/true,
      /*payment_method_identifier=*/"https://play.google.com/billing",
      /*stringified_details*/ "{\"status\": \"ok\"}");

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data.insert(std::make_pair(
      "https://play.google.com/billing", std::set<std::string>()));
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_,
      base::BindOnce(&AndroidAppCommunicationTest::OnPaymentAppResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
    EXPECT_TRUE(is_activity_result_ok_);
    EXPECT_EQ("https://play.google.com/billing", payment_method_identifier_);
    EXPECT_EQ("{\"status\": \"ok\"}", stringified_details_);
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }
}

TEST_F(AndroidAppCommunicationTest, UserCancelInvokePaymentApp) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectInvokePaymentAppAndRespond(
      /*is_activity_result_ok=*/false,
      /*payment_method_identifier=*/"https://play.google.com/billing",
      /*stringified_details*/ "{}");

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_,
      base::BindOnce(&AndroidAppCommunicationTest::OnPaymentAppResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
    EXPECT_FALSE(is_activity_result_ok_);
    EXPECT_EQ("https://play.google.com/billing", payment_method_identifier_);
    EXPECT_EQ("{}", stringified_details_);
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }
}

TEST_F(AndroidAppCommunicationTest, UserConfirmInvokePaymentApp) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectInvokePaymentAppAndRespond(
      /*is_activity_result_ok=*/true,
      /*payment_method_identifier=*/"https://play.google.com/billing",
      /*stringified_details*/ "{\"status\": \"ok\"}");

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_,
      base::BindOnce(&AndroidAppCommunicationTest::OnPaymentAppResponse,
                     base::Unretained(this)));

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error_.has_value());
    EXPECT_TRUE(is_activity_result_ok_);
    EXPECT_EQ("https://play.google.com/billing", payment_method_identifier_);
    EXPECT_EQ("{\"status\": \"ok\"}", stringified_details_);
  } else {
    ASSERT_TRUE(error_.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error_.value());
  }
}

}  // namespace
}  // namespace payments
