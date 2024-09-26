// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_value_store.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/prefs/pref_notifier.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::_;

namespace {

// Allows to capture pref notifications through gmock.
class MockPrefNotifier : public PrefNotifier {
 public:
  MOCK_METHOD1(OnPreferenceChanged, void(const std::string&));
  MOCK_METHOD1(OnInitializationCompleted, void(bool));
};

}  // namespace

// Names of the preferences used in this test.
namespace prefs {
const char kManagedPref[] = "this.pref.managed";
const char kSupervisedUserPref[] = "this.pref.supervised_user";
const char kCommandLinePref[] = "this.pref.command_line";
const char kExtensionPref[] = "this.pref.extension";
const char kStandaloneBrowserPref[] = "this.pref.standalone_browser";
const char kUserPref[] = "this.pref.user";
const char kRecommendedPref[] = "this.pref.recommended";
const char kDefaultPref[] = "this.pref.default";
const char kMissingPref[] = "this.pref.does_not_exist";
}

// Potentially expected values of all preferences used in this test program.
namespace managed_pref {
const char kManagedValue[] = "managed:managed";
}

namespace supervised_user_pref {
const char kManagedValue[] = "supervised_user:managed";
const char kSupervisedUserValue[] = "supervised_user:supervised_user";
}

namespace extension_pref {
const char kManagedValue[] = "extension:managed";
const char kSupervisedUserValue[] = "extension:supervised_user";
const char kExtensionValue[] = "extension:extension";
}

namespace standalone_browser_pref {
const char kManagedValue[] = "standalone_browser:managed";
const char kSupervisedUserValue[] = "standalone_browser:supervised_user";
const char kExtensionValue[] = "standalone_browser:extension";
const char kStandaloneBrowserValue[] = "standalone_browser:standalone_browser";
}  // namespace standalone_browser_pref

namespace command_line_pref {
const char kManagedValue[] = "command_line:managed";
const char kSupervisedUserValue[] = "command_line:supervised_user";
const char kExtensionValue[] = "command_line:extension";
const char kStandaloneBrowserValue[] = "command_line:standalone_browser";
const char kCommandLineValue[] = "command_line:command_line";
}

namespace user_pref {
const char kManagedValue[] = "user:managed";
const char kSupervisedUserValue[] = "supervised_user:supervised_user";
const char kExtensionValue[] = "user:extension";
const char kStandaloneBrowserValue[] = "user:standalone_browser";
const char kCommandLineValue[] = "user:command_line";
const char kUserValue[] = "user:user";
}

namespace recommended_pref {
const char kManagedValue[] = "recommended:managed";
const char kSupervisedUserValue[] = "recommended:supervised_user";
const char kExtensionValue[] = "recommended:extension";
const char kStandaloneBrowserValue[] = "recommended:standalone_browser";
const char kCommandLineValue[] = "recommended:command_line";
const char kUserValue[] = "recommended:user";
const char kRecommendedValue[] = "recommended:recommended";
}

namespace default_pref {
const char kManagedValue[] = "default:managed";
const char kSupervisedUserValue[] = "default:supervised_user";
const char kExtensionValue[] = "default:extension";
const char kStandaloneBrowserValue[] = "default:standalone_browser";
const char kCommandLineValue[] = "default:command_line";
const char kUserValue[] = "default:user";
const char kRecommendedValue[] = "default:recommended";
const char kDefaultValue[] = "default:default";
}

class PrefValueStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    // Create TestingPrefStores.
    CreateManagedPrefs();
    CreateSupervisedUserPrefs();
    CreateExtensionPrefs();
    CreateStandaloneBrowserPrefs();
    CreateCommandLinePrefs();
    CreateUserPrefs();
    CreateRecommendedPrefs();
    CreateDefaultPrefs();

    // Create a fresh PrefValueStore.
    pref_value_store_ = std::make_unique<PrefValueStore>(
        managed_pref_store_.get(), supervised_user_pref_store_.get(),
        extension_pref_store_.get(), standalone_browser_pref_store_.get(),
        command_line_pref_store_.get(), user_pref_store_.get(),
        recommended_pref_store_.get(), default_pref_store_.get(),
        &pref_notifier_);
  }

  void CreateManagedPrefs() {
    managed_pref_store_ = new TestingPrefStore;
    managed_pref_store_->SetString(
        prefs::kManagedPref,
        managed_pref::kManagedValue);
  }

  void CreateSupervisedUserPrefs() {
    supervised_user_pref_store_ = new TestingPrefStore;
    supervised_user_pref_store_->SetString(
        prefs::kManagedPref,
        supervised_user_pref::kManagedValue);
    supervised_user_pref_store_->SetString(
        prefs::kSupervisedUserPref,
        supervised_user_pref::kSupervisedUserValue);
  }

  void CreateExtensionPrefs() {
    extension_pref_store_ = new TestingPrefStore;
    extension_pref_store_->SetString(
        prefs::kManagedPref,
        extension_pref::kManagedValue);
    extension_pref_store_->SetString(
        prefs::kSupervisedUserPref,
        extension_pref::kSupervisedUserValue);
    extension_pref_store_->SetString(
        prefs::kExtensionPref,
        extension_pref::kExtensionValue);
  }

  void CreateStandaloneBrowserPrefs() {
    standalone_browser_pref_store_ = new TestingPrefStore;
    standalone_browser_pref_store_->SetString(
        prefs::kManagedPref, standalone_browser_pref::kManagedValue);
    standalone_browser_pref_store_->SetString(
        prefs::kSupervisedUserPref,
        standalone_browser_pref::kSupervisedUserValue);
    standalone_browser_pref_store_->SetString(
        prefs::kExtensionPref, standalone_browser_pref::kExtensionValue);
    standalone_browser_pref_store_->SetString(
        prefs::kStandaloneBrowserPref,
        standalone_browser_pref::kStandaloneBrowserValue);
  }

  void CreateCommandLinePrefs() {
    command_line_pref_store_ = new TestingPrefStore;
    command_line_pref_store_->SetString(
        prefs::kManagedPref,
        command_line_pref::kManagedValue);
    command_line_pref_store_->SetString(
        prefs::kSupervisedUserPref,
        command_line_pref::kSupervisedUserValue);
    command_line_pref_store_->SetString(
        prefs::kExtensionPref,
        command_line_pref::kExtensionValue);
    command_line_pref_store_->SetString(
        prefs::kStandaloneBrowserPref,
        command_line_pref::kStandaloneBrowserValue);
    command_line_pref_store_->SetString(
        prefs::kCommandLinePref,
        command_line_pref::kCommandLineValue);
  }

  void CreateUserPrefs() {
    user_pref_store_ = new TestingPrefStore;
    user_pref_store_->SetString(
        prefs::kManagedPref,
        user_pref::kManagedValue);
    user_pref_store_->SetString(
        prefs::kSupervisedUserPref,
        user_pref::kSupervisedUserValue);
    user_pref_store_->SetString(
        prefs::kCommandLinePref,
        user_pref::kCommandLineValue);
    user_pref_store_->SetString(
        prefs::kExtensionPref,
        user_pref::kExtensionValue);
    user_pref_store_->SetString(prefs::kStandaloneBrowserPref,
                                user_pref::kStandaloneBrowserValue);
    user_pref_store_->SetString(
        prefs::kUserPref,
        user_pref::kUserValue);
  }

  void CreateRecommendedPrefs() {
    recommended_pref_store_ = new TestingPrefStore;
    recommended_pref_store_->SetString(
        prefs::kManagedPref,
        recommended_pref::kManagedValue);
    recommended_pref_store_->SetString(
        prefs::kSupervisedUserPref,
        recommended_pref::kSupervisedUserValue);
    recommended_pref_store_->SetString(
        prefs::kCommandLinePref,
        recommended_pref::kCommandLineValue);
    recommended_pref_store_->SetString(
        prefs::kExtensionPref,
        recommended_pref::kExtensionValue);
    recommended_pref_store_->SetString(
        prefs::kStandaloneBrowserPref,
        recommended_pref::kStandaloneBrowserValue);
    recommended_pref_store_->SetString(
        prefs::kUserPref,
        recommended_pref::kUserValue);
    recommended_pref_store_->SetString(
        prefs::kRecommendedPref,
        recommended_pref::kRecommendedValue);
  }

  void CreateDefaultPrefs() {
    default_pref_store_ = new TestingPrefStore;
    default_pref_store_->SetString(
        prefs::kSupervisedUserPref,
        default_pref::kSupervisedUserValue);
    default_pref_store_->SetString(
        prefs::kManagedPref,
        default_pref::kManagedValue);
    default_pref_store_->SetString(
        prefs::kCommandLinePref,
        default_pref::kCommandLineValue);
    default_pref_store_->SetString(
        prefs::kExtensionPref,
        default_pref::kExtensionValue);
    default_pref_store_->SetString(prefs::kStandaloneBrowserPref,
                                   default_pref::kStandaloneBrowserValue);
    default_pref_store_->SetString(
        prefs::kUserPref,
        default_pref::kUserValue);
    default_pref_store_->SetString(
        prefs::kRecommendedPref,
        default_pref::kRecommendedValue);
    default_pref_store_->SetString(
        prefs::kDefaultPref,
        default_pref::kDefaultValue);
  }

  void ExpectValueChangeNotifications(const std::string& name) {
    EXPECT_CALL(pref_notifier_, OnPreferenceChanged(name));
  }

  void CheckAndClearValueChangeNotifications() {
    Mock::VerifyAndClearExpectations(&pref_notifier_);
  }

  MockPrefNotifier pref_notifier_;
  std::unique_ptr<PrefValueStore> pref_value_store_;

  scoped_refptr<TestingPrefStore> managed_pref_store_;
  scoped_refptr<TestingPrefStore> supervised_user_pref_store_;
  scoped_refptr<TestingPrefStore> extension_pref_store_;
  scoped_refptr<TestingPrefStore> standalone_browser_pref_store_;
  scoped_refptr<TestingPrefStore> command_line_pref_store_;
  scoped_refptr<TestingPrefStore> user_pref_store_;
  scoped_refptr<TestingPrefStore> recommended_pref_store_;
  scoped_refptr<TestingPrefStore> default_pref_store_;
};

