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

#include "cobalt/updater/configurator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kOmahaCobaltLTSNightlyAppID[] =
    "{26CD2F67-091F-4680-A9A9-2229635B65A5}";
const char kOmahaCobaltTrunkAppID[] = "{A9557415-DDCD-4948-8113-C643EFCF710C}";
const char kOmahaCobaltAppID[] = "{6D4E53F3-CC64-4CB8-B6BD-AB0B8F300E1C}";
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
      cobalt::updater::Configurator::GetAppGuidHelper("prod", "23.master.0"),
      kOmahaCobaltTrunkAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsLtsIdWithVersionMaster) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("ltsnightly",
                                                           "23.master.0"),
           kOmahaCobaltLTSNightlyAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsLtsIdWithVersionLts) {
  CHECK_EQ(
      cobalt::updater::Configurator::GetAppGuidHelper("ltsnightly", "23.lts.0"),
      kOmahaCobaltLTSNightlyAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsTrunkIdWithVersionMain) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("prod", "23.main.0"),
           kOmahaCobaltTrunkAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsProdIdWithVersionLts) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("prod", "23.lts.0"),
           kOmahaCobaltAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsProdIdWithChannelEmpty) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("", "23.lts.0"),
           kOmahaCobaltAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsTrunkIdWithVersionEmpty) {
  CHECK_EQ(cobalt::updater::Configurator::GetAppGuidHelper("", ""),
           kOmahaCobaltTrunkAppID);
}

TEST_F(ConfiguratorTest, GetAppGuidReturnsTrunkIdWithVersionMasterLts) {
  CHECK_EQ(
      cobalt::updater::Configurator::GetAppGuidHelper("", "23.master.lts.0"),
      kOmahaCobaltTrunkAppID);
}

}  // namespace updater
}  // namespace cobalt
