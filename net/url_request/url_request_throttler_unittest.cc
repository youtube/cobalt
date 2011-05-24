// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "net/base/test_completion_callback.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_throttler_header_interface.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {

namespace {
class MockURLRequestThrottlerManager;

class MockBackoffEntry : public BackoffEntry {
 public:
  explicit MockBackoffEntry(const BackoffEntry::Policy* const policy)
      : BackoffEntry(policy), fake_now_(TimeTicks()) {
  }

  virtual ~MockBackoffEntry() {}

  TimeTicks GetTimeNow() const {
    return fake_now_;
  }

  void SetFakeNow(const TimeTicks& now) {
    fake_now_ = now;
  }

 private:
  TimeTicks fake_now_;
};

class MockURLRequestThrottlerEntry : public URLRequestThrottlerEntry {
 public :
  explicit MockURLRequestThrottlerEntry(
      net::URLRequestThrottlerManager* manager)
      : net::URLRequestThrottlerEntry(manager),
        mock_backoff_entry_(&backoff_policy_) {
    InitPolicy();
  }
  MockURLRequestThrottlerEntry(
      net::URLRequestThrottlerManager* manager,
      const TimeTicks& exponential_backoff_release_time,
      const TimeTicks& sliding_window_release_time,
      const TimeTicks& fake_now)
      : net::URLRequestThrottlerEntry(manager),
        fake_time_now_(fake_now),
        mock_backoff_entry_(&backoff_policy_) {
    InitPolicy();

    mock_backoff_entry_.SetFakeNow(fake_now);
    set_exponential_backoff_release_time(exponential_backoff_release_time);
    set_sliding_window_release_time(sliding_window_release_time);
  }
  virtual ~MockURLRequestThrottlerEntry() {}

  void InitPolicy() {
    // Some tests become flaky if we have jitter.
    backoff_policy_.jitter_factor = 0.0;

    // This lets us avoid having to make multiple failures initially (this
    // logic is already tested in the BackoffEntry unit tests).
    backoff_policy_.num_errors_to_ignore = 0;
  }

  const BackoffEntry* GetBackoffEntry() const {
    return &mock_backoff_entry_;
  }

  BackoffEntry* GetBackoffEntry() {
    return &mock_backoff_entry_;
  }

  void ResetToBlank(const TimeTicks& time_now) {
    fake_time_now_ = time_now;
    mock_backoff_entry_.SetFakeNow(time_now);

    GetBackoffEntry()->InformOfRequest(true);  // Sets failure count to 0.
    GetBackoffEntry()->SetCustomReleaseTime(time_now);
    set_sliding_window_release_time(time_now);
  }

  // Overridden for tests.
  virtual TimeTicks GetTimeNow() const { return fake_time_now_; }

  void set_exponential_backoff_release_time(
      const base::TimeTicks& release_time) {
    GetBackoffEntry()->SetCustomReleaseTime(release_time);
  }

  base::TimeTicks sliding_window_release_time() const {
    return URLRequestThrottlerEntry::sliding_window_release_time();
  }

  void set_sliding_window_release_time(
      const base::TimeTicks& release_time) {
    URLRequestThrottlerEntry::set_sliding_window_release_time(
        release_time);
  }

  TimeTicks fake_time_now_;
  MockBackoffEntry mock_backoff_entry_;
};

class MockURLRequestThrottlerHeaderAdapter
    : public URLRequestThrottlerHeaderInterface {
 public:
  MockURLRequestThrottlerHeaderAdapter()
      : fake_retry_value_(""),
        fake_opt_out_value_(""),
        fake_response_code_(0) {
  }

  explicit MockURLRequestThrottlerHeaderAdapter(int response_code)
      : fake_retry_value_(""),
        fake_opt_out_value_(""),
        fake_response_code_(response_code) {
  }

  MockURLRequestThrottlerHeaderAdapter(const std::string& retry_value,
                                       const std::string& opt_out_value,
                                       int response_code)
      : fake_retry_value_(retry_value),
        fake_opt_out_value_(opt_out_value),
        fake_response_code_(response_code) {
  }

  virtual ~MockURLRequestThrottlerHeaderAdapter() {}

  virtual std::string GetNormalizedValue(const std::string& key) const {
    if (key == MockURLRequestThrottlerEntry::kRetryHeaderName &&
        !fake_retry_value_.empty()) {
      return fake_retry_value_;
    } else if (key ==
        MockURLRequestThrottlerEntry::kExponentialThrottlingHeader &&
        !fake_opt_out_value_.empty()) {
      return fake_opt_out_value_;
    }
    return "";
  }

  virtual int GetResponseCode() const { return fake_response_code_; }

  std::string fake_retry_value_;
  std::string fake_opt_out_value_;
  int fake_response_code_;
};

class MockURLRequestThrottlerManager : public URLRequestThrottlerManager {
 public:
  MockURLRequestThrottlerManager() : create_entry_index_(0) {}