TEST_F(PrefValueStoreTest, GetValue) {
  const base::Value* value;

  // The following tests read a value from the PrefService. The preferences are
  // set in a way such that all lower-priority stores have a value and we can
  // test whether overrides work correctly.

  // Test getting a managed value.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kManagedPref,
                                          base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(managed_pref::kManagedValue, value->GetString());

  // Test getting a supervised user value.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kSupervisedUserPref,
                                          base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(supervised_user_pref::kSupervisedUserValue, value->GetString());

  // Test getting an extension value.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kExtensionPref,
                                          base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(extension_pref::kExtensionValue, value->GetString());

  // Test getting a command-line value.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kCommandLinePref,
                                          base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(command_line_pref::kCommandLineValue, value->GetString());

  // Test getting a user-set value.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kUserPref,
                                          base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(user_pref::kUserValue, value->GetString());

  // Test getting a user set value overwriting a recommended value.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kRecommendedPref,
                                          base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(recommended_pref::kRecommendedValue, value->GetString());

  // Test getting a default value.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kDefaultPref,
                                          base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(default_pref::kDefaultValue, value->GetString());

  // Test getting a preference value that the |PrefValueStore|
  // does not contain.
  base::Value tmp_dummy_value(true);
  value = &tmp_dummy_value;
  ASSERT_FALSE(pref_value_store_->GetValue(prefs::kMissingPref,
                                           base::Value::Type::STRING, &value));
  ASSERT_FALSE(value);
}

TEST_F(PrefValueStoreTest, GetRecommendedValue) {
  const base::Value* value;

  // The following tests read a value from the PrefService. The preferences are
  // set in a way such that all lower-priority stores have a value and we can
  // test whether overrides do not clutter the recommended value.

  // Test getting recommended value when a managed value is present.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetRecommendedValue(
      prefs::kManagedPref,
      base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(recommended_pref::kManagedValue, value->GetString());

  // Test getting recommended value when a supervised user value is present.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetRecommendedValue(
      prefs::kSupervisedUserPref,
      base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(recommended_pref::kSupervisedUserValue, value->GetString());

  // Test getting recommended value when an extension value is present.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetRecommendedValue(
      prefs::kExtensionPref,
      base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(recommended_pref::kExtensionValue, value->GetString());

  // Test getting recommended value when a command-line value is present.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetRecommendedValue(
      prefs::kCommandLinePref,
      base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(recommended_pref::kCommandLineValue, value->GetString());

  // Test getting recommended value when a user-set value is present.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetRecommendedValue(
      prefs::kUserPref,
      base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(recommended_pref::kUserValue, value->GetString());

  // Test getting recommended value when no higher-priority value is present.
  value = nullptr;
  ASSERT_TRUE(pref_value_store_->GetRecommendedValue(
      prefs::kRecommendedPref,
      base::Value::Type::STRING, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(recommended_pref::kRecommendedValue, value->GetString());

  // Test getting recommended value when no recommended value is present.
  base::Value tmp_dummy_value(true);
  value = &tmp_dummy_value;
  ASSERT_FALSE(pref_value_store_->GetRecommendedValue(
      prefs::kDefaultPref,
      base::Value::Type::STRING, &value));
  ASSERT_FALSE(value);

  // Test getting a preference value that the |PrefValueStore|
  // does not contain.
  value = &tmp_dummy_value;
  ASSERT_FALSE(pref_value_store_->GetRecommendedValue(
      prefs::kMissingPref,
      base::Value::Type::STRING, &value));
  ASSERT_FALSE(value);
}

TEST_F(PrefValueStoreTest, PrefChanges) {
  // Check pref controlled by highest-priority store.
  ExpectValueChangeNotifications(prefs::kManagedPref);
  managed_pref_store_->NotifyPrefValueChanged(prefs::kManagedPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kManagedPref);
  supervised_user_pref_store_->NotifyPrefValueChanged(prefs::kManagedPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kManagedPref);
  extension_pref_store_->NotifyPrefValueChanged(prefs::kManagedPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kManagedPref);
  command_line_pref_store_->NotifyPrefValueChanged(prefs::kManagedPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kManagedPref);
  user_pref_store_->NotifyPrefValueChanged(prefs::kManagedPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kManagedPref);
  recommended_pref_store_->NotifyPrefValueChanged(prefs::kManagedPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kManagedPref);
  default_pref_store_->NotifyPrefValueChanged(prefs::kManagedPref);
  CheckAndClearValueChangeNotifications();

  // Check pref controlled by user store.
  ExpectValueChangeNotifications(prefs::kUserPref);
  managed_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kUserPref);
  extension_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kUserPref);
  command_line_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kUserPref);
  user_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kUserPref);
  recommended_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kUserPref);
  default_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  CheckAndClearValueChangeNotifications();

  // Check pref controlled by default-pref store.
  ExpectValueChangeNotifications(prefs::kDefaultPref);
  managed_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kDefaultPref);
  extension_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kDefaultPref);
  command_line_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kDefaultPref);
  user_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kDefaultPref);
  recommended_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  CheckAndClearValueChangeNotifications();

  ExpectValueChangeNotifications(prefs::kDefaultPref);
  default_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  CheckAndClearValueChangeNotifications();
}

