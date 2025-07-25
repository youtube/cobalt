// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/ip_protection_config_cache_impl.h"
#include "services/network/ip_protection_token_cache_manager.h"
#include "services/network/ip_protection_token_cache_manager_impl.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

constexpr char kGetAuthTokenResultHistogram[] =
    "NetworkService.IpProtection.GetAuthTokenResult";
constexpr char kTokenSpendRateHistogram[] =
    "NetworkService.IpProtection.TokenSpendRate";
constexpr char kTokenExpirationRateHistogram[] =
    "NetworkService.IpProtection.TokenExpirationRate";
const base::TimeDelta kTokenRateMeasurementInterval = base::Minutes(5);

struct ExpectedTryGetAuthTokensCall {
  // The expected batch_size argument for the call.
  uint32_t batch_size;
  // The response to the call.
  absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
      bsa_tokens;
  absl::optional<base::Time> try_again_after;
};

class MockIpProtectionConfigGetter
    : public network::mojom::IpProtectionConfigGetter {
 public:
  ~MockIpProtectionConfigGetter() override = default;

  // Register an expectation of a call to `TryGetAuthTokens()` returning the
  // given tokens.
  void ExpectTryGetAuthTokensCall(
      uint32_t batch_size,
      std::vector<network::mojom::BlindSignedAuthTokenPtr> bsa_tokens) {
    expected_try_get_auth_token_calls_.emplace_back(
        ExpectedTryGetAuthTokensCall{
            .batch_size = batch_size,
            .bsa_tokens = std::move(bsa_tokens),
            .try_again_after = absl::nullopt,
        });
  }

  // Register an expectation of a call to `TryGetAuthTokens()` returning no
  // tokens and the given `try_again_after`.
  void ExpectTryGetAuthTokensCall(uint32_t batch_size,
                                  base::Time try_again_after) {
    expected_try_get_auth_token_calls_.emplace_back(
        ExpectedTryGetAuthTokensCall{
            .batch_size = batch_size,
            .bsa_tokens = absl::nullopt,
            .try_again_after = try_again_after,
        });
  }

  // True if all expected `TryGetAuthTokens` calls have occurred.
  bool GotAllExpectedMockCalls() {
    return expected_try_get_auth_token_calls_.empty();
  }

  // Reset all test expectations.
  void Reset() { expected_try_get_auth_token_calls_.clear(); }

  void TryGetAuthTokens(uint32_t batch_size,
                        network::mojom::IpProtectionProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override {
    ASSERT_FALSE(expected_try_get_auth_token_calls_.empty())
        << "Unexpected call to TryGetAuthTokens";
    auto& exp = expected_try_get_auth_token_calls_.front();
    EXPECT_EQ(batch_size, exp.batch_size);
    std::move(callback).Run(std::move(exp.bsa_tokens), exp.try_again_after);
    expected_try_get_auth_token_calls_.pop_front();
  }

  void GetProxyList(GetProxyListCallback callback) override {
    NOTREACHED_NORETURN();
  }

 protected:
  std::deque<ExpectedTryGetAuthTokensCall> expected_try_get_auth_token_calls_;
};

}  // namespace

struct HistogramState {
  // Number of successful requests (true).
  int success;
  // Number of failed requests (false).
  int failure;
};

class IpProtectionTokenCacheManagerImplTest : public testing::Test {
 protected:
  IpProtectionTokenCacheManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        mock_(),
        receiver_(&mock_) {
    remote_ = mojo::Remote<network::mojom::IpProtectionConfigGetter>();
    remote_.Bind(receiver_.BindNewPipeAndPassRemote());
    ipp_token_cache_manager_ =
        std::make_unique<IpProtectionTokenCacheManagerImpl>(
            &remote_, network::mojom::IpProtectionProxyLayer::kProxyA,
            /* disable_cache_management_for_testing=*/true);
  }

  void ExpectHistogramState(HistogramState state) {
    histogram_tester_.ExpectBucketCount(kGetAuthTokenResultHistogram, true,
                                        state.success);
    histogram_tester_.ExpectBucketCount(kGetAuthTokenResultHistogram, false,
                                        state.failure);
  }

