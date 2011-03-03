// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_MAC_UTIL_H_
#define BASE_MAC_MAC_UTIL_H_
#pragma once

#include <Carbon/Carbon.h>
#include <string>

#include "base/logging.h"

// TODO(rohitrao): Clean up sites that include mac_util.h and remove this line.
#include "base/mac/foundation_util.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#else  // __OBJC__
class NSImage;
#endif  // __OBJC__

class FilePath;

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
void ActivateProcess(pid_t pid);

// Set the Time Machine exclusion property for the given file.
bool SetFileBackupExclusion(const FilePath& file_path, bool exclude);

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

}  // namespace mac
}  // namespace base

#if !defined(__OBJC__)
#define OBJC_CPP_CLASS_DECL(x) class x;
#else  // __OBJC__
#define OBJC_CPP_CLASS_DECL(x)
#endif  // __OBJC__

// Convert toll-free bridged CFTypes to NSTypes and vice-versa. This does not
// autorelease |cf_val|. This is useful for the case where there is a CFType in
// a call that expects an NSType and the compiler is complaining about const
// casting problems.
// The calls are used like this:
// NSString *foo = CFToNSCast(CFSTR("Hello"));
// CFStringRef foo2 = NSToCFCast(@"Hello");
// The macro magic below is to enforce safe casting. It could possibly have
// been done using template function specialization, but template function
// specialization doesn't always work intuitively,
// (http://www.gotw.ca/publications/mill17.htm) so the trusty combination
// of macros and function overloading is used instead.

#define CF_TO_NS_CAST_DECL(TypeCF, TypeNS) \
OBJC_CPP_CLASS_DECL(TypeNS) \
\
namespace base { \
namespace mac { \
TypeNS* CFToNSCast(TypeCF##Ref cf_val); \
TypeCF##Ref NSToCFCast(TypeNS* ns_val); \
} \
} \

#define CF_TO_NS_MUTABLE_CAST_DECL(name) \
CF_TO_NS_CAST_DECL(CF##name, NS##name) \
OBJC_CPP_CLASS_DECL(NSMutable##name) \
\
namespace base { \
namespace mac { \
NSMutable##name* CFToNSCast(CFMutable##name##Ref cf_val); \
CFMutable##name##Ref NSToCFCast(NSMutable##name* ns_val); \
} \
} \

// List of toll-free bridged types taken from:
// http://www.cocoadev.com/index.pl?TollFreeBridged

CF_TO_NS_MUTABLE_CAST_DECL(Array);
CF_TO_NS_MUTABLE_CAST_DECL(AttributedString);
CF_TO_NS_CAST_DECL(CFCalendar, NSCalendar);
CF_TO_NS_MUTABLE_CAST_DECL(CharacterSet);
CF_TO_NS_MUTABLE_CAST_DECL(Data);
CF_TO_NS_CAST_DECL(CFDate, NSDate);
CF_TO_NS_MUTABLE_CAST_DECL(Dictionary);
CF_TO_NS_CAST_DECL(CFError, NSError);
CF_TO_NS_CAST_DECL(CFLocale, NSLocale);
CF_TO_NS_CAST_DECL(CFNumber, NSNumber);
CF_TO_NS_CAST_DECL(CFRunLoopTimer, NSTimer);
CF_TO_NS_CAST_DECL(CFTimeZone, NSTimeZone);
CF_TO_NS_MUTABLE_CAST_DECL(Set);
CF_TO_NS_CAST_DECL(CFReadStream, NSInputStream);
CF_TO_NS_CAST_DECL(CFWriteStream, NSOutputStream);
CF_TO_NS_MUTABLE_CAST_DECL(String);
CF_TO_NS_CAST_DECL(CFURL, NSURL);

// Stream operations for CFTypes. They can be used with NSTypes as well
// by using the NSToCFCast methods above.
// e.g. LOG(INFO) << base::mac::NSToCFCast(@"foo");
// Operator << can not be overloaded for ObjectiveC types as the compiler
// can not distinguish between overloads for id with overloads for void*.
extern std::ostream& operator<<(std::ostream& o, const CFErrorRef err);
extern std::ostream& operator<<(std::ostream& o, const CFStringRef str);

#endif  // BASE_MAC_MAC_UTIL_H_
