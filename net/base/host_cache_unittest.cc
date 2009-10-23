// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_cache.h"

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
const int kMaxCacheEntries = 10;
const int kCacheDurationMs = 10000;  // 10 seconds.

// Builds a key for |hostname|, defaulting the address family to unspecified.
HostCache::Key Key(const std::string& hostname) {
  return HostCache::Key(hostname, ADDRESS_FAMILY_UNSPECIFIED);
}

}  // namespace

TEST(HostCacheTest, Basic) {
  HostCache cache(kMaxCacheEntries, kCacheDurationMs);

  // Start at t=0.
  base::TimeTicks now;

  const HostCache::Entry* entry1 = NULL;  // Entry for foobar.com.
  const HostCache::Entry* entry2 = NULL;  // Entry for foobar2.com.

  EXPECT_EQ(0U, cache.size());

  // Add an entry for "foobar.com" at t=0.
  EXPECT_TRUE(cache.Lookup(Key("foobar.com"), base::TimeTicks()) == NULL);
  cache.Set(Key("foobar.com"), OK, AddressList(), now);
  entry1 = cache.Lookup(Key("foobar.com"), base::TimeTicks());
  EXPECT_FALSE(entry1 == NULL);
  EXPECT_EQ(1U, cache.size());

  // Advance to t=5.
  now += base::TimeDelta::FromSeconds(5);

  // Add an entry for "foobar2.com" at t=5.
  EXPECT_TRUE(cache.Lookup(Key("foobar2.com"), base::TimeTicks()) == NULL);
  cache.Set(Key("foobar2.com"), OK, AddressList(), now);
  entry2 = cache.Lookup(Key("foobar2.com"), base::TimeTicks());
  EXPECT_FALSE(NULL == entry1);
  EXPECT_EQ(2U, cache.size());

  // Advance to t=9
  now += base::TimeDelta::FromSeconds(4);

  // Verify that the entries we added are still retrievable, and usable.
  EXPECT_EQ(entry1, cache.Lookup(Key("foobar.com"), now));
  EXPECT_EQ(entry2, cache.Lookup(Key("foobar2.com"), now));

  // Advance to t=10; entry1 is now expired.
  now += base::TimeDelta::FromSeconds(1);

  EXPECT_TRUE(cache.Lookup(Key("foobar.com"), now) == NULL);
  EXPECT_EQ(entry2, cache.Lookup(Key("foobar2.com"), now));

  // Update entry1, so it is no longer expired.
  cache.Set(Key("foobar.com"), OK, AddressList(), now);
  // Re-uses existing entry storage.
  EXPECT_EQ(entry1, cache.Lookup(Key("foobar.com"), now));
  EXPECT_EQ(2U, cache.size());

  // Both entries should still be retrievable and usable.
  EXPECT_EQ(entry1, cache.Lookup(Key("foobar.com"), now));
  EXPECT_EQ(entry2, cache.Lookup(Key("foobar2.com"), now));

  // Advance to t=20; both entries are now expired.
  now += base::TimeDelta::FromSeconds(10);

  EXPECT_TRUE(cache.Lookup(Key("foobar.com"), now) == NULL);
  EXPECT_TRUE(cache.Lookup(Key("foobar2.com"), now) == NULL);
}

// Try caching entries for a failed resolve attempt.
TEST(HostCacheTest, NegativeEntry) {
  HostCache cache(kMaxCacheEntries, kCacheDurationMs);

  // Set t=0.
  base::TimeTicks now;

  EXPECT_TRUE(cache.Lookup(Key("foobar.com"), base::TimeTicks()) == NULL);
  cache.Set(Key("foobar.com"), ERR_NAME_NOT_RESOLVED, AddressList(), now);
  EXPECT_EQ(1U, cache.size());

  // We disallow use of negative entries.
  EXPECT_TRUE(cache.Lookup(Key("foobar.com"), now) == NULL);

  // Now overwrite with a valid entry, and then overwrite with negative entry
  // again -- the valid entry should be kicked out.
  cache.Set(Key("foobar.com"), OK, AddressList(), now);
  EXPECT_FALSE(cache.Lookup(Key("foobar.com"), now) == NULL);
  cache.Set(Key("foobar.com"), ERR_NAME_NOT_RESOLVED, AddressList(), now);
  EXPECT_TRUE(cache.Lookup(Key("foobar.com"), now) == NULL);
}