  // Method to process the URL using URLRequestThrottlerManager protected
  // method.
  std::string DoGetUrlIdFromUrl(const GURL& url) { return GetIdFromUrl(url); }

  // Method to use the garbage collecting method of URLRequestThrottlerManager.
  void DoGarbageCollectEntries() { GarbageCollectEntries(); }

  // Returns the number of entries in the map.
  int GetNumberOfEntries() const { return GetNumberOfEntriesForTests(); }

  void CreateEntry(bool is_outdated) {
    TimeTicks time = TimeTicks::Now();
    if (is_outdated) {
      time -= TimeDelta::FromMilliseconds(
          MockURLRequestThrottlerEntry::kDefaultEntryLifetimeMs + 1000);
    }
    std::string fake_url_string("http://www.fakeurl.com/");
    fake_url_string.append(base::IntToString(create_entry_index_++));
    GURL fake_url(fake_url_string);
    OverrideEntryForTests(
        fake_url,
        new MockURLRequestThrottlerEntry(this, time, TimeTicks::Now(),
                                         TimeTicks::Now()));
  }

 private:
  int create_entry_index_;
};

struct TimeAndBool {
  TimeAndBool(const TimeTicks& time_value, bool expected, int line_num) {
    time = time_value;
    result = expected;
    line = line_num;
  }
  TimeTicks time;
  bool result;
  int line;
};

struct GurlAndString {
  GurlAndString(const GURL& url_value,
                const std::string& expected,
                int line_num) {
    url = url_value;
    result = expected;
    line = line_num;
  }
  GURL url;
  std::string result;
  int line;
};

}  // namespace

class URLRequestThrottlerEntryTest : public testing::Test {
 protected:
  virtual void SetUp();
  TimeTicks now_;
  MockURLRequestThrottlerManager manager_;  // Dummy object, not used.
  scoped_refptr<MockURLRequestThrottlerEntry> entry_;
};

void URLRequestThrottlerEntryTest::SetUp() {
  now_ = TimeTicks::Now();
  entry_ = new MockURLRequestThrottlerEntry(&manager_);
  entry_->ResetToBlank(now_);
}

