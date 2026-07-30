// Copyright (C) 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PROTOSTORE_STATUS_MATCHERS_H_
#define PROTOSTORE_STATUS_MATCHERS_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/message.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace protostore {
namespace testing {

namespace {
using ::testing::MakePolymorphicMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::PolymorphicMatcher;

class StatusCodeMatcher {
 public:
  StatusCodeMatcher(absl::StatusCode code) : code_(code) {}

  template <class T>
  bool MatchAndExplain(absl::StatusOr<T>* statusor,
                       MatchResultListener *listener) const {
    *listener << absl::StatusCodeToString(statusor->status().code());
    return statusor->status().code() == code_;
  }

  template <class U>
  bool MatchAndExplain(const absl::StatusOr<U>& statusor,
                       MatchResultListener *listener) const {
    *listener << absl::StatusCodeToString(statusor.status().code());
    return statusor.status().code() == code_;
  }

  template <class V>
  bool MatchAndExplain(const V& status,
      MatchResultListener *listener) const {
    *listener << absl::StatusCodeToString(status.code());
    return status.code() == code_;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is " << absl::StatusCodeToString(code_);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "isn't " << absl::StatusCodeToString(code_);
  }

 private:
  absl::StatusCode code_;
};

// Monomorphic implementation of matcher IsOkAndHolds(m).  StatusOrType is a
// reference to StatusOr<T>.
template <typename StatusOrType>
class IsOkAndHoldsMatcherImpl
    : public ::testing::MatcherInterface<StatusOrType> {
 public:
  typedef
      typename std::remove_reference<StatusOrType>::type::value_type value_type;

  template <typename InnerMatcher>
  explicit IsOkAndHoldsMatcherImpl(InnerMatcher&& inner_matcher)
      : inner_matcher_(::testing::SafeMatcherCast<const value_type&>(
            std::forward<InnerMatcher>(inner_matcher))) {}

  void DescribeTo(std::ostream* os) const override {
    *os << "is OK and has a value that ";
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "isn't OK or has a value that ";
    inner_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(
      StatusOrType actual_value,
      ::testing::MatchResultListener* result_listener) const override {
    if (!actual_value.ok()) {
      *result_listener << "which has status " << actual_value.status();
      return false;
    }

    ::testing::StringMatchResultListener inner_listener;
    const bool matches =
        inner_matcher_.MatchAndExplain(*actual_value, &inner_listener);
    const std::string inner_explanation = inner_listener.str();
    if (!inner_explanation.empty()) {
      *result_listener << "which contains value "
                       << ::testing::PrintToString(*actual_value) << ", "
                       << inner_explanation;
    }
    return matches;
  }

 private:
  const ::testing::Matcher<const value_type&> inner_matcher_;
};

// Implements IsOkAndHolds(m) as a polymorphic matcher.
template <typename InnerMatcher>
class IsOkAndHoldsMatcher {
 public:
  explicit IsOkAndHoldsMatcher(InnerMatcher inner_matcher)
      : inner_matcher_(std::move(inner_matcher)) {}

  // Converts this polymorphic matcher to a monomorphic matcher of the
  // given type.  StatusOrType can be either StatusOr<T> or a
  // reference to StatusOr<T>.
  template <typename StatusOrType>
  operator ::testing::Matcher<StatusOrType>() const {  // NOLINT
    return ::testing::Matcher<StatusOrType>(
        new IsOkAndHoldsMatcherImpl<const StatusOrType&>(inner_matcher_));
  }

 private:
  const InnerMatcher inner_matcher_;
};

class ProtoStringMatcher {
 public:
  ProtoStringMatcher(const google::protobuf::Message& msg)
    : expected_(msg.DebugString()) {}

  template <typename Message> bool MatchAndExplain(const Message& p,
    MatchResultListener *listener) const {
      *listener << p.DebugString();
      return p.DebugString() == expected_;
  }

  void DescribeTo(std::ostream* os) const {
    *os << expected_;
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "not equal to expected message: " << expected_;
  }

 private:
  const std::string expected_;
};

}  // namespace

PolymorphicMatcher<StatusCodeMatcher> StatusIs(
    absl::StatusCode code) {
  return MakePolymorphicMatcher(StatusCodeMatcher(code));
}

PolymorphicMatcher<StatusCodeMatcher> IsOk() {
  return MakePolymorphicMatcher(StatusCodeMatcher(absl::StatusCode::kOk));
}

// Returns a gMock matcher that matches a StatusOr<> whose status is
// OK and whose value matches the inner matcher.
template <typename InnerMatcher>
IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type> IsOkAndHolds(
    InnerMatcher&& inner_matcher) {
  return IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type>(
      std::forward<InnerMatcher>(inner_matcher));
}

PolymorphicMatcher<ProtoStringMatcher> EqualsProto(
    const google::protobuf::Message& x) {
  return MakePolymorphicMatcher(ProtoStringMatcher(x));
}

#define EXPECT_OK(status) \
  EXPECT_THAT((status), testing::IsOk())
#define ASSERT_OK(status) \
  ASSERT_THAT((status), testing::IsOk())

}  // namespace testing
}  // namespace protostore
#endif  // PROTOSTORE_STATUS_MATCHERS_H_
