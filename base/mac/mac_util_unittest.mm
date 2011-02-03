// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/mac/mac_util.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {
namespace mac {

namespace {

typedef PlatformTest MacUtilTest;

TEST_F(MacUtilTest, TestFSRef) {
  FSRef ref;
  std::string path("/System/Library");

  ASSERT_TRUE(FSRefFromPath(path, &ref));
  EXPECT_EQ(path, PathFromFSRef(ref));
}

TEST_F(MacUtilTest, GetUserDirectoryTest) {
  // Try a few keys, make sure they come back with non-empty paths.
  FilePath caches_dir;
  EXPECT_TRUE(GetUserDirectory(NSCachesDirectory, &caches_dir));
  EXPECT_FALSE(caches_dir.empty());

  FilePath application_support_dir;
  EXPECT_TRUE(GetUserDirectory(NSApplicationSupportDirectory,
                               &application_support_dir));
  EXPECT_FALSE(application_support_dir.empty());

  FilePath library_dir;
  EXPECT_TRUE(GetUserDirectory(NSLibraryDirectory, &library_dir));
  EXPECT_FALSE(library_dir.empty());
}

TEST_F(MacUtilTest, TestLibraryPath) {
  FilePath library_dir = GetUserLibraryPath();
  // Make sure the string isn't empty.
  EXPECT_FALSE(library_dir.value().empty());
}

TEST_F(MacUtilTest, TestGrabWindowSnapshot) {
  // Launch a test window so we can take a snapshot.
  NSRect frame = NSMakeRect(0, 0, 400, 400);
  scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:frame
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [window setBackgroundColor:[NSColor whiteColor]];
  [window makeKeyAndOrderFront:NSApp];

  scoped_ptr<std::vector<unsigned char> > png_representation(
      new std::vector<unsigned char>);
  int width, height;
  GrabWindowSnapshot(window, png_representation.get(),
                     &width, &height);

  // Copy png back into NSData object so we can make sure we grabbed a png.
  scoped_nsobject<NSData> image_data(
      [[NSData alloc] initWithBytes:&(*png_representation)[0]
                             length:png_representation->size()]);
  NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:image_data.get()];
  EXPECT_TRUE([rep isKindOfClass:[NSBitmapImageRep class]]);
  EXPECT_TRUE(CGImageGetWidth([rep CGImage]) == 400);
  NSColor* color = [rep colorAtX:200 y:200];
  CGFloat red = 0, green = 0, blue = 0, alpha = 0;
  [color getRed:&red green:&green blue:&blue alpha:&alpha];
  EXPECT_GE(red + green + blue, 3.0);
}

TEST_F(MacUtilTest, TestGetAppBundlePath) {
  FilePath out;

  // Make sure it doesn't crash.
  out = GetAppBundlePath(FilePath());
  EXPECT_TRUE(out.empty());

  // Some more invalid inputs.
  const char* invalid_inputs[] = {
    "/", "/foo", "foo", "/foo/bar.", "foo/bar.", "/foo/bar./bazquux",
    "foo/bar./bazquux", "foo/.app", "//foo",
  };
  for (size_t i = 0; i < arraysize(invalid_inputs); i++) {
    out = GetAppBundlePath(FilePath(invalid_inputs[i]));
    EXPECT_TRUE(out.empty()) << "loop: " << i;
  }

  // Some valid inputs; this and |expected_outputs| should be in sync.
  struct {
    const char *in;
    const char *expected_out;
  } valid_inputs[] = {
    { "FooBar.app/", "FooBar.app" },
    { "/FooBar.app", "/FooBar.app" },
    { "/FooBar.app/", "/FooBar.app" },
    { "//FooBar.app", "//FooBar.app" },
    { "/Foo/Bar.app", "/Foo/Bar.app" },
    { "/Foo/Bar.app/", "/Foo/Bar.app" },
    { "/F/B.app", "/F/B.app" },
    { "/F/B.app/", "/F/B.app" },
    { "/Foo/Bar.app/baz", "/Foo/Bar.app" },
    { "/Foo/Bar.app/baz/", "/Foo/Bar.app" },
    { "/Foo/Bar.app/baz/quux.app/quuux", "/Foo/Bar.app" },
    { "/Applications/Google Foo.app/bar/Foo Helper.app/quux/Foo Helper",
        "/Applications/Google Foo.app" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(valid_inputs); i++) {
    out = GetAppBundlePath(FilePath(valid_inputs[i].in));
    EXPECT_FALSE(out.empty()) << "loop: " << i;
    EXPECT_STREQ(valid_inputs[i].expected_out,
        out.value().c_str()) << "loop: " << i;
  }
}

TEST_F(MacUtilTest, TestExcludeFileFromBackups) {
  NSString* homeDirectory = NSHomeDirectory();
  NSString* dummyFilePath =
      [homeDirectory stringByAppendingPathComponent:@"DummyFile"];
  const char* dummy_file_path = [dummyFilePath fileSystemRepresentation];
  ASSERT_TRUE(dummy_file_path);
  FilePath file_path(dummy_file_path);
  // It is not actually necessary to have a physical file in order to
  // set its exclusion property.
  NSURL* fileURL = [NSURL URLWithString:dummyFilePath];
  // Reset the exclusion in case it was set previously.
  SetFileBackupExclusion(file_path, false);
  Boolean excludeByPath;
  // Initial state should be non-excluded.
  EXPECT_FALSE(CSBackupIsItemExcluded((CFURLRef)fileURL, &excludeByPath));
  // Exclude the file.
  EXPECT_TRUE(SetFileBackupExclusion(file_path, true));
  EXPECT_TRUE(CSBackupIsItemExcluded((CFURLRef)fileURL, &excludeByPath));
  // Un-exclude the file.
  EXPECT_TRUE(SetFileBackupExclusion(file_path, false));
  EXPECT_FALSE(CSBackupIsItemExcluded((CFURLRef)fileURL, &excludeByPath));
}

TEST_F(MacUtilTest, TestGetValueFromDictionary) {
  ScopedCFTypeRef<CFMutableDictionaryRef> dict(
      CFDictionaryCreateMutable(0, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(dict.get(), CFSTR("key"), CFSTR("value"));

  EXPECT_TRUE(CFEqual(CFSTR("value"),
                      GetValueFromDictionary(
                          dict, CFSTR("key"), CFStringGetTypeID())));
  EXPECT_FALSE(GetValueFromDictionary(dict, CFSTR("key"), CFNumberGetTypeID()));
  EXPECT_FALSE(GetValueFromDictionary(
                   dict, CFSTR("no-exist"), CFStringGetTypeID()));
}

TEST_F(MacUtilTest, CopyNSImageToCGImage) {
  scoped_nsobject<NSImage> nsImage(
      [[NSImage alloc] initWithSize:NSMakeSize(20, 20)]);
  [nsImage lockFocus];
  [[NSColor redColor] set];
  NSRect rect = NSZeroRect;
  rect.size = [nsImage size];
  NSRectFill(rect);
  [nsImage unlockFocus];

  ScopedCFTypeRef<CGImageRef> cgImage(CopyNSImageToCGImage(nsImage.get()));
  EXPECT_TRUE(cgImage.get());
}

TEST_F(MacUtilTest, NSObjectRetainRelease) {
  scoped_nsobject<NSArray> array([[NSArray alloc] initWithObjects:@"foo", nil]);
  EXPECT_EQ(1U, [array retainCount]);

  NSObjectRetain(array);
  EXPECT_EQ(2U, [array retainCount]);

  NSObjectRelease(array);
  EXPECT_EQ(1U, [array retainCount]);
}

}  // namespace

}  // namespace mac
}  // namespace base