std::ostream& operator<<(std::ostream& out, const base::TimeTicks& time) {
  return out << time.ToInternalValue();
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceDuringExponentialBackoff) {
  entry_->set_exponential_backoff_release_time(
      entry_->fake_time_now_ + TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(entry_->IsDuringExponentialBackoff());
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceNotDuringExponentialBackoff) {
  entry_->set_exponential_backoff_release_time(entry_->fake_time_now_);
  EXPECT_FALSE(entry_->IsDuringExponentialBackoff());
  entry_->set_exponential_backoff_release_time(
      entry_->fake_time_now_ - TimeDelta::FromMilliseconds(1));
  EXPECT_FALSE(entry_->IsDuringExponentialBackoff());
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateRetryAfter) {
  // If the response we received has a retry-after field,
  // the request should be delayed.
  MockURLRequestThrottlerHeaderAdapter header_w_delay_header("5.5", "", 200);
  entry_->UpdateWithResponse("", &header_w_delay_header);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "When the server put a positive value in retry-after we should "
         "increase release_time";

  entry_->ResetToBlank(now_);
  header_w_delay_header.fake_retry_value_ = "-5.5";
  EXPECT_EQ(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "When given a negative value, it should not change the release_time";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateFailure) {
  MockURLRequestThrottlerHeaderAdapter failure_response(505);
  entry_->UpdateWithResponse("", &failure_response);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "A failure should increase the release_time";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateSuccess) {
  MockURLRequestThrottlerHeaderAdapter success_response(200);
  entry_->UpdateWithResponse("", &success_response);
  EXPECT_EQ(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "A success should not add any delay";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateSuccessThenFailure) {
  MockURLRequestThrottlerHeaderAdapter failure_response(500);
  MockURLRequestThrottlerHeaderAdapter success_response(200);
  entry_->UpdateWithResponse("", &success_response);
  entry_->UpdateWithResponse("", &failure_response);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "This scenario should add delay";
}

TEST_F(URLRequestThrottlerEntryTest, IsEntryReallyOutdated) {
  TimeDelta lifetime = TimeDelta::FromMilliseconds(
      MockURLRequestThrottlerEntry::kDefaultEntryLifetimeMs);
  const TimeDelta kFiveMs = TimeDelta::FromMilliseconds(5);

  TimeAndBool test_values[] = {
      TimeAndBool(now_, false, __LINE__),
      TimeAndBool(now_ - kFiveMs, false, __LINE__),
      TimeAndBool(now_ + kFiveMs, false, __LINE__),
      TimeAndBool(now_ - (lifetime - kFiveMs), false, __LINE__),
      TimeAndBool(now_ - lifetime, true, __LINE__),
      TimeAndBool(now_ - (lifetime + kFiveMs), true, __LINE__)};

  for (unsigned int i = 0; i < arraysize(test_values); ++i) {
    entry_->set_exponential_backoff_release_time(test_values[i].time);
    EXPECT_EQ(entry_->IsEntryOutdated(), test_values[i].result) <<
        "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST_F(URLRequestThrottlerEntryTest, MaxAllowedBackoff) {
  for (int i = 0; i < 30; ++i) {
    MockURLRequestThrottlerHeaderAdapter response_adapter(505);
    entry_->UpdateWithResponse("", &response_adapter);
  }

  TimeDelta delay = entry_->GetExponentialBackoffReleaseTime() - now_;
  EXPECT_EQ(delay.InMilliseconds(),
            MockURLRequestThrottlerEntry::kDefaultMaximumBackoffMs);
}

TEST_F(URLRequestThrottlerEntryTest, MalformedContent) {
  MockURLRequestThrottlerHeaderAdapter response_adapter(505);
  for (int i = 0; i < 5; ++i)
    entry_->UpdateWithResponse("", &response_adapter);

  TimeTicks release_after_failures = entry_->GetExponentialBackoffReleaseTime();

  // Inform the entry that a response body was malformed, which is supposed to
  // increase the back-off time.  Note that we also submit a successful
  // UpdateWithResponse to pair with ReceivedContentWasMalformed() since that
  // is what happens in practice (if a body is received, then a non-500
  // response must also have been received).
  entry_->ReceivedContentWasMalformed();
  MockURLRequestThrottlerHeaderAdapter success_adapter(200);
  entry_->UpdateWithResponse("", &success_adapter);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), release_after_failures);
}

TEST_F(URLRequestThrottlerEntryTest, SlidingWindow) {
  int max_send = URLRequestThrottlerEntry::kDefaultMaxSendThreshold;
  int sliding_window =
      URLRequestThrottlerEntry::kDefaultSlidingWindowPeriodMs;

  TimeTicks time_1 = entry_->fake_time_now_ +
      TimeDelta::FromMilliseconds(sliding_window / 3);
  TimeTicks time_2 = entry_->fake_time_now_ +
      TimeDelta::FromMilliseconds(2 * sliding_window / 3);
  TimeTicks time_3 = entry_->fake_time_now_ +
      TimeDelta::FromMilliseconds(sliding_window);
  TimeTicks time_4 = entry_->fake_time_now_ +
      TimeDelta::FromMilliseconds(sliding_window + 2 * sliding_window / 3);

  entry_->set_exponential_backoff_release_time(time_1);

  for (int i = 0; i < max_send / 2; ++i) {
    EXPECT_EQ(2 * sliding_window / 3,
              entry_->ReserveSendingTimeForNextRequest(time_2));
  }
  EXPECT_EQ(time_2, entry_->sliding_window_release_time());

  entry_->fake_time_now_ = time_3;

  for (int i = 0; i < (max_send + 1) / 2; ++i)
    EXPECT_EQ(0, entry_->ReserveSendingTimeForNextRequest(TimeTicks()));

  EXPECT_EQ(time_4, entry_->sliding_window_release_time());
}

TEST(URLRequestThrottlerManager, IsUrlStandardised) {
  MockURLRequestThrottlerManager manager;
  GurlAndString test_values[] = {
      GurlAndString(GURL("http://www.example.com"),
                    std::string("http://www.example.com/"),
                    __LINE__),
      GurlAndString(GURL("http://www.Example.com"),
                    std::string("http://www.example.com/"),
                    __LINE__),
      GurlAndString(GURL("http://www.ex4mple.com/Pr4c71c41"),
                    std::string("http://www.ex4mple.com/pr4c71c41"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/0/token/false"),
                    std::string("http://www.example.com/0/token/false"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=javascript"),
                    std::string("http://www.example.com/index.php"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=1#superEntry"),
                    std::string("http://www.example.com/index.php"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com:1234/"),
                    std::string("http://www.example.com:1234/"),
                    __LINE__)};

  for (unsigned int i = 0; i < arraysize(test_values); ++i) {
    std::string temp = manager.DoGetUrlIdFromUrl(test_values[i].url);
    EXPECT_EQ(temp, test_values[i].result) <<
        "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST(URLRequestThrottlerManager, AreEntriesBeingCollected) {
  MockURLRequestThrottlerManager manager;

  manager.CreateEntry(true);  // true = Entry is outdated.
  manager.CreateEntry(true);
  manager.CreateEntry(true);
  manager.DoGarbageCollectEntries();
  EXPECT_EQ(0, manager.GetNumberOfEntries());

  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(true);
  manager.DoGarbageCollectEntries();
  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

TEST(URLRequestThrottlerManager, IsHostBeingRegistered) {
  MockURLRequestThrottlerManager manager;

  manager.RegisterRequestUrl(GURL("http://www.example.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0?code=1"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0#lolsaure"));

  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

void ExpectEntryAllowsAllOnErrorIfOptedOut(
    net::URLRequestThrottlerEntryInterface* entry,
    bool opted_out) {
  EXPECT_FALSE(entry->IsDuringExponentialBackoff());
  MockURLRequestThrottlerHeaderAdapter failure_adapter(503);
  for (int i = 0; i < 10; ++i) {
    // Host doesn't really matter in this scenario so we skip it.
    entry->UpdateWithResponse("", &failure_adapter);
  }
  EXPECT_NE(opted_out, entry->IsDuringExponentialBackoff());

  if (opted_out) {
    // We're not mocking out GetTimeNow() in this scenario
    // so add a 100 ms buffer to avoid flakiness (that should always
    // give enough time to get from the TimeTicks::Now() call here
    // to the TimeTicks::Now() call in the entry class).
    EXPECT_GT(TimeTicks::Now() + TimeDelta::FromMilliseconds(100),
              entry->GetExponentialBackoffReleaseTime());
  } else {
    // As above, add 100 ms.
    EXPECT_LT(TimeTicks::Now() + TimeDelta::FromMilliseconds(100),
              entry->GetExponentialBackoffReleaseTime());
  }
}

TEST(URLRequestThrottlerManager, OptOutHeader) {
  MockURLRequestThrottlerManager manager;
  scoped_refptr<net::URLRequestThrottlerEntryInterface> entry =
      manager.RegisterRequestUrl(GURL("http://www.google.com/yodude"));

  // Fake a response with the opt-out header.
  MockURLRequestThrottlerHeaderAdapter response_adapter(
      "",
      MockURLRequestThrottlerEntry::kExponentialThrottlingDisableValue,
      200);
  entry->UpdateWithResponse("www.google.com", &response_adapter);

  // Ensure that the same entry on error always allows everything.
  ExpectEntryAllowsAllOnErrorIfOptedOut(entry, true);

  // Ensure that a freshly created entry (for a different URL on an
  // already opted-out host) also gets "always allow" behavior.
  scoped_refptr<net::URLRequestThrottlerEntryInterface> other_entry =
      manager.RegisterRequestUrl(GURL("http://www.google.com/bingobob"));
  ExpectEntryAllowsAllOnErrorIfOptedOut(other_entry, true);

  // Fake a response with the opt-out header incorrectly specified.
  scoped_refptr<net::URLRequestThrottlerEntryInterface> no_opt_out_entry =
      manager.RegisterRequestUrl(GURL("http://www.nike.com/justdoit"));
  MockURLRequestThrottlerHeaderAdapter wrong_adapter("", "yesplease", 200);
  no_opt_out_entry->UpdateWithResponse("www.nike.com", &wrong_adapter);
  ExpectEntryAllowsAllOnErrorIfOptedOut(no_opt_out_entry, false);

  // A localhost entry should always be opted out.
  scoped_refptr<net::URLRequestThrottlerEntryInterface> localhost_entry =
      manager.RegisterRequestUrl(GURL("http://localhost/hello"));
  ExpectEntryAllowsAllOnErrorIfOptedOut(localhost_entry, true);
}

}  // namespace net