  // Create a batch of tokens.
  std::vector<network::mojom::BlindSignedAuthTokenPtr> TokenBatch(
      int count,
      base::Time expiration) {
    std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
    for (int i = 0; i < count; i++) {
      tokens.emplace_back(network::mojom::BlindSignedAuthToken::New(
          base::StringPrintf("token-%d", i), expiration));
    }
    return tokens;
  }

  void CallTryGetAuthTokensAndWait() {
    ipp_token_cache_manager_->SetOnTryGetAuthTokensCompletedForTesting(
        task_environment_.QuitClosure());
    ipp_token_cache_manager_->CallTryGetAuthTokensForTesting();
    task_environment_.RunUntilQuit();
  }

  // Wait until the cache fills itself.
  void WaitForTryGetAuthTokensCompletion() {
    ipp_token_cache_manager_->SetOnTryGetAuthTokensCompletedForTesting(
        task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  int expected_batch_size_ =
      net::features::kIpPrivacyAuthTokenCacheBatchSize.Get();
  int cache_low_water_mark_ =
      net::features::kIpPrivacyAuthTokenCacheLowWaterMark.Get();

  // Expiration times with respect to the TaskEnvironment's mock time.
  const base::Time kFutureExpiration = base::Time::Now() + base::Hours(1);
  const base::Time kPastExpiration = base::Time::Now() - base::Hours(1);

  MockIpProtectionConfigGetter mock_;

  mojo::Receiver<network::mojom::IpProtectionConfigGetter> receiver_;

  mojo::Remote<network::mojom::IpProtectionConfigGetter> remote_;

  std::unique_ptr<IpProtectionTokenCacheManagerImpl> ipp_token_cache_manager_;

  base::HistogramTester histogram_tester_;
};

// `IsAuthTokenAvailable()` returns false on an empty cache.
TEST_F(IpProtectionTokenCacheManagerImplTest, IsAuthTokenAvailableFalseEmpty) {
  EXPECT_FALSE(ipp_token_cache_manager_->IsAuthTokenAvailable());
}

// `IsAuthTokenAvailable()` returns true on a cache containing unexpired tokens.
TEST_F(IpProtectionTokenCacheManagerImplTest, IsAuthTokenAvailableTrue) {
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(1, kFutureExpiration));
  CallTryGetAuthTokensAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_token_cache_manager_->IsAuthTokenAvailable());
}

// `IsAuthTokenAvailable()` returns false on a cache containing expired tokens.
TEST_F(IpProtectionTokenCacheManagerImplTest,
       IsAuthTokenAvailableFalseExpired) {
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(1, kPastExpiration));
  CallTryGetAuthTokensAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_FALSE(ipp_token_cache_manager_->IsAuthTokenAvailable());
}

// `GetAuthToken()` returns nullopt on an empty cache.
TEST_F(IpProtectionTokenCacheManagerImplTest, GetAuthTokenEmpty) {
  EXPECT_FALSE(ipp_token_cache_manager_->GetAuthToken());
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// `GetAuthToken()` returns a token on a cache containing unexpired tokens.
TEST_F(IpProtectionTokenCacheManagerImplTest, GetAuthTokenTrue) {
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(1, kFutureExpiration));
  CallTryGetAuthTokensAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> token =
      ipp_token_cache_manager_->GetAuthToken();
  ASSERT_TRUE(token);
  EXPECT_EQ((*token)->token, "token-0");
  EXPECT_EQ((*token)->expiration, kFutureExpiration);
  ExpectHistogramState(HistogramState{.success = 1, .failure = 0});
}

