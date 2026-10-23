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

#include "starboard/common/drm.h"

#include <sstream>
#include <string>

#include "starboard/drm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

struct DrmSessionRequestTypeParam {
  SbDrmSessionRequestType type;
  const char* name;
};

class SbDrmSessionRequestTypeTest
    : public ::testing::TestWithParam<DrmSessionRequestTypeParam> {};

TEST_P(SbDrmSessionRequestTypeTest, GetSbDrmSessionRequestTypeName) {
  const auto& [type, name] = GetParam();

  EXPECT_STREQ(GetSbDrmSessionRequestTypeName(type), name);
}

TEST_P(SbDrmSessionRequestTypeTest, StreamInsertionOperator) {
  const auto& [type, name] = GetParam();
  std::stringstream ss;

  ss << type;

  EXPECT_EQ(ss.str(), name);
}

INSTANTIATE_TEST_SUITE_P(
    DrmTest,
    SbDrmSessionRequestTypeTest,
    ::testing::Values(
        DrmSessionRequestTypeParam{
            .type = kSbDrmSessionRequestTypeLicenseRequest,
            .name = "license-request"},
        DrmSessionRequestTypeParam{
            .type = kSbDrmSessionRequestTypeLicenseRenewal,
            .name = "license-renewal"},
        DrmSessionRequestTypeParam{
            .type = kSbDrmSessionRequestTypeLicenseRelease,
            .name = "license-release"},
        DrmSessionRequestTypeParam{
            .type = kSbDrmSessionRequestTypeIndividualizationRequest,
            .name = "individualization-request"}));

struct DrmStatusParam {
  SbDrmStatus status;
  const char* name;
};

class SbDrmStatusTest : public ::testing::TestWithParam<DrmStatusParam> {};

TEST_P(SbDrmStatusTest, GetSbDrmStatusName) {
  const auto& [status, name] = GetParam();

  EXPECT_STREQ(GetSbDrmStatusName(status), name);
}

TEST_P(SbDrmStatusTest, StreamInsertionOperator) {
  const auto& [type, name] = GetParam();
  std::stringstream ss;

  ss << type;

  EXPECT_EQ(ss.str(), name);
}

INSTANTIATE_TEST_SUITE_P(
    DrmTest,
    SbDrmStatusTest,
    ::testing::Values(
        DrmStatusParam{.status = kSbDrmStatusSuccess, .name = "success"},
        DrmStatusParam{.status = kSbDrmStatusTypeError, .name = "type-error"},
        DrmStatusParam{.status = kSbDrmStatusNotSupportedError,
                       .name = "not-supported-error"},
        DrmStatusParam{.status = kSbDrmStatusInvalidStateError,
                       .name = "invalid-state-error"},
        DrmStatusParam{.status = kSbDrmStatusQuotaExceededError,
                       .name = "quota-exceeded-error"},
        DrmStatusParam{.status = kSbDrmStatusUnknownError,
                       .name = "unknown-error"}));

}  // namespace
}  // namespace starboard