TEST(HostCacheTest, Compact) {
  // Initial entries limit is big enough to accomadate everything we add.
  HostCache cache(kMaxCacheEntries, kCacheDurationMs);

  EXPECT_EQ(0U, cache.size());

  // t=10
  base::TimeTicks now = base::TimeTicks() + base::TimeDelta::FromSeconds(10);

  // Add five valid entries at t=10.
  for (int i = 0; i < 5; ++i) {
    std::string hostname = StringPrintf("valid%d", i);
    cache.Set(Key(hostname), OK, AddressList(), now);
  }
  EXPECT_EQ(5U, cache.size());

  // Add 3 expired entries at t=0.
  for (int i = 0; i < 3; ++i) {
    std::string hostname = StringPrintf("expired%d", i);
    base::TimeTicks t = now - base::TimeDelta::FromSeconds(10);
    cache.Set(Key(hostname), OK, AddressList(), t);
  }
  EXPECT_EQ(8U, cache.size());

  // Add 2 negative entries at t=10
  for (int i = 0; i < 2; ++i) {
    std::string hostname = StringPrintf("negative%d", i);
    cache.Set(Key(hostname), ERR_NAME_NOT_RESOLVED, AddressList(), now);
  }
  EXPECT_EQ(10U, cache.size());

  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid0")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid1")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid2")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid3")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid4")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("expired0")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("expired1")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("expired2")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("negative0")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("negative1")));

  // Shrink the max constraints bound and compact. We expect the "negative"
  // and "expired" entries to have been dropped.
  cache.max_entries_ = 5;
  cache.Compact(now, NULL);
  EXPECT_EQ(5U, cache.entries_.size());

  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid0")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid1")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid2")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid3")));
  EXPECT_TRUE(ContainsKey(cache.entries_, Key("valid4")));
  EXPECT_FALSE(ContainsKey(cache.entries_, Key("expired0")));
  EXPECT_FALSE(ContainsKey(cache.entries_, Key("expired1")));
  EXPECT_FALSE(ContainsKey(cache.entries_, Key("expired2")));
  EXPECT_FALSE(ContainsKey(cache.entries_, Key("negative0")));
  EXPECT_FALSE(ContainsKey(cache.entries_, Key("negative1")));

  // Shrink further -- this time the compact will start dropping valid entries
  // to make space.
  cache.max_entries_ = 3;
  cache.Compact(now, NULL);
  EXPECT_EQ(3U, cache.size());
}

// Add entries while the cache is at capacity, causing evictions.
TEST(HostCacheTest, SetWithCompact) {
  HostCache cache(3, kCacheDurationMs);

  // t=10
  base::TimeTicks now =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(kCacheDurationMs);

  cache.Set(Key("host1"), OK, AddressList(), now);
  cache.Set(Key("host2"), OK, AddressList(), now);
  cache.Set(Key("expired"), OK, AddressList(),
      now - base::TimeDelta::FromMilliseconds(kCacheDurationMs));

  EXPECT_EQ(3U, cache.size());

  // Should all be retrievable except "expired".
  EXPECT_FALSE(NULL == cache.Lookup(Key("host1"), now));
  EXPECT_FALSE(NULL == cache.Lookup(Key("host2"), now));
  EXPECT_TRUE(NULL == cache.Lookup(Key("expired"), now));

  // Adding the fourth entry will cause "expired" to be evicted.
  cache.Set(Key("host3"), OK, AddressList(), now);
  EXPECT_EQ(3U, cache.size());
  EXPECT_TRUE(cache.Lookup(Key("expired"), now) == NULL);
  EXPECT_FALSE(cache.Lookup(Key("host1"), now) == NULL);
  EXPECT_FALSE(cache.Lookup(Key("host2"), now) == NULL);
  EXPECT_FALSE(cache.Lookup(Key("host3"), now) == NULL);

  // Add two more entries. Something should be evicted, however "host5"
  // should definitely be in there (since it was last inserted).
  cache.Set(Key("host4"), OK, AddressList(), now);
  EXPECT_EQ(3U, cache.size());
  cache.Set(Key("host5"), OK, AddressList(), now);
  EXPECT_EQ(3U, cache.size());
  EXPECT_FALSE(cache.Lookup(Key("host5"), now) == NULL);
}

// Tests that the same hostname can be duplicated in the cache, so long as
// the address family differs.
TEST(HostCacheTest, AddressFamilyIsPartOfKey) {
  HostCache cache(kMaxCacheEntries, kCacheDurationMs);

  // t=0.
  base::TimeTicks now;

  HostCache::Key key1("foobar.com", ADDRESS_FAMILY_UNSPECIFIED);
  HostCache::Key key2("foobar.com", ADDRESS_FAMILY_IPV4);

  const HostCache::Entry* entry1 = NULL;  // Entry for key1
  const HostCache::Entry* entry2 = NULL;  // Entry for key2

  EXPECT_EQ(0U, cache.size());

  // Add an entry for ("foobar.com", UNSPECIFIED) at t=0.
  EXPECT_TRUE(cache.Lookup(key1, base::TimeTicks()) == NULL);
  cache.Set(key1, OK, AddressList(), now);
  entry1 = cache.Lookup(key1, base::TimeTicks());
  EXPECT_FALSE(entry1 == NULL);
  EXPECT_EQ(1U, cache.size());

  // Add an entry for ("foobar.com", IPV4_ONLY) at t=0.
  EXPECT_TRUE(cache.Lookup(key2, base::TimeTicks()) == NULL);
  cache.Set(key2, OK, AddressList(), now);
  entry2 = cache.Lookup(key2, base::TimeTicks());
  EXPECT_FALSE(entry2 == NULL);
  EXPECT_EQ(2U, cache.size());

  // Even though the hostnames were the same, we should have two unique
  // entries (because the address families differ).
  EXPECT_NE(entry1, entry2);
}

TEST(HostCacheTest, NoCache) {
  // Disable caching.
  HostCache cache(0, kCacheDurationMs);
  EXPECT_TRUE(cache.caching_is_disabled());

  // Set t=0.
  base::TimeTicks now;

  // Lookup and Set should have no effect.
  EXPECT_TRUE(cache.Lookup(Key("foobar.com"), base::TimeTicks()) == NULL);
  cache.Set(Key("foobar.com"), OK, AddressList(), now);
  EXPECT_TRUE(cache.Lookup(Key("foobar.com"), base::TimeTicks()) == NULL);

  EXPECT_EQ(0U, cache.size());
}

}  // namespace net
