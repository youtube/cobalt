// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_mac.h"

#include <CoreFoundation/CoreFoundation.h>

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/preferences_mock_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ScopedCFTypeRef;

namespace policy {

namespace {

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  TestHarness(const TestHarness&) = delete;
  TestHarness& operator=(const TestHarness&) = delete;
  ~TestHarness() override;

  void SetUp() override;

  ConfigurationPolicyProvider* CreateProvider(
      SchemaRegistry* registry,
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;

  void InstallEmptyPolicy() override;
  void InstallStringPolicy(const std::string& policy_name,
                           const std::string& policy_value) override;
  void InstallIntegerPolicy(const std::string& policy_name,
                            int policy_value) override;
  void InstallBooleanPolicy(const std::string& policy_name,
                            bool policy_value) override;
  void InstallStringListPolicy(const std::string& policy_name,
                               const base::Value::List& policy_value) override;
  void InstallDictionaryPolicy(const std::string& policy_name,
                               const base::Value::Dict& policy_value) override;

  static PolicyProviderTestHarness* Create();

 private:
  raw_ptr<MockPreferences> prefs_;
};

TestHarness::TestHarness()
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY,
                                POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_PLATFORM) {}

TestHarness::~TestHarness() {}

void TestHarness::SetUp() {}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  prefs_ = new MockPreferences();
  std::unique_ptr<AsyncPolicyLoader> loader(
      new PolicyLoaderMac(task_runner, base::FilePath(), prefs_));
  return new AsyncPolicyProvider(registry, std::move(loader));
}

void TestHarness::InstallEmptyPolicy() {}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  ScopedCFTypeRef<CFStringRef> value(base::SysUTF8ToCFStringRef(policy_value));
  prefs_->AddTestItem(name, value, /*is_forced=*/true, /*is_machine=*/true);
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  ScopedCFTypeRef<CFNumberRef> value(
      CFNumberCreate(NULL, kCFNumberIntType, &policy_value));
  prefs_->AddTestItem(name, value, /*is_forced=*/true, /*is_machine=*/true);
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  prefs_->AddTestItem(name, policy_value ? kCFBooleanTrue : kCFBooleanFalse,
                      /*is_forced=*/true, /*is_machine=*/true);
}

void TestHarness::InstallStringListPolicy(
    const std::string& policy_name,
    const base::Value::List& policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  ScopedCFTypeRef<CFPropertyListRef> array(
      ValueToProperty(base::Value(policy_value.Clone())));
  ASSERT_TRUE(array);
  prefs_->AddTestItem(name, array, /*is_forced=*/true, /*is_machine=*/true);
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::Value::Dict& policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  ScopedCFTypeRef<CFPropertyListRef> dict(
      ValueToProperty(base::Value(policy_value.Clone())));
  ASSERT_TRUE(dict);
  prefs_->AddTestItem(name, dict, /*is_forced=*/true, /*is_machine=*/true);
}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness();
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_SUITE_P(PolicyProviderMacTest,
                         ConfigurationPolicyProviderTest,
                         testing::Values(TestHarness::Create));

// TODO(joaodasilva): instantiate Configuration3rdPartyPolicyProviderTest too
// once the mac loader supports 3rd party policy. http://crbug.com/108995

// Special test cases for some mac preferences details.
class PolicyLoaderMacTest : public PolicyTestBase {
 protected:
  PolicyLoaderMacTest()
      : prefs_(new MockPreferences()) {}

  void SetUp() override {
    PolicyTestBase::SetUp();
    std::unique_ptr<AsyncPolicyLoader> loader(new PolicyLoaderMac(
        task_environment_.GetMainThreadTaskRunner(), base::FilePath(), prefs_));
    provider_ = std::make_unique<AsyncPolicyProvider>(&schema_registry_,
                                                      std::move(loader));
    provider_->Init(&schema_registry_);
  }

  void TearDown() override {
    provider_->Shutdown();
    PolicyTestBase::TearDown();
  }

  raw_ptr<MockPreferences> prefs_;
  std::unique_ptr<AsyncPolicyProvider> provider_;
};

TEST_F(PolicyLoaderMacTest, Invalid) {
  ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(test_keys::kKeyString));
  const char buffer[] = "binary \xde\xad\xbe\xef data";
  ScopedCFTypeRef<CFDataRef> invalid_data(
      CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(buffer),
                   std::size(buffer)));
  ASSERT_TRUE(invalid_data);
  prefs_->AddTestItem(name, invalid_data.get(), /*is_forced=*/true,
                      /*is_machine=*/true);
  prefs_->AddTestItem(name, invalid_data.get(), /*is_forced=*/false,
                      /*is_machine=*/true);

  // Make the provider read the updated |prefs_|.
  provider_->RefreshPolicies();
  task_environment_.RunUntilIdle();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_->policies().Equals(kEmptyBundle));
}

TEST_F(PolicyLoaderMacTest, TestNonForcedValue) {
  ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(test_keys::kKeyString));
  ScopedCFTypeRef<CFPropertyListRef> test_value(
      base::SysUTF8ToCFStringRef("string value"));
  ASSERT_TRUE(test_value.get());
  prefs_->AddTestItem(name, test_value.get(), /*is_forced=*/false,
                      /*is_machine=*/true);

  // Make the provider read the updated |prefs_|.
  provider_->RefreshPolicies();
  task_environment_.RunUntilIdle();
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_keys::kKeyString, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
           POLICY_SOURCE_PLATFORM, base::Value("string value"), nullptr);
  EXPECT_TRUE(provider_->policies().Equals(expected_bundle));
}

TEST_F(PolicyLoaderMacTest, TestUserScopeValue) {
  ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(test_keys::kKeyString));
  ScopedCFTypeRef<CFPropertyListRef> test_value(
      base::SysUTF8ToCFStringRef("string value"));
  ASSERT_TRUE(test_value.get());
  prefs_->AddTestItem(name, test_value.get(), /*is_forced=*/true,
                      /*is_machine=*/false);

  // Make the provider read the updated |prefs_|.
  provider_->RefreshPolicies();
  task_environment_.RunUntilIdle();
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_keys::kKeyString, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_PLATFORM, base::Value("string value"), nullptr);
  EXPECT_TRUE(provider_->policies().Equals(expected_bundle));
}

}  // namespace policy
