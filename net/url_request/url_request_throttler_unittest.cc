// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "base/scoped_ptr.h"
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

namespace {
class MockURLRequestThrottlerManager;

class MockURLRequestThrottlerEntry : public net::URLRequestThrottlerEntry {
 public :
  MockURLRequestThrottlerEntry() {}
  MockURLRequestThrottlerEntry(
      const TimeTicks& exponential_backoff_release_time,
      const TimeTicks& sliding_window_release_time,
      const TimeTicks& fake_now)
      : fake_time_now_(fake_now) {
    set_exponential_backoff_release_time(exponential_backoff_release_time);
    set_sliding_window_release_time(sliding_window_release_time);
  }
  virtual ~MockURLRequestThrottlerEntry() {}

  void ResetToBlank(const TimeTicks& time_now) {
    fake_time_now_ = time_now;
    set_exponential_backoff_release_time(time_now);
    set_failure_count(0);
    set_sliding_window_release_time(time_now);
  }

  // Overridden for tests.
  virtual TimeTicks GetTimeNow() const { return fake_time_now_; }

  void set_exponential_backoff_release_time(
      const base::TimeTicks& release_time) {
    net::URLRequestThrottlerEntry::set_exponential_backoff_release_time(
        release_time);
  }

  base::TimeTicks sliding_window_release_time() const {
    return net::URLRequestThrottlerEntry::sliding_window_release_time();
  }

  void set_sliding_window_release_time(
      const base::TimeTicks& release_time) {
    net::URLRequestThrottlerEntry::set_sliding_window_release_time(
        release_time);
  }

