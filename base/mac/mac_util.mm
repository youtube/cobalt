// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"

namespace base {
namespace mac {

namespace {

// The current count of outstanding requests for full screen mode from browser
// windows, plugins, etc.
int g_full_screen_requests[kNumFullScreenModes] = { 0, 0, 0};

// Sets the appropriate SystemUIMode based on the current full screen requests.
// Since only one SystemUIMode can be active at a given time, full screen
// requests are ordered by priority.  If there are no outstanding full screen
// requests, reverts to normal mode.  If the correct SystemUIMode is already
// set, does nothing.
void SetUIMode() {
  // Get the current UI mode.
  SystemUIMode current_mode;
  GetSystemUIMode(&current_mode, NULL);

  // Determine which mode should be active, based on which requests are
  // currently outstanding.  More permissive requests take precedence.  For
  // example, plugins request |kFullScreenModeAutoHideAll|, while browser
  // windows request |kFullScreenModeHideDock| when the fullscreen overlay is
  // down.  Precedence goes to plugins in this case, so AutoHideAll wins over
  // HideDock.
  SystemUIMode desired_mode = kUIModeNormal;
  SystemUIOptions desired_options = 0;
  if (g_full_screen_requests[kFullScreenModeAutoHideAll] > 0) {
    desired_mode = kUIModeAllHidden;
    desired_options = kUIOptionAutoShowMenuBar;
  } else if (g_full_screen_requests[kFullScreenModeHideDock] > 0) {
    desired_mode = kUIModeContentHidden;
  } else if (g_full_screen_requests[kFullScreenModeHideAll] > 0) {
    desired_mode = kUIModeAllHidden;
  }

  if (current_mode != desired_mode)
    SetSystemUIMode(desired_mode, desired_options);
}

bool WasLaunchedAsLoginItem() {
  ProcessSerialNumber psn = { 0, kCurrentProcess };

  scoped_nsobject<NSDictionary> process_info(
      CFToNSCast(ProcessInformationCopyDictionary(&psn,
                     kProcessDictionaryIncludeAllInformationMask)));

  long long temp = [[process_info objectForKey:@"ParentPSN"] longLongValue];
  ProcessSerialNumber parent_psn =
      { (temp >> 32) & 0x00000000FFFFFFFFLL, temp & 0x00000000FFFFFFFFLL };

  scoped_nsobject<NSDictionary> parent_info(
      CFToNSCast(ProcessInformationCopyDictionary(&parent_psn,
                     kProcessDictionaryIncludeAllInformationMask)));

  // Check that creator process code is that of loginwindow.
  BOOL result =
      [[parent_info objectForKey:@"FileCreator"] isEqualToString:@"lgnw"];

  return result == YES;
}

// Looks into Shared File Lists corresponding to Login Items for the item
// representing the current application. If such an item is found, returns a
// retained reference to it. Caller is responsible for releasing the reference.
LSSharedFileListItemRef GetLoginItemForApp() {
  ScopedCFTypeRef<LSSharedFileListRef> login_items(LSSharedFileListCreate(
      NULL, kLSSharedFileListSessionLoginItems, NULL));

  if (!login_items.get()) {
    LOG(ERROR) << "Couldn't get a Login Items list.";
    return NULL;
  }

  scoped_nsobject<NSArray> login_items_array(
      CFToNSCast(LSSharedFileListCopySnapshot(login_items, NULL)));

  NSURL* url = [NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];

  for(NSUInteger i = 0; i < [login_items_array count]; ++i) {
    LSSharedFileListItemRef item = reinterpret_cast<LSSharedFileListItemRef>(
        [login_items_array objectAtIndex:i]);
    CFURLRef item_url_ref = NULL;

    if (LSSharedFileListItemResolve(item, 0, &item_url_ref, NULL) == noErr) {
      ScopedCFTypeRef<CFURLRef> item_url(item_url_ref);
      if (CFEqual(item_url, url)) {
        CFRetain(item);
        return item;
      }
    }
  }

  return NULL;
}

#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
// kLSSharedFileListLoginItemHidden is supported on
// 10.5, but missing from the 10.5 headers.
// http://openradar.appspot.com/6482251
static NSString* kLSSharedFileListLoginItemHidden =
    @"com.apple.loginitem.HideOnLaunch";
#endif

bool IsHiddenLoginItem(LSSharedFileListItemRef item) {
  ScopedCFTypeRef<CFBooleanRef> hidden(reinterpret_cast<CFBooleanRef>(
      LSSharedFileListItemCopyProperty(item,
          reinterpret_cast<CFStringRef>(kLSSharedFileListLoginItemHidden))));

  return hidden && hidden == kCFBooleanTrue;
}

}  // namespace

std::string PathFromFSRef(const FSRef& ref) {
  ScopedCFTypeRef<CFURLRef> url(
      CFURLCreateFromFSRef(kCFAllocatorDefault, &ref));
  NSString *path_string = [(NSURL *)url.get() path];
  return [path_string fileSystemRepresentation];
}

bool FSRefFromPath(const std::string& path, FSRef* ref) {
  OSStatus status = FSPathMakeRef((const UInt8*)path.c_str(),
                                  ref, nil);
  return status == noErr;
}

CGColorSpaceRef GetSRGBColorSpace() {
  // Leaked.  That's OK, it's scoped to the lifetime of the application.
  static CGColorSpaceRef g_color_space_sRGB =
      CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  LOG_IF(ERROR, !g_color_space_sRGB) << "Couldn't get the sRGB color space";
  return g_color_space_sRGB;
}

CGColorSpaceRef GetSystemColorSpace() {
  // Leaked.  That's OK, it's scoped to the lifetime of the application.
  // Try to get the main display's color space.
  static CGColorSpaceRef g_system_color_space =
      CGDisplayCopyColorSpace(CGMainDisplayID());

  if (!g_system_color_space) {
    // Use a generic RGB color space.  This is better than nothing.
    g_system_color_space = CGColorSpaceCreateDeviceRGB();

    if (g_system_color_space) {
      LOG(WARNING) <<
          "Couldn't get the main display's color space, using generic";
    } else {
      LOG(ERROR) << "Couldn't get any color space";
    }
  }

  return g_system_color_space;
}

// Add a request for full screen mode.  Must be called on the main thread.
void RequestFullScreen(FullScreenMode mode) {
  DCHECK_LT(mode, kNumFullScreenModes);
  if (mode >= kNumFullScreenModes)
    return;

  DCHECK_GE(g_full_screen_requests[mode], 0);
  g_full_screen_requests[mode] = std::max(g_full_screen_requests[mode] + 1, 1);
  SetUIMode();
}

// Release a request for full screen mode.  Must be called on the main thread.
void ReleaseFullScreen(FullScreenMode mode) {
  DCHECK_LT(mode, kNumFullScreenModes);
  if (mode >= kNumFullScreenModes)
    return;

  DCHECK_GT(g_full_screen_requests[mode], 0);
  g_full_screen_requests[mode] = std::max(g_full_screen_requests[mode] - 1, 0);
  SetUIMode();
}

// Switches full screen modes.  Releases a request for |from_mode| and adds a
// new request for |to_mode|.  Must be called on the main thread.
void SwitchFullScreenModes(FullScreenMode from_mode, FullScreenMode to_mode) {
  DCHECK_LT(from_mode, kNumFullScreenModes);
  DCHECK_LT(to_mode, kNumFullScreenModes);
  if (from_mode >= kNumFullScreenModes || to_mode >= kNumFullScreenModes)
    return;

  DCHECK_GT(g_full_screen_requests[from_mode], 0);
  DCHECK_GE(g_full_screen_requests[to_mode], 0);
  g_full_screen_requests[from_mode] =
      std::max(g_full_screen_requests[from_mode] - 1, 0);
  g_full_screen_requests[to_mode] =
      std::max(g_full_screen_requests[to_mode] + 1, 1);
  SetUIMode();
}

void SetCursorVisibility(bool visible) {
  if (visible)
    [NSCursor unhide];
  else
    [NSCursor hide];
}

bool ShouldWindowsMiniaturizeOnDoubleClick() {
  // We use an undocumented method in Cocoa; if it doesn't exist, default to
  // |true|. If it ever goes away, we can do (using an undocumented pref key):
  //   NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  //   return ![defaults objectForKey:@"AppleMiniaturizeOnDoubleClick"] ||
  //          [defaults boolForKey:@"AppleMiniaturizeOnDoubleClick"];
  BOOL methodImplemented =
      [NSWindow respondsToSelector:@selector(_shouldMiniaturizeOnDoubleClick)];
  DCHECK(methodImplemented);
  return !methodImplemented ||
      [NSWindow performSelector:@selector(_shouldMiniaturizeOnDoubleClick)];
}

void ActivateProcess(pid_t pid) {
  ProcessSerialNumber process;
  OSStatus status = GetProcessForPID(pid, &process);
  if (status == noErr) {
    SetFrontProcess(&process);
  } else {
    LOG(WARNING) << "Unable to get process for pid " << pid;
  }
}

bool SetFileBackupExclusion(const FilePath& file_path, bool exclude) {
  NSString* filePath =
      [NSString stringWithUTF8String:file_path.value().c_str()];

  // If being asked to exclude something in a tmp directory, just lie and say it
  // was done.  TimeMachine will already ignore tmp directories.  This keeps the
  // temporary profiles used by unittests from being added to the exclude list.
  // Otherwise, as /Library/Preferences/com.apple.TimeMachine.plist grows the
  // bots slow down due to reading/writing all the temporary profiles used over
  // time.

  NSString* tmpDir = NSTemporaryDirectory();
  // Make sure the temp dir is terminated with a slash
  if (tmpDir && ![tmpDir hasSuffix:@"/"])
    tmpDir = [tmpDir stringByAppendingString:@"/"];
  // '/var' is a link to '/private/var', make sure to check both forms.
  NSString* privateTmpDir = nil;
  if ([tmpDir hasPrefix:@"/var/"])
    privateTmpDir = [@"/private" stringByAppendingString:tmpDir];

  if ((tmpDir && [filePath hasPrefix:tmpDir]) ||
      (privateTmpDir && [filePath hasPrefix:privateTmpDir]) ||
      [filePath hasPrefix:@"/tmp/"] ||
      [filePath hasPrefix:@"/var/tmp/"] ||
      [filePath hasPrefix:@"/private/tmp/"] ||
      [filePath hasPrefix:@"/private/var/tmp/"]) {
    return true;
  }

  NSURL* url = [NSURL fileURLWithPath:filePath];
  // Note that we always set CSBackupSetItemExcluded's excludeByPath param
  // to true.  This prevents a problem with toggling the setting: if the file
  // is excluded with excludeByPath set to true then excludeByPath must
  // also be true when un-excluding the file, otherwise the un-excluding
  // will be ignored.
  bool success =
      CSBackupSetItemExcluded((CFURLRef)url, exclude, true) == noErr;
  if (!success)
    LOG(WARNING) << "Failed to set backup exclusion for file '"
                 << file_path.value().c_str() << "'.  Continuing.";
  return success;
}

void SetProcessName(CFStringRef process_name) {
  if (!process_name || CFStringGetLength(process_name) == 0) {
    NOTREACHED() << "SetProcessName given bad name.";
    return;
  }

  if (![NSThread isMainThread]) {
    NOTREACHED() << "Should only set process name from main thread.";
    return;
  }

  // Warning: here be dragons! This is SPI reverse-engineered from WebKit's
  // plugin host, and could break at any time (although realistically it's only
  // likely to break in a new major release).
  // When 10.7 is available, check that this still works, and update this
  // comment for 10.8.

  // Private CFType used in these LaunchServices calls.
  typedef CFTypeRef PrivateLSASN;
  typedef PrivateLSASN (*LSGetCurrentApplicationASNType)();
  typedef OSStatus (*LSSetApplicationInformationItemType)(int, PrivateLSASN,
                                                          CFStringRef,
                                                          CFStringRef,
                                                          CFDictionaryRef*);

  static LSGetCurrentApplicationASNType ls_get_current_application_asn_func =
      NULL;
  static LSSetApplicationInformationItemType
      ls_set_application_information_item_func = NULL;
  static CFStringRef ls_display_name_key = NULL;

  static bool did_symbol_lookup = false;
  if (!did_symbol_lookup) {
    did_symbol_lookup = true;
    CFBundleRef launch_services_bundle =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.LaunchServices"));
    if (!launch_services_bundle) {
      LOG(ERROR) << "Failed to look up LaunchServices bundle";
      return;
    }

    ls_get_current_application_asn_func =
        reinterpret_cast<LSGetCurrentApplicationASNType>(
            CFBundleGetFunctionPointerForName(
                launch_services_bundle, CFSTR("_LSGetCurrentApplicationASN")));
    if (!ls_get_current_application_asn_func)
      LOG(ERROR) << "Could not find _LSGetCurrentApplicationASN";

    ls_set_application_information_item_func =
        reinterpret_cast<LSSetApplicationInformationItemType>(
            CFBundleGetFunctionPointerForName(
                launch_services_bundle,
                CFSTR("_LSSetApplicationInformationItem")));
    if (!ls_set_application_information_item_func)
      LOG(ERROR) << "Could not find _LSSetApplicationInformationItem";

    CFStringRef* key_pointer = reinterpret_cast<CFStringRef*>(
        CFBundleGetDataPointerForName(launch_services_bundle,
                                      CFSTR("_kLSDisplayNameKey")));
    ls_display_name_key = key_pointer ? *key_pointer : NULL;
    if (!ls_display_name_key)
      LOG(ERROR) << "Could not find _kLSDisplayNameKey";

    // Internally, this call relies on the Mach ports that are started up by the
    // Carbon Process Manager.  In debug builds this usually happens due to how
    // the logging layers are started up; but in release, it isn't started in as
    // much of a defined order.  So if the symbols had to be loaded, go ahead
    // and force a call to make sure the manager has been initialized and hence
    // the ports are opened.
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
  }
  if (!ls_get_current_application_asn_func ||
      !ls_set_application_information_item_func ||
      !ls_display_name_key) {
    return;
  }

  PrivateLSASN asn = ls_get_current_application_asn_func();
  // Constant used by WebKit; what exactly it means is unknown.
  const int magic_session_constant = -2;
  OSErr err =
      ls_set_application_information_item_func(magic_session_constant, asn,
                                               ls_display_name_key,
                                               process_name,
                                               NULL /* optional out param */);
  LOG_IF(ERROR, err) << "Call to set process name failed, err " << err;
}

// Converts a NSImage to a CGImageRef.  Normally, the system frameworks can do
// this fine, especially on 10.6.  On 10.5, however, CGImage cannot handle
// converting a PDF-backed NSImage into a CGImageRef.  This function will
// rasterize the PDF into a bitmap CGImage.  The caller is responsible for
// releasing the return value.
CGImageRef CopyNSImageToCGImage(NSImage* image) {
  // This is based loosely on http://www.cocoadev.com/index.pl?CGImageRef .
  NSSize size = [image size];
  ScopedCFTypeRef<CGContextRef> context(
      CGBitmapContextCreate(NULL,  // Allow CG to allocate memory.
                            size.width,
                            size.height,
                            8,  // bitsPerComponent
                            0,  // bytesPerRow - CG will calculate by default.
                            [[NSColorSpace genericRGBColorSpace] CGColorSpace],
                            kCGBitmapByteOrder32Host |
                                kCGImageAlphaPremultipliedFirst));
  if (!context.get())
    return NULL;

  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:
      [NSGraphicsContext graphicsContextWithGraphicsPort:context.get()
                                                 flipped:NO]];
  [image drawInRect:NSMakeRect(0,0, size.width, size.height)
           fromRect:NSZeroRect
          operation:NSCompositeCopy
           fraction:1.0];
  [NSGraphicsContext restoreGraphicsState];

