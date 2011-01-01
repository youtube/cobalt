// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_MAC_UTIL_H_
#define BASE_MAC_MAC_UTIL_H_
#pragma once

#include <Carbon/Carbon.h>
#include <string>
#include <vector>

#include "base/logging.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>

@class NSBundle;
@class NSWindow;
#else  // __OBJC__
class NSBundle;
class NSImage;
class NSWindow;
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

// Full screen modes, in increasing order of priority.  More permissive modes
// take predecence.
enum FullScreenMode {
  kFullScreenModeHideAll = 0,
  kFullScreenModeHideDock = 1,
  kFullScreenModeAutoHideAll = 2,
  kNumFullScreenModes = 3,

  // kFullScreenModeNormal is not a valid FullScreenMode, but it is useful to
  // other classes, so we include it here.
  kFullScreenModeNormal = 10,
};

std::string PathFromFSRef(const FSRef& ref);
bool FSRefFromPath(const std::string& path, FSRef* ref);

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

// Searches for directories for the given key in only the user domain.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
bool GetUserDirectory(NSSearchPathDirectory directory, FilePath* result);

// Searches for directories for the given key in only the local domain.
// If found, fills result (which must always be non-NULL) with the
// first found directory and returns true.  Otherwise, returns false.
bool GetLocalDirectory(NSSearchPathDirectory directory, FilePath* result);

// Returns the ~/Library directory.
FilePath GetUserLibraryPath();

// Returns an sRGB color space.  The return value is a static value; do not
// release it!
CGColorSpaceRef GetSRGBColorSpace();

// Returns the color space being used by the main display.  The return value
// is a static value; do not release it!
CGColorSpaceRef GetSystemColorSpace();

// Add a full screen request for the given |mode|.  Must be paired with a
// ReleaseFullScreen() call for the same |mode|.  This does not by itself create
// a fullscreen window; rather, it manages per-application state related to
// hiding the dock and menubar.  Must be called on the main thread.
void RequestFullScreen(FullScreenMode mode);

// Release a request for full screen mode.  Must be matched with a
// RequestFullScreen() call for the same |mode|.  As with RequestFullScreen(),
// this does not affect windows directly, but rather manages per-application
// state.  For example, if there are no other outstanding
// |kFullScreenModeAutoHideAll| requests, this will reshow the menu bar.  Must
// be called on main thread.
void ReleaseFullScreen(FullScreenMode mode);

// Convenience method to switch the current fullscreen mode.  This has the same
// net effect as a ReleaseFullScreen(from_mode) call followed immediately by a
// RequestFullScreen(to_mode).  Must be called on the main thread.
void SwitchFullScreenModes(FullScreenMode from_mode, FullScreenMode to_mode);

// Set the visibility of the cursor.
void SetCursorVisibility(bool visible);

// Should windows miniaturize on a double-click (on the title bar)?
bool ShouldWindowsMiniaturizeOnDoubleClick();

// Activates the process with the given PID.
void ActivateProcess(pid_t);

// Pulls a snapshot of the entire browser into png_representation.
void GrabWindowSnapshot(NSWindow* window,
                        std::vector<unsigned char>* png_representation,
                        int* width, int* height);

// Takes a path to an (executable) binary and tries to provide the path to an
// application bundle containing it. It takes the outermost bundle that it can
// find (so for "/Foo/Bar.app/.../Baz.app/..." it produces "/Foo/Bar.app").
//   |exec_name| - path to the binary
//   returns - path to the application bundle, or empty on error
FilePath GetAppBundlePath(const FilePath& exec_name);

// Set the Time Machine exclusion property for the given file.
bool SetFileBackupExclusion(const FilePath& file_path, bool exclude);

// Utility function to pull out a value from a dictionary, check its type, and
// return it.  Returns NULL if the key is not present or of the wrong type.
CFTypeRef GetValueFromDictionary(CFDictionaryRef dict,
                                 CFStringRef key,
                                 CFTypeID expected_type);

// Sets the process name as displayed in Activity Monitor to process_name.
void SetProcessName(CFStringRef process_name);

