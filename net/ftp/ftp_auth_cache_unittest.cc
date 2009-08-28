// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_auth_cache.h"

#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::AuthData;
using net::FtpAuthCache;

TEST(FtpAuthCacheTest, LookupAddRemove) {
  FtpAuthCache cache;

  GURL origin1("ftp://foo1");
  scoped_refptr<AuthData> data1(new AuthData());

  GURL origin2("ftp://foo2");
  scoped_refptr<AuthData> data2(new AuthData());

  GURL origin3("ftp://foo3");
  scoped_refptr<AuthData> data3(new AuthData());

  // Lookup non-existent entry.
  EXPECT_TRUE(NULL == cache.Lookup(origin1));

  // Add entry for origin1.
  cache.Add(origin1, data1.get());
  EXPECT_EQ(data1.get(), cache.Lookup(origin1));

  // Add an entry for origin2.
  cache.Add(origin2, data2.get());
  EXPECT_EQ(data1.get(), cache.Lookup(origin1));
  EXPECT_EQ(data2.get(), cache.Lookup(origin2));

  // Overwrite the entry for origin1.
  cache.Add(origin1, data3.get());
  EXPECT_EQ(data3.get(), cache.Lookup(origin1));
  EXPECT_EQ(data2.get(), cache.Lookup(origin2));

  // Remove entry of origin1.
  cache.Remove(origin1);
  EXPECT_TRUE(NULL == cache.Lookup(origin1));
  EXPECT_EQ(data2.get(), cache.Lookup(origin2));

  // Remove non-existent entry
  cache.Remove(origin1);
  EXPECT_TRUE(NULL == cache.Lookup(origin1));
  EXPECT_EQ(data2.get(), cache.Lookup(origin2));
}

// Check that if the origin differs only by port number, it is considered
// a separate origin.
TEST(FtpAuthCacheTest, LookupWithPort) {
  FtpAuthCache cache;

  GURL origin1("ftp://foo:80");
  scoped_refptr<AuthData> data1(new AuthData());

  GURL origin2("ftp://foo:21");
  scoped_refptr<AuthData> data2(new AuthData());

  cache.Add(origin1, data1.get());
  cache.Add(origin2, data2.get());

  EXPECT_EQ(data1.get(), cache.Lookup(origin1));
  EXPECT_EQ(data2.get(), cache.Lookup(origin2));
}

TEST(FtpAuthCacheTest, NormalizedKey) {
  // GURL is automatically canonicalized. Hence the following variations in
  // url format should all map to the same entry (case insensitive host,
  // default port of 21).

  FtpAuthCache cache;

  scoped_refptr<AuthData> data1(new AuthData());
  scoped_refptr<AuthData> data2(new AuthData());

  // Add.
  cache.Add(GURL("ftp://HoSt:21"), data1.get());

  // Lookup.
  EXPECT_EQ(data1.get(), cache.Lookup(GURL("ftp://HoSt:21")));
  EXPECT_EQ(data1.get(), cache.Lookup(GURL("ftp://host:21")));
  EXPECT_EQ(data1.get(), cache.Lookup(GURL("ftp://host")));

  // Overwrite.
  cache.Add(GURL("ftp://host"), data2.get());
  EXPECT_EQ(data2.get(), cache.Lookup(GURL("ftp://HoSt:21")));

  // Remove
  cache.Remove(GURL("ftp://HOsT"));
  EXPECT_TRUE(NULL == cache.Lookup(GURL("ftp://host")));
}

TEST(FtpAuthCacheTest, EvictOldEntries) {
  FtpAuthCache cache;

  scoped_refptr<AuthData> auth_data(new AuthData());

  for (size_t i = 0; i < FtpAuthCache::kMaxEntries; i++)
    cache.Add(GURL("ftp://host" + IntToString(i)), auth_data.get());

  // No entries should be evicted before reaching the limit.
  for (size_t i = 0; i < FtpAuthCache::kMaxEntries; i++) {
    EXPECT_EQ(auth_data.get(),
              cache.Lookup(GURL("ftp://host" + IntToString(i))));
  }

  // Adding one entry should cause eviction of the first entry.
  cache.Add(GURL("ftp://last_host"), auth_data.get());
  EXPECT_TRUE(NULL == cache.Lookup(GURL("ftp://host0")));

  // Remaining entries should not get evicted.
  for (size_t i = 1; i < FtpAuthCache::kMaxEntries; i++) {
    EXPECT_EQ(auth_data.get(),
              cache.Lookup(GURL("ftp://host" + IntToString(i))));
  }
  EXPECT_EQ(auth_data.get(), cache.Lookup(GURL("ftp://last_host")));
}
