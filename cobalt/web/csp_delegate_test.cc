// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/csp_delegate.h"

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/web/csp_delegate_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::ValuesIn;

namespace cobalt {
namespace web {

namespace {

struct ResourcePair {
  // Resource type queried for the test.
  CspDelegate::ResourceType type;
  // Directive to allow 'self' for.
  const char* directive;
  // Effective directive reported in the violation.
  const char* effective_directive;
};

std::ostream& operator<<(std::ostream& out, const ResourcePair& obj) {
  return out << obj.directive;
}

const ResourcePair s_params[] = {
    {CspDelegate::kFont, "font-src", "font-src"},
    {CspDelegate::kFont, "default-src", "font-src"},
    {CspDelegate::kImage, "img-src", "img-src"},
    {CspDelegate::kImage, "default-src", "img-src"},
    {CspDelegate::kLocation, "h5vcc-location-src", "h5vcc-location-src"},
    {CspDelegate::kMedia, "media-src", "media-src"},
    {CspDelegate::kMedia, "default-src", "media-src"},
    {CspDelegate::kScript, "script-src", "script-src"},
    {CspDelegate::kScript, "default-src", "script-src"},
    {CspDelegate::kStyle, "style-src", "style-src"},
    {CspDelegate::kStyle, "default-src", "style-src"},
    {CspDelegate::kWorker, "worker-src", "worker-src"},
    {CspDelegate::kWorker, "script-src", "worker-src"},
    {CspDelegate::kWorker, "default-src", "worker-src"},
    {CspDelegate::kXhr, "connect-src", "connect-src"},
    {CspDelegate::kXhr, "default-src", "connect-src"},
    {CspDelegate::kWebSocket, "connect-src", "connect-src"},
    {CspDelegate::kWebSocket, "default-src", "connect-src"},
};

std::string ResourcePairName(::testing::TestParamInfo<ResourcePair> info) {
  std::string directive(info.param.directive);
  std::replace(directive.begin(), directive.end(), '-', '_');
  std::string effective_directive(info.param.effective_directive);
  std::replace(effective_directive.begin(), effective_directive.end(), '-',
               '_');
  return base::StringPrintf("type_%d_directive_%s_effective_%s",
                            info.param.type, directive.c_str(),
                            effective_directive.c_str());
}

class MockViolationReporter : public CspViolationReporter {
 public:
  MockViolationReporter()
      : CspViolationReporter(nullptr, network_bridge::PostSender()) {}
  MOCK_METHOD1(Report, void(const csp::ViolationInfo&));
};

class CspDelegateTest : public ::testing::TestWithParam<ResourcePair> {
 protected:
  virtual void SetUp();
  std::unique_ptr<CspDelegateSecure> csp_delegate_;
  StrictMock<MockViolationReporter>* mock_reporter_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// TODO: Combine this with the one in xml_http_request_test.
class ScopedLogInterceptor {
 public:
  explicit ScopedLogInterceptor(std::string* output)
      : output_(output), old_handler_(logging::GetLogMessageHandler()) {
    DCHECK(output_);
    DCHECK(!log_interceptor_);
    log_interceptor_ = this;
    logging::SetLogMessageHandler(LogHandler);
  }

  ~ScopedLogInterceptor() {
    logging::SetLogMessageHandler(old_handler_);
    log_interceptor_ = nullptr;
  }

  static bool LogHandler(int severity, const char* file, int line,
                         size_t message_start, const std::string& str) {
    *log_interceptor_->output_ += str;
    return true;
  }

 private:
  std::string* output_;
  logging::LogMessageHandlerFunction old_handler_;
  static ScopedLogInterceptor* log_interceptor_;
};

ScopedLogInterceptor* ScopedLogInterceptor::log_interceptor_;

}  // namespace

void CspDelegateTest::SetUp() {
  GURL origin("https://www.example.com");
  std::string default_navigation_policy("h5vcc-location-src 'self'");

  mock_reporter_ = new StrictMock<MockViolationReporter>();
  std::unique_ptr<CspViolationReporter> reporter(mock_reporter_);

  csp_delegate_.reset(new CspDelegateSecure(
      std::move(reporter), origin, csp::kCSPRequired, base::Closure()));
  std::string policy;
  if (!strcmp(GetParam().directive, "default-src")) {
    policy = base::StringPrintf("%s 'self'", GetParam().directive);
  } else {
    policy =
        base::StringPrintf("default-src none; %s 'self'", GetParam().directive);
  }
  csp_delegate_->OnReceiveHeader(policy, csp::kHeaderTypeEnforce,
                                 csp::kHeaderSourceMeta);
}

TEST_P(CspDelegateTest, LoadOk) {
  CspDelegate::ResourceType param = GetParam().type;
  GURL test_url("https://www.example.com");
  EXPECT_TRUE(csp_delegate_->CanLoad(param, test_url, false));
}

TEST_P(CspDelegateTest, LoadNotOk) {
  CspDelegate::ResourceType param = GetParam().type;
  std::string effective_directive = GetParam().effective_directive;
  GURL test_url("http://www.evil.com");

  csp::ViolationInfo info;
  EXPECT_CALL(*mock_reporter_, Report(_)).WillOnce(SaveArg<0>(&info));
  EXPECT_FALSE(csp_delegate_->CanLoad(param, test_url, false));
  EXPECT_EQ(test_url, info.blocked_url);
  EXPECT_EQ(effective_directive, info.effective_directive);
}

INSTANTIATE_TEST_CASE_P(CanLoad, CspDelegateTest, ValuesIn(s_params),
                        ResourcePairName);

TEST(CspDelegateFactoryTest, Secure) {
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  CspDelegate::Options options;
  options.enforcement_type = kCspEnforcementEnable;
  std::unique_ptr<CspDelegate> delegate =
      CspDelegateFactory::Create(nullptr, options);
  EXPECT_TRUE(delegate != nullptr);
}

TEST(CspDelegateFactoryTest, InsecureBlocked) {
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::string output;
  {
    // Capture the output, because we should get a FATAL log and we don't
    // want to crash.
    ScopedLogInterceptor li(&output);
    CspDelegate::Options options;
    options.enforcement_type = kCspEnforcementDisable;
    std::unique_ptr<CspDelegate> delegate =
        CspDelegateFactory::Create(nullptr, options);

    std::unique_ptr<CspDelegate> empty_delegate;
    EXPECT_EQ(empty_delegate.get(), delegate.get());
  }
  EXPECT_THAT(output, HasSubstr("FATAL"));
}

TEST(CspDelegateFactoryTest, InsecureAllowed) {
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // This only compiles because this test is a friend of CspDelegateFactory,
  // otherwise GetInsecureAllowedToken is private.
  CspDelegate::Options options;
  options.enforcement_type = kCspEnforcementDisable;
  options.insecure_allowed_token =
      CspDelegateFactory::GetInstance()->GetInsecureAllowedToken();
  std::unique_ptr<CspDelegate> delegate =
      CspDelegateFactory::Create(nullptr, options);
  EXPECT_TRUE(delegate != nullptr);
}

}  // namespace web
}  // namespace cobalt
