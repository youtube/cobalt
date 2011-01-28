// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_FOUNDATION_UTIL_H_
#define BASE_MAC_FOUNDATION_UTIL_H_
#pragma once

#include <string>
#include <vector>

#include "base/logging.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
@class NSBundle;
#else  // __OBJC__
class NSBundle;
#endif  // __OBJC__

class FilePath;

// Adapted from NSPathUtilities.h and NSObjCRuntime.h.
#if __LP64__ || NS_BUILD_32_LIKE_64
typedef unsigned long NSSearchPathDirectory;
typedef unsigned long NSSearchPathDomainMask;
#else
typedef unsigned int NSSearchPathDirectory;
typedef unsigned int NSSearchPathDomainMask;
#endif

namespace base {
namespace mac {

// Returns true if the application is running from a bundle
bool AmIBundled();
void SetOverrideAmIBundled(bool value);

// Returns true if this process is marked as a "Background only process".
bool IsBackgroundOnlyProcess();

// Returns the main bundle or the override, used for code that needs
// to fetch resources from bundles, but work within a unittest where we
// aren't a bundle.
NSBundle* MainAppBundle();
FilePath MainAppBundlePath();

// Set the bundle that MainAppBundle will return, overriding the default value
// (Restore the default by calling SetOverrideAppBundle(nil)).
void SetOverrideAppBundle(NSBundle* bundle);
void SetOverrideAppBundlePath(const FilePath& file_path);

// Returns the creator code associated with the CFBundleRef at bundle.
OSType CreatorCodeForCFBundleRef(CFBundleRef bundle);

// Returns the creator code associated with this application, by calling
// CreatorCodeForCFBundleRef for the application's main bundle.  If this
// information cannot be determined, returns kUnknownType ('????').  This
// does not respect the override app bundle because it's based on CFBundle
// instead of NSBundle, and because callers probably don't want the override
// app bundle's creator code anyway.
OSType CreatorCodeForApplication();

// Searches for directories for the given key in only the given |domain_mask|.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
bool GetSearchPathDirectory(NSSearchPathDirectory directory,
                            NSSearchPathDomainMask domain_mask,
                            FilePath* result);

// Searches for directories for the given key in only the local domain.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
bool GetLocalDirectory(NSSearchPathDirectory directory, FilePath* result);

// Searches for directories for the given key in only the user domain.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
bool GetUserDirectory(NSSearchPathDirectory directory, FilePath* result);

// Returns the ~/Library directory.
FilePath GetUserLibraryPath();

// Takes a path to an (executable) binary and tries to provide the path to an
// application bundle containing it. It takes the outermost bundle that it can
// find (so for "/Foo/Bar.app/.../Baz.app/..." it produces "/Foo/Bar.app").
//   |exec_name| - path to the binary
//   returns - path to the application bundle, or empty on error
FilePath GetAppBundlePath(const FilePath& exec_name);

// Utility function to pull out a value from a dictionary, check its type, and
// return it.  Returns NULL if the key is not present or of the wrong type.
CFTypeRef GetValueFromDictionary(CFDictionaryRef dict,
                                 CFStringRef key,
                                 CFTypeID expected_type);

// Retain/release calls for memory management in C++.
void NSObjectRetain(void* obj);
void NSObjectRelease(void* obj);

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_FOUNDATION_UTIL_H_
