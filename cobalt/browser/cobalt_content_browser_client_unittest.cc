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

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

class CobaltContentBrowserClientTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_CACHE, temp_dir_.GetPath(), true, true);
    instance_ = GlobalFeatures::GetInstance();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_override_;
  GlobalFeatures* instance_ = nullptr;
};

TEST_F(CobaltContentBrowserClientTest,
       CreateFeatureListAndFieldTrialsAppliesH5vccSettings) {
  ASSERT_NE(instance_, nullptr);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableH5vccSettings, "Foo=1234;Bar=Baz");

  auto backup = base::FeatureList::ClearInstanceForTesting();
  CobaltContentBrowserClient client(std::nullopt, "");
  client.CreateFeatureListAndFieldTrials();

  const auto& settings = instance_->GetSettings();
  auto it1 = settings.find("Foo");
  ASSERT_NE(it1, settings.end());
  EXPECT_EQ(std::get<int64_t>(it1->second), 1234);

  auto it2 = settings.find("Bar");
  ASSERT_NE(it2, settings.end());
  EXPECT_EQ(std::get<std::string>(it2->second), "Baz");

  base::FeatureList::ClearInstanceForTesting();
  base::FeatureList::RestoreInstanceForTesting(std::move(backup));
}

}  // namespace
}  // namespace cobalt
