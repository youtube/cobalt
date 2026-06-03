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

#include "third_party/blink/renderer/platform/media/web_content_decryption_module_impl.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/starboard/starboard_cdm_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/media/cdm_session_adapter.h"

namespace blink {

const char* kKeyType = "com.widevine.alpha";

class WebContentDecryptionModuleImplTest : public testing::Test {
 public:
  void SetUp() override {}

  void OnWebCdmCreated(WebContentDecryptionModule* cdm,
                       const std::string& error_message) {
    cdm_ = cdm;
  }

 protected:
  media::CdmConfig cdm_config_{kKeyType, false, false, false};
  std::unique_ptr<media::StarboardCdmFactory> cdm_factory_;
  scoped_refptr<CdmSessionAdapter> adapter_;
  WebContentDecryptionModule* cdm_{nullptr};

  base::WeakPtrFactory<WebContentDecryptionModuleImplTest> weak_factory_{this};
};

TEST_F(WebContentDecryptionModuleImplTest, IsValid) {
  EXPECT_NE(cdm_factory_, nullptr);

  base::test::TaskEnvironment task_environment_;
  cdm_factory_ = std::make_unique<media::StarboardCdmFactory>();
  adapter_ = new CdmSessionAdapter();

  adapter_->CreateCdm(
      cdm_factory_.get(), cdm_config_,
      base::BindOnce(&WebContentDecryptionModuleImplTest::OnWebCdmCreated,
                     weak_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(cdm_, nullptr);
}

}  // namespace blink
