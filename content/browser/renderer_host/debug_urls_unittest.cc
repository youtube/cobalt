// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/debug_urls.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "url/gurl.h"

namespace content {

using DebugUrlsUnitTest = testing::Test;

TEST_F(DebugUrlsUnitTest, IsDebugURL_NonDebugUrlsReturnFalse) {
  EXPECT_FALSE(IsDebugURL(GURL("invalid")));
  EXPECT_FALSE(IsDebugURL(GURL("https://example.com")));
  EXPECT_FALSE(IsDebugURL(GURL("http://example.com")));
  EXPECT_FALSE(IsDebugURL(GURL("chrome://version")));
  EXPECT_FALSE(IsDebugURL(GURL(blink::kChromeUIRendererHeapUaFURL)));
  EXPECT_FALSE(IsDebugURL(GURL("chrome://crash/renderer/heap-overflow")));
  EXPECT_FALSE(IsDebugURL(GURL("chrome://crash/renderer/heap-underflow")));
  EXPECT_FALSE(IsDebugURL(
      GURL("chrome://crash/renderer/member-dereference-after-free")));
}

TEST_F(DebugUrlsUnitTest, IsDebugURL_AsanUrlsReturnTrue) {
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(IsDebugURL(GURL("chrome://crash/browser-corrupt-heap-block")));
  EXPECT_TRUE(IsDebugURL(GURL("chrome://crash/browser-corrupt-heap")));
#endif
}

TEST_F(DebugUrlsUnitTest, IsDebugURL_DebugUrlsReturnTrue) {
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIBrowserCrashURL)));
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIBrowserDcheckURL)));
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIBrowserHeapUaFURL)));
  EXPECT_TRUE(IsDebugURL(GURL("chrome://crash/browser/heap-overflow")));
  EXPECT_TRUE(IsDebugURL(GURL("chrome://crash/browser/heap-underflow")));
  EXPECT_TRUE(
      IsDebugURL(GURL("chrome://crash/browser/member-dereference-after-free")));
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIBrowserHeapCorruptionURL)));
#endif
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIBrowserUIHang)));
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIDelayedBrowserUIHang)));
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIGpuCleanURL)));
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIGpuCrashURL)));
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIGpuHeapUaFURL)));
  EXPECT_TRUE(IsDebugURL(GURL("chrome://crash/gpu/heap-overflow")));
  EXPECT_TRUE(IsDebugURL(GURL("chrome://crash/gpu/heap-underflow")));
  EXPECT_TRUE(
      IsDebugURL(GURL("chrome://crash/gpu/member-dereference-after-free")));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIGpuJavaCrashURL)));
#endif
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIGpuHangURL)));
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIMemoryPressureCriticalURL)));
  EXPECT_TRUE(IsDebugURL(GURL(blink::kChromeUIMemoryPressureModerateURL)));
}

}  // namespace content
