// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/vlog.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace logging {

namespace {

class VlogTest : public testing::Test {
};

TEST_F(VlogTest, NoVmodule) {
  EXPECT_EQ(0, VlogInfo("", "").GetVlogLevel("test1"));
  EXPECT_EQ(0, VlogInfo("0", "").GetVlogLevel("test2"));
  EXPECT_EQ(0, VlogInfo("blah", "").GetVlogLevel("test3"));
  EXPECT_EQ(0, VlogInfo("0blah1", "").GetVlogLevel("test4"));
  EXPECT_EQ(1, VlogInfo("1", "").GetVlogLevel("test5"));
  EXPECT_EQ(5, VlogInfo("5", "").GetVlogLevel("test6"));
}

TEST_F(VlogTest, VmoduleBasic) {
  const char kVSwitch[] = "-1";
  const char kVModuleSwitch[] =
      "foo=,bar=0,baz=blah,,qux=0blah1,quux=1,corge.ext=5";
  VlogInfo vlog_info(kVSwitch, kVModuleSwitch);
  EXPECT_EQ(-1, vlog_info.GetVlogLevel("/path/to/grault.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/path/to/foo.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("D:\\Path\\To\\bar-inl.mm"));
  EXPECT_EQ(-1, vlog_info.GetVlogLevel("D:\\path\\to what/bar_unittest.m"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("baz.h"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/another/path/to/qux.h"));
  EXPECT_EQ(1, vlog_info.GetVlogLevel("/path/to/quux"));
  EXPECT_EQ(5, vlog_info.GetVlogLevel("c:\\path/to/corge.ext.h"));
}

TEST_F(VlogTest, VmoduleDirs) {
  const char kVModuleSwitch[] =
      "foo/bar.cc=1,baz\\*\\qux.cc=2,*quux/*=3,*/*-inl.h=4";
  VlogInfo vlog_info("", kVModuleSwitch);
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/foo/bar.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("bar.cc"));
  EXPECT_EQ(1, vlog_info.GetVlogLevel("foo/bar.cc"));

  EXPECT_EQ(0, vlog_info.GetVlogLevel("baz/grault/qux.h"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/baz/grault/qux.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("baz/grault/qux.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("baz/grault/blah/qux.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("baz\\grault\\qux.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("baz\\grault\\blah\\qux.cc"));

  EXPECT_EQ(0, vlog_info.GetVlogLevel("/foo/bar/baz/quux.cc"));
  EXPECT_EQ(3, vlog_info.GetVlogLevel("/foo/bar/baz/quux/grault.cc"));

  EXPECT_EQ(0, vlog_info.GetVlogLevel("foo/bar/test-inl.cc"));
  EXPECT_EQ(4, vlog_info.GetVlogLevel("foo/bar/test-inl.h"));
  EXPECT_EQ(4, vlog_info.GetVlogLevel("foo/bar/baz/blah-inl.h"));
}

}  // namespace

}  // namespace logging