  TimeTicks fake_time_now_;
};

class MockURLRequestThrottlerHeaderAdapter
    : public net::URLRequestThrottlerHeaderInterface {
 public:
  MockURLRequestThrottlerHeaderAdapter()
      : fake_retry_value_("0.0"),
        fake_response_code_(0) {
  }

  MockURLRequestThrottlerHeaderAdapter(const std::string& retry_value,
                                       int response_code)
      : fake_retry_value_(retry_value),
        fake_response_code_(response_code) {
  }

  virtual ~MockURLRequestThrottlerHeaderAdapter() {}

  virtual std::string GetNormalizedValue(const std::string& key) const {
    if (key == MockURLRequestThrottlerEntry::kRetryHeaderName)
      return fake_retry_value_;
    return "";
  }

  virtual int GetResponseCode() const { return fake_response_code_; }

  std::string fake_retry_value_;
  int fake_response_code_;
};

class MockURLRequestThrottlerManager : public net::URLRequestThrottlerManager {
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
        new MockURLRequestThrottlerEntry(time, TimeTicks::Now(),
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
  scoped_refptr<MockURLRequestThrottlerEntry> entry_;
};

void URLRequestThrottlerEntryTest::SetUp() {
  now_ = TimeTicks::Now();
  entry_ = new MockURLRequestThrottlerEntry();
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
  MockURLRequestThrottlerHeaderAdapter header_w_delay_header("5.5", 200);
  entry_->UpdateWithResponse(&header_w_delay_header);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "When the server put a positive value in retry-after we should "
         "increase release_time";

  entry_->ResetToBlank(now_);
  header_w_delay_header.fake_retry_value_ = "-5.5";
  EXPECT_EQ(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "When given a negative value, it should not change the release_time";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateFailure) {
  MockURLRequestThrottlerHeaderAdapter failure_response("0", 505);
  entry_->UpdateWithResponse(&failure_response);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "A failure should increase the release_time";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateSuccess) {
  MockURLRequestThrottlerHeaderAdapter success_response("0", 200);
  entry_->UpdateWithResponse(&success_response);
  EXPECT_EQ(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "A success should not add any delay";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateSuccessThenFailure) {
  MockURLRequestThrottlerHeaderAdapter failure_response("0", 500);
  MockURLRequestThrottlerHeaderAdapter success_response("0", 200);
  entry_->UpdateWithResponse(&success_response);
  entry_->UpdateWithResponse(&failure_response);
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
      TimeAndBool(now_ - lifetime, false, __LINE__),
      TimeAndBool(now_ - (lifetime + kFiveMs), true, __LINE__)};

  for (unsigned int i = 0; i < arraysize(test_values); ++i) {
    entry_->set_exponential_backoff_release_time(test_values[i].time);
    EXPECT_EQ(entry_->IsEntryOutdated(), test_values[i].result) <<
        "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST_F(URLRequestThrottlerEntryTest, MaxAllowedBackoff) {
  for (int i = 0; i < 30; ++i) {
    MockURLRequestThrottlerHeaderAdapter response_adapter("0.0", 505);
    entry_->UpdateWithResponse(&response_adapter);
  }

  TimeDelta delay = entry_->GetExponentialBackoffReleaseTime() - now_;
  EXPECT_EQ(delay.InMilliseconds(),
            MockURLRequestThrottlerEntry::kDefaultMaximumBackoffMs);
}

TEST_F(URLRequestThrottlerEntryTest, MalformedContent) {
  MockURLRequestThrottlerHeaderAdapter response_adapter("0.0", 505);
  for (int i = 0; i < 5; ++i)
    entry_->UpdateWithResponse(&response_adapter);

  TimeTicks release_after_failures = entry_->GetExponentialBackoffReleaseTime();

  // Inform the entry that a response body was malformed, which is supposed to
  // increase the back-off time.
  entry_->ReceivedContentWasMalformed();
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), release_after_failures);
}

TEST_F(URLRequestThrottlerEntryTest, SlidingWindow) {
  int max_send = net::URLRequestThrottlerEntry::kDefaultMaxSendThreshold;
  int sliding_window =
      net::URLRequestThrottlerEntry::kDefaultSlidingWindowPeriodMs;

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
                    std::string("GOOGYhttp://www.example.com/MONSTA"),
                    __LINE__),
      GurlAndString(GURL("http://www.Example.com"),
                    std::string("GOOGYhttp://www.example.com/MONSTA"),
                    __LINE__),
      GurlAndString(GURL("http://www.ex4mple.com/Pr4c71c41"),
                    std::string("GOOGYhttp://www.ex4mple.com/pr4c71c41MONSTA"),
                    __LINE__),
      GurlAndString(
          GURL("http://www.example.com/0/token/false"),
          std::string("GOOGYhttp://www.example.com/0/token/falseMONSTA"),
          __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=javascript"),
                    std::string("GOOGYhttp://www.example.com/index.phpMONSTA"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=1#superEntry"),
                    std::string("GOOGYhttp://www.example.com/index.phpMONSTA"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com:1234/"),
                    std::string("GOOGYhttp://www.example.com:1234/MONSTA"),
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

class DummyResponseHeaders : public net::URLRequestThrottlerHeaderInterface {
 public:
  DummyResponseHeaders(int response_code) : response_code_(response_code) {}

  std::string GetNormalizedValue(const std::string& key) const {
    return "";
  }

  // Returns the HTTP response code associated with the request.
  int GetResponseCode() const {
    return response_code_;
  }

 private:
  int response_code_;
};

TEST(URLRequestThrottlerManager, StressTest) {
  MockURLRequestThrottlerManager manager;

  for (int i = 0; i < 10000; ++i) {
    // Make some entries duplicates or triplicates.
    int entry_number = i + 2;
    if (i % 17 == 0) {
      entry_number = i - 1;
    } else if ((i - 1) % 17 == 0) {
      entry_number = i - 2;
    } else if (i % 13 == 0) {
      entry_number = i - 1;
    }

    scoped_refptr<net::URLRequestThrottlerEntryInterface> entry =
        manager.RegisterRequestUrl(GURL(StringPrintf(
            "http://bingourl.com/%d", entry_number)));
    entry->IsDuringExponentialBackoff();
    entry->GetExponentialBackoffReleaseTime();

    DummyResponseHeaders headers(i % 7 == 0 ? 500 : 200);
    entry->UpdateWithResponse(&headers);
    entry->SetEntryLifetimeMsForTest(1);

    entry->IsDuringExponentialBackoff();
    entry->GetExponentialBackoffReleaseTime();
    entry->ReserveSendingTimeForNextRequest(base::TimeTicks::Now());

    if (i % 11 == 0) {
      manager.DoGarbageCollectEntries();
    }
  }
}

// TODO(joi): Remove the debug-only condition after M11 branch point.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(NDEBUG)
TEST(URLRequestThrottlerManager, NullHandlingTest) {
  MockURLRequestThrottlerManager manager;
  manager.OverrideEntryForTests(GURL("http://www.foo.com/"), NULL);
  ASSERT_DEATH({
      manager.DoGarbageCollectEntries();
  }, "");
}
#endif  // defined(GTEST_HAS_DEATH_TEST) && !defined(NDEBUG)