// `GetAuthToken()` returns nullopt on a cache containing expired tokens.
TEST_F(IpProtectionTokenCacheManagerImplTest, GetAuthTokenFalseExpired) {
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(1, kPastExpiration));
  CallTryGetAuthTokensAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_FALSE(ipp_token_cache_manager_->GetAuthToken());
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// If `TryGetAuthTokens()` returns an empty batch, the cache remains empty.
TEST_F(IpProtectionTokenCacheManagerImplTest, EmptyBatch) {
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(0, kFutureExpiration));
  CallTryGetAuthTokensAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  ASSERT_FALSE(ipp_token_cache_manager_->IsAuthTokenAvailable());
  ASSERT_FALSE(ipp_token_cache_manager_->GetAuthToken());
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// If `TryGetAuthTokens()` returns an backoff due to an error, the cache remains
// empty.
TEST_F(IpProtectionTokenCacheManagerImplTest, ErrorBatch) {
  const base::TimeDelta kBackoff = base::Seconds(10);
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   base::Time::Now() + kBackoff);
  CallTryGetAuthTokensAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  ASSERT_FALSE(ipp_token_cache_manager_->IsAuthTokenAvailable());
  ASSERT_FALSE(ipp_token_cache_manager_->GetAuthToken());
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// `GetAuthToken()` skips expired tokens and returns a non-expired token,
// if one is found in the cache.
TEST_F(IpProtectionTokenCacheManagerImplTest, SkipExpiredTokens) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens =
      TokenBatch(10, kPastExpiration);
  tokens.emplace_back(network::mojom::BlindSignedAuthToken::New(
      "good-token", kFutureExpiration));
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, std::move(tokens));
  CallTryGetAuthTokensAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  auto got_token = ipp_token_cache_manager_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "good-token");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
  ExpectHistogramState(HistogramState{.success = 1, .failure = 0});
}

// If the `IpProtectionConfigGetter` is nullptr, no tokens are gotten,
// but things don't crash.
TEST_F(IpProtectionTokenCacheManagerImplTest, NullGetter) {
  auto ipp_token_cache_manager = IpProtectionTokenCacheManagerImpl(
      nullptr, network::mojom::IpProtectionProxyLayer::kProxyA,
      /* disable_cache_management_for_testing=*/true);
  EXPECT_FALSE(ipp_token_cache_manager_->IsAuthTokenAvailable());
  auto token = ipp_token_cache_manager.GetAuthToken();
  ASSERT_FALSE(token);
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// Verify that the token spend rate is measured correctly.
TEST_F(IpProtectionTokenCacheManagerImplTest, TokenSpendRate) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;

  // Fill the cache with 5 tokens.
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(5, kFutureExpiration));
  CallTryGetAuthTokensAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Get four tokens from the batch.
  for (int i = 0; i < 4; i++) {
    auto got_token = ipp_token_cache_manager_->GetAuthToken();
    EXPECT_EQ(got_token.value()->token, base::StringPrintf("token-%d", i));
    EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
  }

  // Fast-forward to run the measurement timer.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // Four tokens in five minutes is a rate of 36 tokens per hour.
  histogram_tester_.ExpectUniqueSample(kTokenSpendRateHistogram, 48, 1);

  // Get the remaining token in the batch.
  auto got_token = ipp_token_cache_manager_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "token-4");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);

  // Fast-forward to run the measurement timer again, for another interval.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // One token in five minutes is a rate of 12 tokens per hour.
  histogram_tester_.ExpectBucketCount(kTokenSpendRateHistogram, 12, 1);
  histogram_tester_.ExpectTotalCount(kTokenSpendRateHistogram, 2);
}

// Verify that the token expiration rate is measured correctly.
TEST_F(IpProtectionTokenCacheManagerImplTest, TokenExpirationRate) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;

  // Fill the cache with 1024 expired tokens. An entire batch expiring
  // in one 5-minute interval is a very likely event.
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(1024, kPastExpiration));
  CallTryGetAuthTokensAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Try to get a token, which will incidentally record the expired tokens.
  auto got_token = ipp_token_cache_manager_->GetAuthToken();
  EXPECT_FALSE(got_token);

  // Fast-forward to run the measurement timer.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // 1024 tokens in five minutes is a rate of 12288 tokens per hour.
  histogram_tester_.ExpectUniqueSample(kTokenExpirationRateHistogram, 12288, 1);

  // Fast-forward to run the measurement timer again.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // Zero tokens expired in this interval.
  histogram_tester_.ExpectBucketCount(kTokenExpirationRateHistogram, 0, 1);
  histogram_tester_.ExpectTotalCount(kTokenExpirationRateHistogram, 2);
}