  return CGBitmapContextCreateImage(context);
}

bool CheckLoginItemStatus(bool* is_hidden) {
  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (!item.get())
    return false;

  if (is_hidden)
    *is_hidden = IsHiddenLoginItem(item);

  return true;
}

void AddToLoginItems(bool hide_on_startup) {
  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (item.get() && (IsHiddenLoginItem(item) == hide_on_startup)) {
    return;  // Already is a login item with required hide flag.
  }

  ScopedCFTypeRef<LSSharedFileListRef> login_items(LSSharedFileListCreate(
      NULL, kLSSharedFileListSessionLoginItems, NULL));

  if (!login_items.get()) {
    LOG(ERROR) << "Couldn't get a Login Items list.";
    return;
  }

  // Remove the old item, it has wrong hide flag, we'll create a new one.
  if (item.get()) {
    LSSharedFileListItemRemove(login_items, item);
  }

  NSURL* url = [NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];

  BOOL hide = hide_on_startup ? YES : NO;
  NSDictionary* properties =
      [NSDictionary
        dictionaryWithObject:[NSNumber numberWithBool:hide]
                      forKey:(NSString*)kLSSharedFileListLoginItemHidden];

  ScopedCFTypeRef<LSSharedFileListItemRef> new_item;
  new_item.reset(LSSharedFileListInsertItemURL(
      login_items, kLSSharedFileListItemLast, NULL, NULL,
      reinterpret_cast<CFURLRef>(url),
      reinterpret_cast<CFDictionaryRef>(properties), NULL));

  if (!new_item.get()) {
    LOG(ERROR) << "Couldn't insert current app into Login Items list.";
  }
}

void RemoveFromLoginItems() {
  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (!item.get())
    return;

  ScopedCFTypeRef<LSSharedFileListRef> login_items(LSSharedFileListCreate(
      NULL, kLSSharedFileListSessionLoginItems, NULL));

  if (!login_items.get()) {
    LOG(ERROR) << "Couldn't get a Login Items list.";
    return;
  }

  LSSharedFileListItemRemove(login_items, item);
}

bool WasLaunchedAsHiddenLoginItem() {
  if (!WasLaunchedAsLoginItem())
    return false;

  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (!item.get()) {
    LOG(ERROR) << "Process launched at Login but can't access Login Item List.";
    return false;
  }
  return IsHiddenLoginItem(item);
}

// Definitions for the corresponding CF_TO_NS_CAST_DECL macros in mac_util.h.
#define CF_TO_NS_CAST_DEFN(TypeCF, TypeNS) \
\
TypeNS* CFToNSCast(TypeCF##Ref cf_val) { \
  DCHECK(!cf_val || TypeCF##GetTypeID() == CFGetTypeID(cf_val)); \
  TypeNS* ns_val = \
      const_cast<TypeNS*>(reinterpret_cast<const TypeNS*>(cf_val)); \
  return ns_val; \
} \
\
TypeCF##Ref NSToCFCast(TypeNS* ns_val) { \
  TypeCF##Ref cf_val = reinterpret_cast<TypeCF##Ref>(ns_val); \
  DCHECK(!cf_val || TypeCF##GetTypeID() == CFGetTypeID(cf_val)); \
  return cf_val; \
} \

#define CF_TO_NS_MUTABLE_CAST_DEFN(name) \
CF_TO_NS_CAST_DEFN(CF##name, NS##name) \
\
NSMutable##name* CFToNSCast(CFMutable##name##Ref cf_val) { \
  DCHECK(!cf_val || CF##name##GetTypeID() == CFGetTypeID(cf_val)); \
  NSMutable##name* ns_val = reinterpret_cast<NSMutable##name*>(cf_val); \
  return ns_val; \
} \
\
CFMutable##name##Ref NSToCFCast(NSMutable##name* ns_val) { \
  CFMutable##name##Ref cf_val = \
      reinterpret_cast<CFMutable##name##Ref>(ns_val); \
  DCHECK(!cf_val || CF##name##GetTypeID() == CFGetTypeID(cf_val)); \
  return cf_val; \
} \

CF_TO_NS_MUTABLE_CAST_DEFN(Array);
CF_TO_NS_MUTABLE_CAST_DEFN(AttributedString);
CF_TO_NS_CAST_DEFN(CFCalendar, NSCalendar);
CF_TO_NS_MUTABLE_CAST_DEFN(CharacterSet);
CF_TO_NS_MUTABLE_CAST_DEFN(Data);
CF_TO_NS_CAST_DEFN(CFDate, NSDate);
CF_TO_NS_MUTABLE_CAST_DEFN(Dictionary);
CF_TO_NS_CAST_DEFN(CFError, NSError);
CF_TO_NS_CAST_DEFN(CFLocale, NSLocale);
CF_TO_NS_CAST_DEFN(CFNumber, NSNumber);
CF_TO_NS_CAST_DEFN(CFRunLoopTimer, NSTimer);
CF_TO_NS_CAST_DEFN(CFTimeZone, NSTimeZone);
CF_TO_NS_MUTABLE_CAST_DEFN(Set);
CF_TO_NS_CAST_DEFN(CFReadStream, NSInputStream);
CF_TO_NS_CAST_DEFN(CFWriteStream, NSOutputStream);
CF_TO_NS_MUTABLE_CAST_DEFN(String);
CF_TO_NS_CAST_DEFN(CFURL, NSURL);

}  // namespace mac
}  // namespace base

std::ostream& operator<<(std::ostream& o, const CFStringRef string) {
  return o << base::SysCFStringRefToUTF8(string);
}

std::ostream& operator<<(std::ostream& o, const CFErrorRef err) {
  base::mac::ScopedCFTypeRef<CFStringRef> desc(CFErrorCopyDescription(err));
  base::mac::ScopedCFTypeRef<CFDictionaryRef> user_info(
      CFErrorCopyUserInfo(err));
  CFStringRef errorDesc = NULL;
  if (user_info.get()) {
    errorDesc = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(user_info.get(), kCFErrorDescriptionKey));
  }
  o << "Code: " << CFErrorGetCode(err)
    << " Domain: " << CFErrorGetDomain(err)
    << " Desc: " << desc.get();
  if(errorDesc) {
    o << "(" << errorDesc << ")";
  }
  return o;
}