TEST_F(PrefValueStoreTest, OnInitializationCompleted) {
  EXPECT_CALL(pref_notifier_, OnInitializationCompleted(true)).Times(0);
  managed_pref_store_->SetInitializationCompleted();
  supervised_user_pref_store_->SetInitializationCompleted();
  extension_pref_store_->SetInitializationCompleted();
  standalone_browser_pref_store_->SetInitializationCompleted();
  command_line_pref_store_->SetInitializationCompleted();
  recommended_pref_store_->SetInitializationCompleted();
  default_pref_store_->SetInitializationCompleted();
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  // The notification should only be triggered after the last store is done.
  EXPECT_CALL(pref_notifier_, OnInitializationCompleted(true)).Times(1);
  user_pref_store_->SetInitializationCompleted();
  Mock::VerifyAndClearExpectations(&pref_notifier_);
}

TEST_F(PrefValueStoreTest, PrefValueInManagedStore) {
  EXPECT_TRUE(pref_value_store_->PrefValueInManagedStore(
      prefs::kManagedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kSupervisedUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kStandaloneBrowserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kCommandLinePref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueInExtensionStore) {
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kManagedPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kSupervisedUserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kStandaloneBrowserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kCommandLinePref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueInUserStore) {
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kManagedPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kSupervisedUserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kExtensionPref));
  EXPECT_TRUE(
      pref_value_store_->PrefValueInUserStore(prefs::kStandaloneBrowserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kCommandLinePref));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueFromExtensionStore) {
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kManagedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kSupervisedUserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kStandaloneBrowserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kCommandLinePref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueFromUserStore) {
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kManagedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kSupervisedUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(
      pref_value_store_->PrefValueFromUserStore(prefs::kStandaloneBrowserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kCommandLinePref));
  EXPECT_TRUE(pref_value_store_->PrefValueFromUserStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueFromRecommendedStore) {
  EXPECT_FALSE(pref_value_store_->PrefValueFromRecommendedStore(
      prefs::kManagedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromRecommendedStore(
      prefs::kSupervisedUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromRecommendedStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromRecommendedStore(
      prefs::kStandaloneBrowserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromRecommendedStore(
      prefs::kCommandLinePref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromRecommendedStore(
      prefs::kUserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueFromRecommendedStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromRecommendedStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromRecommendedStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueFromDefaultStore) {
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kManagedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kSupervisedUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kStandaloneBrowserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kCommandLinePref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kRecommendedPref));
  EXPECT_TRUE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueUserModifiable) {
  EXPECT_FALSE(pref_value_store_->PrefValueUserModifiable(
      prefs::kManagedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueUserModifiable(
      prefs::kSupervisedUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueUserModifiable(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueUserModifiable(
      prefs::kStandaloneBrowserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueUserModifiable(
      prefs::kCommandLinePref));
  EXPECT_TRUE(pref_value_store_->PrefValueUserModifiable(
      prefs::kUserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueUserModifiable(
      prefs::kRecommendedPref));
  EXPECT_TRUE(pref_value_store_->PrefValueUserModifiable(
      prefs::kDefaultPref));
  EXPECT_TRUE(pref_value_store_->PrefValueUserModifiable(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueExtensionModifiable) {
  EXPECT_FALSE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kManagedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kSupervisedUserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kExtensionPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kStandaloneBrowserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kCommandLinePref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kUserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kRecommendedPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kDefaultPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kMissingPref));
}