// The cache will pre-fill itself with a batch of tokens after a startup delay.
TEST_F(IpProtectionTokenCacheManagerImplTest, Prefill) {
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration));
  ipp_token_cache_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_token_cache_manager_->IsAuthTokenAvailable());
}

// The cache will initiate a refill when it reaches the low-water mark.
TEST_F(IpProtectionTokenCacheManagerImplTest, RefillLowWaterMark) {
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration));
  ipp_token_cache_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Spend tokens down to (but not below) the low-water mark.
  for (int i = expected_batch_size_ - 1; i > cache_low_water_mark_; i--) {
    ASSERT_TRUE(ipp_token_cache_manager_->IsAuthTokenAvailable());
    ASSERT_TRUE(ipp_token_cache_manager_->GetAuthToken());
    ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  }

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  // Next call to `GetAuthToken()` should call `MaybeRefillCache()`.
  ipp_token_cache_manager_->SetOnTryGetAuthTokensCompletedForTesting(
      task_environment_.QuitClosure());
  ASSERT_TRUE(ipp_token_cache_manager_->GetAuthToken());
  task_environment_.RunUntilQuit();

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
}

// If a fill results in a backoff request, the cache will try again after that
// time.
TEST_F(IpProtectionTokenCacheManagerImplTest, RefillAfterBackoff) {
  base::Time try_again_at = base::Time::Now() + base::Seconds(20);
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, try_again_at);
  ipp_token_cache_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  base::Time try_again_at_2 = base::Time::Now() + base::Seconds(20);
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, try_again_at_2);
  WaitForTryGetAuthTokensCompletion();
  EXPECT_EQ(base::Time::Now(), try_again_at);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  base::Time try_again_at_3 = base::Time::Now() + base::Seconds(20);
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, try_again_at_3);
  WaitForTryGetAuthTokensCompletion();
  EXPECT_EQ(base::Time::Now(), try_again_at_2);
}

// When enough tokens expire to bring the cache size below the low water mark,
// it will automatically refill.
TEST_F(IpProtectionTokenCacheManagerImplTest, RefillAfterExpiration) {
  // Make a batch of tokens almost all with `expiration2`, except one expiring
  // sooner and the one expiring later. These are returned in incorrect order to
  // verify that the cache sorts by expiration time.
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
  base::Time expiration1 = base::Time::Now() + base::Minutes(10);
  base::Time expiration2 = base::Time::Now() + base::Minutes(15);
  base::Time expiration3 = base::Time::Now() + base::Minutes(20);
  for (int i = 0; i < expected_batch_size_ - 2; i++) {
    tokens.emplace_back(
        network::mojom::BlindSignedAuthToken::New("exp2", expiration2));
  }
  tokens.emplace_back(
      network::mojom::BlindSignedAuthToken::New("exp3", expiration3));
  tokens.emplace_back(
      network::mojom::BlindSignedAuthToken::New("exp1", expiration1));
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, std::move(tokens));
  ipp_token_cache_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // After the first expiration, tokens should still be available and no
  // refill should have begun (which would have caused an error).
  task_environment_.FastForwardBy(expiration1 - base::Time::Now());
  ASSERT_TRUE(ipp_token_cache_manager_->IsAuthTokenAvailable());

  // After the second expiration, tokens should still be available, and
  // a second batch should have been requested.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration));
  task_environment_.FastForwardBy(expiration2 - base::Time::Now());
  ASSERT_TRUE(ipp_token_cache_manager_->IsAuthTokenAvailable());

  // The un-expired token should be returned.
  auto got_token = ipp_token_cache_manager_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "exp3");
}

}  // namespace network
