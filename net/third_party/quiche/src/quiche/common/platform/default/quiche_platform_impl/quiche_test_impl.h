// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_COMMON_PLATFORM_DEFAULT_QUICHE_PLATFORM_IMPL_QUICHE_TEST_IMPL_H_
#define QUICHE_COMMON_PLATFORM_DEFAULT_QUICHE_PLATFORM_IMPL_QUICHE_TEST_IMPL_H_

#include "gmock/gmock.h"
#include "gtest/gtest-spi.h"
#include "gtest/gtest.h"

#define EXPECT_QUICHE_DEBUG_DEATH_IMPL(condition, message) \
  EXPECT_DEBUG_DEATH(condition, message)

#define QUICHE_TEST_DISABLED_IN_CHROME_IMPL(name) name
#define QUICHE_SLOW_TEST_IMPL(test) test

class QuicheFlagSaverImpl {
 public:
  QuicheFlagSaverImpl();
  ~QuicheFlagSaverImpl();

 private:
#define QUIC_FLAG(flag, value) bool saved_##flag##_;
#include "quiche/quic/core/quic_flags_list.h"
#undef QUIC_FLAG

#define QUIC_PROTOCOL_FLAG(type, flag, ...) type saved_##flag##_;
#include "quiche/quic/core/quic_protocol_flags_list.h"
#undef QUIC_PROTOCOL_FLAG
};

class ScopedEnvironmentForThreadsImpl {};

namespace quiche::test {

class QuicheTestImpl : public ::testing::Test {
 private:
  QuicheFlagSaverImpl saver_;
};

template <class T>
class QuicheTestWithParamImpl : public ::testing::TestWithParam<T> {
 private:
  QuicheFlagSaverImpl saver_;
};

inline std::string QuicheGetCommonSourcePathImpl() { return "quiche/common"; }

}  // namespace quiche::test

inline std::string QuicheGetTestMemoryCachePathImpl() {
  return "quiche/quic/test_tools/quic_http_response_cache_data";
}

#endif  // QUICHE_COMMON_PLATFORM_DEFAULT_QUICHE_PLATFORM_IMPL_QUICHE_TEST_IMPL_H_
