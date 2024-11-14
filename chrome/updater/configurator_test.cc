// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "chrome/updater/configurator.h"
#include "chrome/updater/util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kTestSbVersion14 = 14;
const int kTestSbVersion15 = 15;
const char kTestStaticChannel[] = "24lts10";
}  // namespace

namespace cobalt {
namespace updater {

class ConfiguratorTest : public testing::Test {
 protected:
  ConfiguratorTest() {}
  ~ConfiguratorTest() {}
};

TEST_F(ConfiguratorTest, GetAppGuidReturnsTrunkIdWithVersionMaster) {
  CHECK_EQ(
      cobalt::updater::Configurator::GetAppGuidHelper("prod",
                                                      "23.master.0",
                                                      kTestSbVersion14),
      kOmahaCobaltTrunkAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsLtsIdWithVersionMaster) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("ltsnightly",
                                                           "23.master.0",
                                                           kTestSbVersion14),
           kOmahaCobaltLTSNightlyAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsLtsIdWithVersionLts) {
  CHECK_EQ(
      cobalt::updater::Configurator::GetAppGuidHelper("ltsnightly",
                                                      "23.lts.0",
                                                      kTestSbVersion14),
      kOmahaCobaltLTSNightlyAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsProdIdWithVersionMain) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("prod",
                                                           "23.main.0",
                                                           kTestSbVersion14),
           kChannelAndSbVersionToOmahaIdMap.at(
              "prod" + std::to_string(kTestSbVersion14)));
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsProdIdWithVersionLts) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("prod",
                                                           "23.lts.0",
                                                           kTestSbVersion14),
           kChannelAndSbVersionToOmahaIdMap.at(
              "prod" + std::to_string(kTestSbVersion14)));
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsProdIdWithChannelEmpty) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("",
                                                           "23.lts.0",
                                                           kTestSbVersion15),
           kChannelAndSbVersionToOmahaIdMap.at(
              "prod" + std::to_string(kTestSbVersion15)));
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsTrunkIdWithInvalidSbVersion) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("", "", 0),
           kOmahaCobaltAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsTrunkIdWithVersionMasterLts) {
  CHECK_EQ(
      cobalt::updater::Configurator::GetAppGuidHelper("",
                                                      "23.master.lts.0",
                                                      kTestSbVersion14),
      kChannelAndSbVersionToOmahaIdMap.at("prod" +
                                          std::to_string(kTestSbVersion14)));
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsProdIdWithInvalidChannel) {
  CHECK_EQ(
      cobalt::updater::Configurator::GetAppGuidHelper("invalid",
                                                      "23.lts.0",
                                                      kTestSbVersion14),
      kChannelAndSbVersionToOmahaIdMap.at("prod" +
                                          std::to_string(kTestSbVersion14)));
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsCorrectNewConfigId) {
  for (auto kvpair : kChannelAndSbVersionToOmahaIdMap) {
    std::string channel = kvpair.first.substr(0, kvpair.first.size() - 2);
    int sb_version = std::stoi(kvpair.first.substr(kvpair.first.size() - 2, 2));
    CHECK_EQ(
      cobalt::updater::Configurator::GetAppGuidHelper(channel,
                                                      "23.lts.0",
                                                      sb_version),
      kvpair.second);
  }
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsLegacyConfigWithOldStaticChannel) {
  CHECK_EQ(
      cobalt::updater::Configurator::GetAppGuidHelper(kTestStaticChannel,
                                                      "23.lts.0",
                                                      kTestSbVersion14),
      kOmahaCobaltAppID);
}

}  // namespace updater
}  // namespace cobalt
