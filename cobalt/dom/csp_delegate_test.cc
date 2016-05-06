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
#include "cobalt/dom/csp_delegate_factory.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::HasSubstr;
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
  scoped_ptr<CspDelegateSecure> csp_delegate_;
  StrictMock<MockViolationReporter>* mock_reporter_;
};

// TODO(***REMOVED***): Combine this with the one in xml_http_request_test.
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
    log_interceptor_ = NULL;
  }

  static bool LogHandler(int severity, const char* file, int line,
                         size_t message_start, const std::string& str) {
    UNREFERENCED_PARAMETER(severity);
    UNREFERENCED_PARAMETER(file);
    UNREFERENCED_PARAMETER(line);
    UNREFERENCED_PARAMETER(message_start);
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
  scoped_ptr<CspViolationReporter> reporter(mock_reporter_);

  csp_delegate_.reset(new CspDelegateSecure(
      reporter.Pass(), origin, default_navigation_policy, base::Closure()));
  std::string policy =
      base::StringPrintf("default-src none; %s 'self'", GetParam().directive);
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
  std::string effective_directive = GetParam().directive;
  GURL test_url("http://www.evil.com");

  csp::ViolationInfo info;
  EXPECT_CALL(*mock_reporter_, Report(_)).WillOnce(SaveArg<0>(&info));
  EXPECT_FALSE(csp_delegate_->CanLoad(param, test_url, false));
  EXPECT_EQ(test_url, info.blocked_url);
  EXPECT_EQ(effective_directive, info.effective_directive);
}

INSTANTIATE_TEST_CASE_P(CanLoad, CspDelegateTest, ValuesIn(s_params));

TEST(CspDelegateFactoryTest, Secure) {
  scoped_ptr<CspDelegate> delegate = CspDelegateFactory::GetInstance()->Create(
      kCspEnforcementEnable, scoped_ptr<CspViolationReporter>(), GURL(),
      std::string(), base::Closure());
  EXPECT_TRUE(delegate != NULL);
}

TEST(CspDelegateFactoryTest, InsecureBlocked) {
  std::string output;
  {
    // Capture the output, because we should get a FATAL log and we don't
    // want to crash.
    ScopedLogInterceptor li(&output);
    scoped_ptr<CspDelegate> delegate =
        CspDelegateFactory::GetInstance()->Create(
            kCspEnforcementDisable, scoped_ptr<CspViolationReporter>(), GURL(),
            std::string(), base::Closure());

    EXPECT_EQ(scoped_ptr<CspDelegate>(), delegate.get());
  }
  EXPECT_THAT(output, HasSubstr("FATAL"));
}

TEST(CspDelegateFactoryTest, InsecureAllowed) {
  // This only compiles because this test is a friend of CspDelegateFactory,
  // otherwise GetInsecureAllowedToken is private.
  int token = CspDelegateFactory::GetInstance()->GetInsecureAllowedToken();
  scoped_ptr<CspDelegate> delegate = CspDelegateFactory::GetInstance()->Create(
      kCspEnforcementDisable, scoped_ptr<CspViolationReporter>(), GURL(),
      std::string(), base::Closure(), token);
  EXPECT_TRUE(delegate != NULL);
}

}  // namespace dom
}  // namespace cobalt
