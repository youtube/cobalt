// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_auth_cache.h"

#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::FtpAuthCache;

TEST(FtpAuthCacheTest, LookupAddRemove) {
  FtpAuthCache cache;

  GURL origin1("ftp://foo1");
  GURL origin2("ftp://foo2");

  // Lookup non-existent entry.
  EXPECT_TRUE(cache.Lookup(origin1) == NULL);

  // Add entry for origin1.
  cache.Add(origin1, L"username1", L"password1");
  FtpAuthCache::Entry* entry1 = cache.Lookup(origin1);
  ASSERT_TRUE(entry1);
  EXPECT_EQ(origin1, entry1->origin);
  EXPECT_EQ(L"username1", entry1->username);
  EXPECT_EQ(L"password1", entry1->password);

  // Add an entry for origin2.
  cache.Add(origin2, L"username2", L"password2");
  FtpAuthCache::Entry* entry2 = cache.Lookup(origin2);
  ASSERT_TRUE(entry2);
  EXPECT_EQ(origin2, entry2->origin);
  EXPECT_EQ(L"username2", entry2->username);
  EXPECT_EQ(L"password2", entry2->password);

  // The original entry1 should still be there.
  EXPECT_EQ(entry1, cache.Lookup(origin1));

  // Overwrite the entry for origin1.
  cache.Add(origin1, L"username3", L"password3");
  FtpAuthCache::Entry* entry3 = cache.Lookup(origin1);
  ASSERT_TRUE(entry3);
  EXPECT_EQ(origin1, entry3->origin);
  EXPECT_EQ(L"username3", entry3->username);
  EXPECT_EQ(L"password3", entry3->password);

  // Remove entry of origin1.
  cache.Remove(origin1, L"username3", L"password3");
  EXPECT_TRUE(cache.Lookup(origin1) == NULL);

  // Remove non-existent entry.
  cache.Remove(origin1, L"username3", L"password3");
  EXPECT_TRUE(cache.Lookup(origin1) == NULL);
}

// Check that if the origin differs only by port number, it is considered
// a separate origin.
TEST(FtpAuthCacheTest, LookupWithPort) {
  FtpAuthCache cache;

  GURL origin1("ftp://foo:80");
  GURL origin2("ftp://foo:21");

  cache.Add(origin1, L"username", L"password");
  cache.Add(origin2, L"username", L"password");

  EXPECT_NE(cache.Lookup(origin1), cache.Lookup(origin2));
}

TEST(FtpAuthCacheTest, NormalizedKey) {
  // GURL is automatically canonicalized. Hence the following variations in
  // url format should all map to the same entry (case insensitive host,
  // default port of 21).

  FtpAuthCache cache;

  // Add.
  cache.Add(GURL("ftp://HoSt:21"), L"username", L"password");

  // Lookup.
  FtpAuthCache::Entry* entry1 = cache.Lookup(GURL("ftp://HoSt:21"));
  ASSERT_TRUE(entry1);
  EXPECT_EQ(entry1, cache.Lookup(GURL("ftp://host:21")));
  EXPECT_EQ(entry1, cache.Lookup(GURL("ftp://host")));

  // Overwrite.
  cache.Add(GURL("ftp://host"), L"othername", L"otherword");
  FtpAuthCache::Entry* entry2 = cache.Lookup(GURL("ftp://HoSt:21"));
  ASSERT_TRUE(entry2);
  EXPECT_EQ(GURL("ftp://host"), entry2->origin);
  EXPECT_EQ(L"othername", entry2->username);
  EXPECT_EQ(L"otherword", entry2->password);

  // Remove
  cache.Remove(GURL("ftp://HOsT"), L"othername", L"otherword");
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host")) == NULL);
}

TEST(FtpAuthCacheTest, OnlyRemoveMatching) {
  FtpAuthCache cache;

  cache.Add(GURL("ftp://host"), L"username", L"password");
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host")));

  // Auth data doesn't match, shouldn't remove.
  cache.Remove(GURL("ftp://host"), L"bogus", L"bogus");
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host")));

  // Auth data matches, should remove.
  cache.Remove(GURL("ftp://host"), L"username", L"password");
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host")) == NULL);
}

TEST(FtpAuthCacheTest, EvictOldEntries) {
  FtpAuthCache cache;

  for (size_t i = 0; i < FtpAuthCache::kMaxEntries; i++)
    cache.Add(GURL("ftp://host" + IntToString(i)), L"username", L"password");

  // No entries should be evicted before reaching the limit.
  for (size_t i = 0; i < FtpAuthCache::kMaxEntries; i++) {
    EXPECT_TRUE(cache.Lookup(GURL("ftp://host" + IntToString(i))));
  }

  // Adding one entry should cause eviction of the first entry.
  cache.Add(GURL("ftp://last_host"), L"username", L"password");
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host0")) == NULL);

  // Remaining entries should not get evicted.
  for (size_t i = 1; i < FtpAuthCache::kMaxEntries; i++) {
    EXPECT_TRUE(cache.Lookup(GURL("ftp://host" + IntToString(i))));
  }
  EXPECT_TRUE(cache.Lookup(GURL("ftp://last_host")));
}