// Converts a NSImage to a CGImageRef.  Normally, the system frameworks can do
// this fine, especially on 10.6.  On 10.5, however, CGImage cannot handle
// converting a PDF-backed NSImage into a CGImageRef.  This function will
// rasterize the PDF into a bitmap CGImage.  The caller is responsible for
// releasing the return value.
CGImageRef CopyNSImageToCGImage(NSImage* image);

// Checks if the current application is set as a Login Item, so it will launch
// on Login. If a non-NULL pointer to is_hidden is passed, the Login Item also
// is queried for the 'hide on launch' flag.
bool CheckLoginItemStatus(bool* is_hidden);

// Adds current application to the set of Login Items with specified "hide"
// flag. This has the same effect as adding/removing the application in
// SystemPreferences->Accounts->LoginItems or marking Application in the Dock
// as "Options->Open on Login".
// Does nothing if the application is already set up as Login Item with
// specified hide flag.
void AddToLoginItems(bool hide_on_startup);

// Removes the current application from the list Of Login Items.
void RemoveFromLoginItems();

// Returns true if the current process was automatically launched as a
// 'Login Item' with 'hide on startup' flag. Used to suppress opening windows.
bool WasLaunchedAsHiddenLoginItem();

// Retain/release calls for memory management in C++.
void NSObjectRetain(void* obj);
void NSObjectRelease(void* obj);

#if defined(__OBJC__)

// Convert toll-free bridged CFTypes to NSTypes. This does not autorelease
// |cf_val|. This is useful for the case where there is a CFType in a call that
// expects an NSType and the compiler is complaining about const casting
// problems.
// The call is used like this:
// NSString *foo = CFToNSCast(CFSTR("Hello"));
// The macro magic below is to enforce safe casting. It could possibly have
// been done using template function specialization, but template function
// specialization doesn't always work intuitively,
// (http://www.gotw.ca/publications/mill17.htm) so the trusty combination
// of macros and function overloading is used instead.

#define CF_TO_NS_CAST(TypeCF, TypeNS) \
inline TypeNS* CFToNSCast(TypeCF cf_val) { \
  TypeNS* ns_val = \
      const_cast<TypeNS*>(reinterpret_cast<const TypeNS*>(cf_val)); \
  DCHECK(!ns_val || [ns_val isKindOfClass:[TypeNS class]]); \
  return ns_val; \
}

// List of toll-free bridged types taken from:
// http://www.cocoadev.com/index.pl?TollFreeBridged

CF_TO_NS_CAST(CFArrayRef, NSArray);
CF_TO_NS_CAST(CFMutableArrayRef, NSMutableArray);
CF_TO_NS_CAST(CFAttributedStringRef, NSAttributedString);
CF_TO_NS_CAST(CFMutableAttributedStringRef, NSMutableAttributedString);
CF_TO_NS_CAST(CFCalendarRef, NSCalendar);
CF_TO_NS_CAST(CFCharacterSetRef, NSCharacterSet);
CF_TO_NS_CAST(CFMutableCharacterSetRef, NSMutableCharacterSet);
CF_TO_NS_CAST(CFDataRef, NSData);
CF_TO_NS_CAST(CFMutableDataRef, NSMutableData);
CF_TO_NS_CAST(CFDateRef, NSDate);
CF_TO_NS_CAST(CFDictionaryRef, NSDictionary);
CF_TO_NS_CAST(CFMutableDictionaryRef, NSMutableDictionary);
CF_TO_NS_CAST(CFNumberRef, NSNumber);
CF_TO_NS_CAST(CFRunLoopTimerRef, NSTimer);
CF_TO_NS_CAST(CFSetRef, NSSet);
CF_TO_NS_CAST(CFMutableSetRef, NSMutableSet);
CF_TO_NS_CAST(CFStringRef, NSString);
CF_TO_NS_CAST(CFMutableStringRef, NSMutableString);
CF_TO_NS_CAST(CFURLRef, NSURL);
CF_TO_NS_CAST(CFTimeZoneRef, NSTimeZone);
CF_TO_NS_CAST(CFReadStreamRef, NSInputStream);
CF_TO_NS_CAST(CFWriteStreamRef, NSOutputStream);

#endif  // __OBJC__

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_MAC_UTIL_H_
