// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/cobalt_content_browser_client.h"

#include <optional>
#include <string>
#include <variant>

#include "base/metrics/field_trial_param_associator.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "cobalt/browser/experiments/experiment_config_manager.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/buildflags.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/overlay_window.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/isolation_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "starboard/configuration_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace cobalt {
namespace {

class CobaltContentBrowserClientTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(CobaltContentBrowserClientTest, ParseAndApplyH5vccSettingsForTesting) {
  auto* instance = GlobalFeatures::GetInstance();
  ASSERT_NE(instance, nullptr);

  ParseAndApplyH5vccSettingsForTesting("Foo=1234;Bar=Baz", instance);

  const auto& settings = instance->GetSettings();
  auto it1 = settings.find("Foo");
  ASSERT_NE(it1, settings.end());
  EXPECT_EQ(std::get<int64_t>(it1->second), 1234);

  auto it2 = settings.find("Bar");
  ASSERT_NE(it2, settings.end());
  EXPECT_EQ(std::get<std::string>(it2->second), "Baz");
}

TEST_F(CobaltContentBrowserClientTest,
       CreateWindowForVideoPictureInPicturePlatformBehavior) {
  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  std::unique_ptr<content::VideoOverlayWindow> window =
      client.CreateWindowForVideoPictureInPicture(/*controller=*/nullptr);
// TODO: b/532158001 - Support PiP on Linux.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_NE(window, nullptr);
#else
  EXPECT_EQ(window, nullptr);
#endif
}

TEST_F(CobaltContentBrowserClientTest, ComputeDefaultHttpCacheSize) {
  // 1. Nominal Starboard budget (24 MiB -> 12 MiB HTTP cache):
  EXPECT_EQ(
      CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(24 * 1024 * 1024),
      12u * 1024 * 1024);

  // 2. Current platform constant equals budget minus the 12 MiB reserve:
  EXPECT_EQ(CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(
                kSbMaxSystemPathCacheDirectorySize),
            kSbMaxSystemPathCacheDirectorySize - 12u * 1024 * 1024);

  // 3. Zero / unconfigured budget:
  EXPECT_EQ(CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(0), 0u);
}

class CobaltContentBrowserClientHeaderTest
    : public CobaltContentBrowserClientTest {
 protected:
  void SetUp() override {
    browser::CobaltHeaderValueProvider::GetInstance()
        ->ClearHeaderValuesForTesting();
  }

  void TearDown() override {
    browser::CobaltHeaderValueProvider::GetInstance()
        ->ClearHeaderValuesForTesting();
  }

  network::mojom::NetworkContextParamsPtr ConfigureNetworkContextParams() {
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::PopulateCobaltExtraRequestHeaders(params.get());
    return params;
  }

  // Returns the header client that WillCreateURLLoaderFactory() installs, if
  // any.
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
  GetInstalledHeaderClient() {
    network::URLLoaderFactoryBuilder factory_builder;
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        header_client;
    client_.WillCreateURLLoaderFactory(
        /*browser_context=*/nullptr, /*frame=*/nullptr,
        /*render_process_id=*/0,
        CobaltContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
        url::Origin(), net::IsolationInfo(), /*navigation_id=*/std::nullopt,
        ukm::kInvalidSourceIdObj, factory_builder, &header_client,
        /*bypass_redirect_checks=*/nullptr, /*disable_secure_dns=*/nullptr,
        /*factory_override=*/nullptr,
        /*navigation_response_task_runner=*/nullptr);
    return header_client;
  }

  CobaltContentBrowserClient client_{/*startup_timestamp=*/std::nullopt,
                                     /*deep_link=*/""};
};

TEST_F(CobaltContentBrowserClientHeaderTest,
       NetworkContextParamsHaveNoHeadersByDefault) {
  browser::CobaltHeaderValueProvider::GetInstance()->SetHeaderValue(
      browser::kAndroidOSExperienceHeaderName, "Amati");

  EXPECT_TRUE(
      ConfigureNetworkContextParams()->cobalt_extra_request_headers.empty());
}

TEST_F(CobaltContentBrowserClientHeaderTest,
       NetworkContextParamsHaveHeadersWhenSkippingHeaderClient) {
  base::test::ScopedFeatureList feature_list(
      features::kCobaltSkipTrustedHeaderClient);
  auto* provider = browser::CobaltHeaderValueProvider::GetInstance();
  provider->SetHeaderValue(browser::kAndroidOSExperienceHeaderName, "Amati");
  provider->SetHeaderValue(browser::kBuildFingerprintHeaderName,
                           "test/fingerprint");

  EXPECT_THAT(
      ConfigureNetworkContextParams()->cobalt_extra_request_headers,
      testing::UnorderedElementsAre(
          testing::Pair(browser::kAndroidOSExperienceHeaderName, "Amati"),
          testing::Pair(browser::kBuildFingerprintHeaderName,
                        "test/fingerprint")));
}

TEST_F(CobaltContentBrowserClientHeaderTest, InstallsHeaderClientByDefault) {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client = GetInstalledHeaderClient();
  EXPECT_TRUE(header_client.is_valid());

  // Let the self-owned header client see the disconnect and delete itself.
  header_client.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(CobaltContentBrowserClientHeaderTest,
       DoesNotInstallHeaderClientWhenSkippingHeaderClient) {
  base::test::ScopedFeatureList feature_list(
      features::kCobaltSkipTrustedHeaderClient);

  EXPECT_FALSE(GetInstalledHeaderClient().is_valid());
}

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
class CobaltContentBrowserClientFieldTrialConfigTest
    : public CobaltContentBrowserClientTest {
 protected:
  void SetUp() override { ResetGlobalState(); }

  void TearDown() override { ResetGlobalState(); }

  void ResetGlobalState() {
    auto* global_features = GlobalFeatures::GetInstance();
    ASSERT_NE(global_features, nullptr);
    global_features->experiment_config()->ClearPref(kExperimentConfigFeatures);
    global_features->experiment_config()->ClearPref(
        kExperimentConfigFeatureParams);
    global_features->experiment_config()->ClearPref(
        kExperimentConfigActiveConfigData);
    global_features->InitializeActiveConfigData(
        ExperimentConfigType::kEmptyConfig);
    global_features->experiment_config_manager()->ResetForTesting();
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  void PopulateCachedExperimentConfig() {
    auto* experiment_config =
        GlobalFeatures::GetInstance()->experiment_config();
    base::DictValue feature_map;
    feature_map.Set(features::kTestFinchFeature.name, true);
    experiment_config->SetDict(kExperimentConfigFeatures,
                               std::move(feature_map));
    base::DictValue param_map;
    param_map.Set(features::kTestFinchFeatureParam.name, "CachedConfigValue");
    experiment_config->SetDict(kExperimentConfigFeatureParams,
                               std::move(param_map));
    experiment_config->SetString(kExperimentConfigActiveConfigData,
                                 "cached_active_config_data");
  }

  base::test::ScopedCommandLine scoped_command_line_;
  CobaltContentBrowserClient client_{/*startup_timestamp=*/std::nullopt,
                                     /*deep_link=*/""};
};

TEST_F(CobaltContentBrowserClientFieldTrialConfigTest,
       SetUpCobaltFeaturesAndParamsAppliesFieldTrialTestingConfig) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      variations::switches::kEnableFieldTrialTestingConfig);
  PopulateCachedExperimentConfig();

  auto feature_list = std::make_unique<base::FeatureList>();
  client_.SetUpCobaltFeaturesAndParams(feature_list.get());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_TRUE(GlobalFeatures::GetInstance()->active_config_data().empty());
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kTestFinchFeature));
  EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "1");
}

