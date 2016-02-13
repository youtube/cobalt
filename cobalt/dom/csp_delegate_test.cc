/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/csp_delegate.h"

#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::ValuesIn;

namespace cobalt {
namespace dom {

namespace {

struct ResourcePair {
  CspDelegate::ResourceType type;
  const char* directive;
};

std::ostream& operator<<(std::ostream& out, const ResourcePair& obj) {
  return out << obj.directive;
}

const ResourcePair s_params[] = {
    {CspDelegate::kFont, "font-src"},
    {CspDelegate::kImage, "img-src"},
    {CspDelegate::kLocation, "h5vcc-location-src"},
    {CspDelegate::kMedia, "media-src"},
    {CspDelegate::kScript, "script-src"},
    {CspDelegate::kStyle, "style-src"},
    {CspDelegate::kXhr, "connect-src"},
};

class MockViolationReporter : public CspViolationReporter {
 public:
  MockViolationReporter()
      : CspViolationReporter(NULL, network_bridge::PostSender()) {}
  MOCK_METHOD1(Report, void(const csp::ViolationInfo&));
};

class CspDelegateTest : public ::testing::TestWithParam<ResourcePair> {
 protected:
  virtual void SetUp();
  scoped_ptr<CspDelegate> csp_delegate_;
  StrictMock<MockViolationReporter>* mock_reporter_;
};

void CspDelegateTest::SetUp() {
  GURL origin("https://www.example.com");
  std::string default_navigation_policy("h5vcc-location-src 'self'");

  scoped_ptr<CspViolationReporter> reporter(
      new StrictMock<MockViolationReporter>());

  csp_delegate_.reset(new CspDelegate(reporter.Pass(), origin,
                                      default_navigation_policy,
                                      CspDelegate::kEnforcementEnable));
  std::string policy =
      base::StringPrintf("default-src none; %s 'self'", GetParam().directive);
  csp_delegate_->OnReceiveHeader(policy, csp::kHeaderTypeEnforce,
                                 csp::kHeaderSourceMeta);
  mock_reporter_ =
      base::polymorphic_downcast<StrictMock<MockViolationReporter>*>(
          csp_delegate_->reporter());
}

TEST_P(CspDelegateTest, LoadOk) {
  CspDelegate::ResourceType param = GetParam().type;
  GURL test_url("https://www.example.com");
  EXPECT_TRUE(csp_delegate_->CanLoad(param, test_url, false));
}

TEST_P(CspDelegateTest, LoadNotOk) {
  CspDelegate::ResourceType param = GetParam().type;
  std::string effective_directive = GetParam().directive;
  GURL test_url("http://www.evil.com");

  csp::ViolationInfo info;
  EXPECT_CALL(*mock_reporter_, Report(_)).WillOnce(SaveArg<0>(&info));
  EXPECT_FALSE(csp_delegate_->CanLoad(param, test_url, false));
  EXPECT_EQ(test_url, info.blocked_url);
  EXPECT_EQ(effective_directive, info.effective_directive);
}

INSTANTIATE_TEST_CASE_P(CanLoad, CspDelegateTest, ValuesIn(s_params));

}  // namespace

}  // namespace dom
}  // namespace cobalt
