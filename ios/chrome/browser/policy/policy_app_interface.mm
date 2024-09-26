// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/policy_app_interface.h"

#import <memory>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/json/json_string_value_serializer.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/test/ios/wait_util.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/values.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/browser/url_blocklist_manager.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/device_management_service.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/configuration_policy_provider.h"
#import "components/policy/core/common/policy_bundle.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/policy_namespace.h"
#import "components/policy/core/common/policy_types.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/paths/paths.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/test_platform_policy_provider.h"
#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_service.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Directory where device management token is stored. This value is from
// "ios/chrome/browser/policy/browser_dm_token_storage_ios.mm"
const char kDmTokenBaseDir[] =
    FILE_PATH_LITERAL("Google/Chrome Cloud Enrollment");

// Directory where cloud policy are stored. This value is from
// "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.cc"
const base::FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Policy");

// Returns a JSON-encoded string representing the given `base::Value`. If
// `value` is nullptr, returns a string representing a `base::Value` of type
// NONE.
NSString* SerializeValue(const base::Value* value) {
  base::Value none_value(base::Value::Type::NONE);

  if (!value) {
    value = &none_value;
  }
  DCHECK(value);

  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  serializer.Serialize(*value);
  return base::SysUTF8ToNSString(serialized_value);
}

// Takes a JSON-encoded string representing a `base::Value`, and deserializes
// into a `base::Value` pointer. If nullptr is given, returns a pointer to a
// `base::Value` of type NONE.
absl::optional<base::Value> DeserializeValue(NSString* json_value) {
  if (!json_value) {
    return base::Value(base::Value::Type::NONE);
  }

  std::string json = base::SysNSStringToUTF8(json_value);
  JSONStringValueDeserializer deserializer(json);
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr,
                               /*error_message=*/nullptr);
  return value ? absl::make_optional<base::Value>(std::move(*value))
               : absl::nullopt;
}
}  // namespace

@implementation PolicyAppInterface

+ (NSString*)valueForPlatformPolicy:(NSString*)policyKey {
  const std::string key = base::SysNSStringToUTF8(policyKey);

  BrowserPolicyConnectorIOS* connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  if (!connector) {
    return SerializeValue(nullptr);
  }

  const policy::ConfigurationPolicyProvider* platformProvider =
      connector->GetPlatformProvider();
  if (!platformProvider) {
    return SerializeValue(nullptr);
  }

  const policy::PolicyBundle& policyBundle = platformProvider->policies();
  const policy::PolicyMap& policyMap = policyBundle.Get(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, ""));
  // `GetValueUnsafe` is used due to multiple policy types being handled.
  return SerializeValue(policyMap.GetValueUnsafe(key));
}

+ (void)setPolicyValue:(NSString*)jsonValue forKey:(NSString*)policyKey {
  absl::optional<base::Value> value = DeserializeValue(jsonValue);
  policy::PolicyMap values;
  values.Set(base::SysNSStringToUTF8(policyKey), policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
             std::move(value), /*external_data_fetcher=*/nullptr);
  GetTestPlatformPolicyProvider()->UpdateChromePolicy(values);
}

+ (void)clearPolicies {
  policy::PolicyMap values;
  GetTestPlatformPolicyProvider()->UpdateChromePolicy(values);
}

+ (void)clearAllPoliciesInNSUserDefault {
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

+ (BOOL)isURLBlocked:(NSString*)URL {
  GURL gurl = GURL(base::SysNSStringToUTF8(URL));
  PolicyBlocklistService* service =
      PolicyBlocklistServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  return service->GetURLBlocklistState(gurl) ==
         policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

+ (void)setBrowserCloudPolicyDataWithDomain:(NSString*)domain {
  policy::MachineLevelUserCloudPolicyManager* manager =
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->machine_level_user_cloud_policy_manager();
  DCHECK(manager);

  policy::CloudPolicyStore* store = manager->core()->store();
  DCHECK(store);

  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_managed_by(base::SysNSStringToUTF8(domain));
  store->set_policy_data_for_testing(std::move(policy_data));
}

+ (void)setUserCloudPolicyDataWithDomain:(NSString*)domain {
  policy::UserCloudPolicyManager* manager =
      chrome_test_util::GetOriginalBrowserState()->GetUserCloudPolicyManager();
  DCHECK(manager);

  policy::CloudPolicyStore* store = manager->core()->store();
  DCHECK(store);

  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_managed_by(base::SysNSStringToUTF8(domain));
  store->set_policy_data_for_testing(std::move(policy_data));
}

+ (BOOL)clearDMTokenDirectory {
  base::FilePath appDataDirPath;
  base::PathService::Get(base::DIR_APP_DATA, &appDataDirPath);
  base::FilePath dmTokenDirPath = appDataDirPath.Append(kDmTokenBaseDir);

  __block BOOL didComplete = NO;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(^{
        base::DeletePathRecursively(dmTokenDirPath);
      }),
      base::BindOnce(^{
        didComplete = YES;
      }));

  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^{
        return didComplete;
      });
}

+ (BOOL)isCloudPolicyClientRegistered {
  return GetApplicationContext()
      ->GetBrowserPolicyConnector()
      ->machine_level_user_cloud_policy_manager()
      ->core()
      ->client()
      ->is_registered();
}

+ (BOOL)clearCloudPolicyDirectory {
  base::FilePath userDataDir;
  base::PathService::Get(ios::DIR_USER_DATA, &userDataDir);
  base::FilePath policyDir = userDataDir.Append(kPolicyDir);

  __block BOOL didComplete = NO;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(^{
        base::DeletePathRecursively(policyDir);
      }),
      base::BindOnce(^{
        didComplete = YES;
      }));

  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^{
        return didComplete;
      });
}

+ (BOOL)hasUserPolicyDataInCurrentBrowserState {
  policy::UserCloudPolicyManager* manager =
      chrome_test_util::GetOriginalBrowserState()->GetUserCloudPolicyManager();
  DCHECK(manager);

  policy::CloudPolicyStore* store = manager->core()->store();
  DCHECK(store);

  return store->has_policy() && store->is_managed();
}

+ (BOOL)hasUserPolicyInCurrentBrowserState:(NSString*)policyName
                          withIntegerValue:(int)expectedValue {
  policy::UserCloudPolicyManager* manager =
      chrome_test_util::GetOriginalBrowserState()->GetUserCloudPolicyManager();
  DCHECK(manager);

  policy::CloudPolicyStore* store = manager->core()->store();
  DCHECK(store);

  const base::Value* value = store->policy_map().GetValue(
      base::SysNSStringToUTF8(policyName), base::Value::Type::INTEGER);

  return value && value->GetInt() == expectedValue;
}

@end