TEST_F(CobaltContentBrowserClientFieldTrialConfigTest,
       SetUpCobaltFeaturesAndParamsDisableFieldTrialTestingConfigUsesCache) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      variations::switches::kDisableFieldTrialTestingConfig);
  PopulateCachedExperimentConfig();

  auto feature_list = std::make_unique<base::FeatureList>();
  client_.SetUpCobaltFeaturesAndParams(feature_list.get());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  auto* global_features = GlobalFeatures::GetInstance();
  EXPECT_EQ(
      global_features->experiment_config_manager()->GetExperimentConfigType(),
      ExperimentConfigType::kRegularConfig);
  EXPECT_EQ(global_features->active_config_data(), "cached_active_config_data");
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kTestFinchFeature));
  EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "CachedConfigValue");
}

TEST_F(CobaltContentBrowserClientFieldTrialConfigTest,
       SetUpCobaltFeaturesAndParamsRespectsFeatureOverridePrecedence) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      variations::switches::kEnableFieldTrialTestingConfig);

  // 1. Command-line overrides (1st) should take precedence over extra feature
  // overrides (2nd) and FieldTrialTestingConfig (3rd).
  {
    auto feature_list = std::make_unique<base::FeatureList>();
    feature_list->InitFromCommandLine(
        /*enable_features=*/"",
        /*disable_features=*/features::kTestFinchFeature.name);
    feature_list->RegisterExtraFeatureOverrides(
        {{std::cref(features::kTestFinchFeature),
          base::FeatureList::OVERRIDE_ENABLE_FEATURE}});
    client_.SetUpCobaltFeaturesAndParams(feature_list.get());

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_FALSE(base::FeatureList::IsEnabled(features::kTestFinchFeature));
    EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "");
  }

  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();

  // 2. Extra feature overrides (2nd) should take precedence over
  // FieldTrialTestingConfig (3rd).
  {
    auto feature_list = std::make_unique<base::FeatureList>();
    feature_list->InitFromCommandLine(/*enable_features=*/"",
                                      /*disable_features=*/"");
    feature_list->RegisterExtraFeatureOverrides(
        {{std::cref(features::kTestFinchFeature),
          base::FeatureList::OVERRIDE_DISABLE_FEATURE}});
    client_.SetUpCobaltFeaturesAndParams(feature_list.get());

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_FALSE(base::FeatureList::IsEnabled(features::kTestFinchFeature));
    EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "");
  }
}
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

}  // namespace
}  // namespace cobalt
