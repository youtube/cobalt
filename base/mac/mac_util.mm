// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"

#import <Cocoa/Cocoa.h>
#include <CoreServices/CoreServices.h>
#import <IOKit/IOKitLib.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/xattr.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_aedesc.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace base::mac {

namespace {

class LoginItemsFileList {
 public:
  LoginItemsFileList() = default;
  LoginItemsFileList(const LoginItemsFileList&) = delete;
  LoginItemsFileList& operator=(const LoginItemsFileList&) = delete;
  ~LoginItemsFileList() = default;

  [[nodiscard]] bool Initialize() {
    DCHECK(!login_items_.get()) << __func__ << " called more than once.";
    // The LSSharedFileList suite of functions has been deprecated. Instead,
    // a LoginItems helper should be registered with SMLoginItemSetEnabled()
    // https://crbug.com/1154377.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    login_items_.reset(LSSharedFileListCreate(
        nullptr, kLSSharedFileListSessionLoginItems, nullptr));
#pragma clang diagnostic pop
    DLOG_IF(ERROR, !login_items_.get()) << "Couldn't get a Login Items list.";
    return login_items_.get();
  }

  LSSharedFileListRef GetLoginFileList() {
    DCHECK(login_items_.get()) << "Initialize() failed or not called.";
    return login_items_;
  }

  // Looks into Shared File Lists corresponding to Login Items for the item
  // representing the specified bundle.  If such an item is found, returns a
  // retained reference to it. Caller is responsible for releasing the
  // reference.
  ScopedCFTypeRef<LSSharedFileListItemRef> GetLoginItemForApp(NSURL* url) {
    DCHECK(login_items_.get()) << "Initialize() failed or not called.";

#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    base::scoped_nsobject<NSArray> login_items_array(
        CFToNSCast(LSSharedFileListCopySnapshot(login_items_, nullptr)));
#pragma clang diagnostic pop

    for (id login_item in login_items_array.get()) {
      LSSharedFileListItemRef item =
          reinterpret_cast<LSSharedFileListItemRef>(login_item);
#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      // kLSSharedFileListDoNotMountVolumes is used so that we don't trigger
      // mounting when it's not expected by a user. Just listing the login
      // items should not cause any side-effects.
      ScopedCFTypeRef<CFURLRef> item_url(LSSharedFileListItemCopyResolvedURL(
          item, kLSSharedFileListDoNotMountVolumes, nullptr));
#pragma clang diagnostic pop

      if (item_url && CFEqual(item_url, url)) {
        return ScopedCFTypeRef<LSSharedFileListItemRef>(
            item, base::scoped_policy::RETAIN);
      }
    }

    return ScopedCFTypeRef<LSSharedFileListItemRef>();
  }

  ScopedCFTypeRef<LSSharedFileListItemRef> GetLoginItemForMainApp() {
    NSURL* url = [NSURL fileURLWithPath:[base::mac::MainBundle() bundlePath]];
    return GetLoginItemForApp(url);
  }

 private:
  ScopedCFTypeRef<LSSharedFileListRef> login_items_;
};

bool IsHiddenLoginItem(LSSharedFileListItemRef item) {
#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  ScopedCFTypeRef<CFBooleanRef> hidden(reinterpret_cast<CFBooleanRef>(
      LSSharedFileListItemCopyProperty(item,
          reinterpret_cast<CFStringRef>(kLSSharedFileListLoginItemHidden))));
#pragma clang diagnostic pop

  return hidden && hidden == kCFBooleanTrue;
}

}  // namespace

CGColorSpaceRef GetGenericRGBColorSpace() {
  // Leaked. That's OK, it's scoped to the lifetime of the application.
  static CGColorSpaceRef g_color_space_generic_rgb(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  DLOG_IF(ERROR, !g_color_space_generic_rgb) <<
      "Couldn't get the generic RGB color space";
  return g_color_space_generic_rgb;
}

CGColorSpaceRef GetSRGBColorSpace() {
  // Leaked.  That's OK, it's scoped to the lifetime of the application.
  static CGColorSpaceRef g_color_space_sRGB =
      CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  DLOG_IF(ERROR, !g_color_space_sRGB) << "Couldn't get the sRGB color space";
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
      DLOG(WARNING) <<
          "Couldn't get the main display's color space, using generic";
    } else {
      DLOG(ERROR) << "Couldn't get any color space";
    }
  }

  return g_system_color_space;
}

void AddToLoginItems(const FilePath& app_bundle_file_path,
                     bool hide_on_startup) {
  LoginItemsFileList login_items;
  if (!login_items.Initialize())
    return;

  NSURL* app_bundle_url = base::mac::FilePathToNSURL(app_bundle_file_path);
  base::ScopedCFTypeRef<LSSharedFileListItemRef> item(
      login_items.GetLoginItemForApp(app_bundle_url));

  if (item.get() && (IsHiddenLoginItem(item) == hide_on_startup)) {
    return;  // Already is a login item with required hide flag.
  }

  // Remove the old item, it has wrong hide flag, we'll create a new one.
  if (item.get()) {
#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    LSSharedFileListItemRemove(login_items.GetLoginFileList(), item);
#pragma clang diagnostic pop
  }

#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  BOOL hide = hide_on_startup ? YES : NO;
  NSDictionary* properties =
      @{(NSString*)kLSSharedFileListLoginItemHidden : @(hide) };

  ScopedCFTypeRef<LSSharedFileListItemRef> new_item(
      LSSharedFileListInsertItemURL(
          login_items.GetLoginFileList(), kLSSharedFileListItemLast, nullptr,
          nullptr, reinterpret_cast<CFURLRef>(app_bundle_url),
          reinterpret_cast<CFDictionaryRef>(properties), nullptr));
#pragma clang diagnostic pop

  if (!new_item.get()) {
    DLOG(ERROR) << "Couldn't insert current app into Login Items list.";
  }
}

void RemoveFromLoginItems(const FilePath& app_bundle_file_path) {
  LoginItemsFileList login_items;
  if (!login_items.Initialize())
    return;

  NSURL* app_bundle_url = base::mac::FilePathToNSURL(app_bundle_file_path);
  base::ScopedCFTypeRef<LSSharedFileListItemRef> item(
      login_items.GetLoginItemForApp(app_bundle_url));
  if (!item.get())
    return;

#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  LSSharedFileListItemRemove(login_items.GetLoginFileList(), item);
#pragma clang diagnostic pop
}

bool WasLaunchedAsLoginOrResumeItem() {
  ProcessSerialNumber psn = { 0, kCurrentProcess };
  ProcessInfoRec info = {};
  info.processInfoLength = sizeof(info);

// GetProcessInformation has been deprecated since macOS 10.9, but there is no
// replacement that provides the information we need. See
// https://crbug.com/650854.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if (GetProcessInformation(&psn, &info) == noErr) {
#pragma clang diagnostic pop
    ProcessInfoRec parent_info = {};
    parent_info.processInfoLength = sizeof(parent_info);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (GetProcessInformation(&info.processLauncher, &parent_info) == noErr) {
#pragma clang diagnostic pop
      return parent_info.processSignature == 'lgnw';
    }
  }
  return false;
}

bool WasLaunchedAsLoginItemRestoreState() {
  // "Reopen windows..." option was added for Lion.  Prior OS versions should
  // not have this behavior.
  if (!WasLaunchedAsLoginOrResumeItem())
    return false;

  CFStringRef app = CFSTR("com.apple.loginwindow");
  CFStringRef save_state = CFSTR("TALLogoutSavesState");
  ScopedCFTypeRef<CFPropertyListRef> plist(
      CFPreferencesCopyAppValue(save_state, app));
  // According to documentation, com.apple.loginwindow.plist does not exist on a
  // fresh installation until the user changes a login window setting.  The
  // "reopen windows" option is checked by default, so the plist would exist had
  // the user unchecked it.
  // https://developer.apple.com/library/mac/documentation/macosx/conceptual/bpsystemstartup/chapters/CustomLogin.html
  if (!plist)
    return true;

  if (CFBooleanRef restore_state = base::mac::CFCast<CFBooleanRef>(plist))
    return CFBooleanGetValue(restore_state);

  return false;
}

bool WasLaunchedAsHiddenLoginItem() {
  if (!WasLaunchedAsLoginOrResumeItem())
    return false;

  LoginItemsFileList login_items;
  if (!login_items.Initialize())
    return false;

  base::ScopedCFTypeRef<LSSharedFileListItemRef> item(
      login_items.GetLoginItemForMainApp());
  if (!item.get()) {
    // OS X can launch items for the resume feature.
    return false;
  }
  return IsHiddenLoginItem(item);
}

bool RemoveQuarantineAttribute(const FilePath& file_path) {
  const char kQuarantineAttrName[] = "com.apple.quarantine";
  int status = removexattr(file_path.value().c_str(), kQuarantineAttrName, 0);
  return status == 0 || errno == ENOATTR;
}

namespace {

// Returns the running system's Darwin major version. Don't call this, it's an
// implementation detail and its result is meant to be cached by
// MacOSVersionInternal().
int DarwinMajorVersionInternal() {
  // base::OperatingSystemVersionNumbers() at one time called Gestalt(), which
  // was observed to be able to spawn threads (see https://crbug.com/53200).
  // Nowadays that function calls -[NSProcessInfo operatingSystemVersion], whose
  // current implementation does things like hit the file system, which is
  // possibly a blocking operation. Either way, it's overkill for what needs to
  // be done here.
  //
  // uname, on the other hand, is implemented as a simple series of sysctl
  // system calls to obtain the relevant data from the kernel. The data is
  // compiled right into the kernel, so no threads or blocking or other
  // funny business is necessary.

  struct utsname uname_info;
  if (uname(&uname_info) != 0) {
    DPLOG(ERROR) << "uname";
    return 0;
  }

  if (strcmp(uname_info.sysname, "Darwin") != 0) {
    DLOG(ERROR) << "unexpected uname sysname " << uname_info.sysname;
    return 0;
  }

  int darwin_major_version = 0;
  char* dot = strchr(uname_info.release, '.');
  if (dot) {
    if (!base::StringToInt(
            base::StringPiece(uname_info.release,
                              static_cast<size_t>(dot - uname_info.release)),
            &darwin_major_version)) {
      dot = NULL;
    }
  }

  if (!dot) {
    DLOG(ERROR) << "could not parse uname release " << uname_info.release;
    return 0;
  }

  return darwin_major_version;
}

// The implementation of MacOSVersion() as defined in the header. Don't call
// this, it's an implementation detail and the result is meant to be cached by
// MacOSVersion().
int MacOSVersionInternal() {
  int darwin_major_version = DarwinMajorVersionInternal();

  // Darwin major versions 6 through 19 corresponded to macOS versions 10.2
  // through 10.15.
  CHECK(darwin_major_version >= 6);
  if (darwin_major_version <= 19)
    return 1000 + darwin_major_version - 4;

  // Darwin major version 20 corresponds to macOS version 11.0. Assume a
  // correspondence between Darwin's major version numbers and macOS major
  // version numbers.
  int macos_major_version = darwin_major_version - 9;

  return macos_major_version * 100;
}

}  // namespace

namespace internal {

int MacOSVersion() {
  static int macos_version = MacOSVersionInternal();
  return macos_version;
}

}  // namespace internal

namespace {

#if defined(ARCH_CPU_X86_64)
// https://developer.apple.com/documentation/apple_silicon/about_the_rosetta_translation_environment#3616845
bool ProcessIsTranslated() {
  int ret = 0;
  size_t size = sizeof(ret);
  if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) == -1)
    return false;
  return ret;
}
#endif  // ARCH_CPU_X86_64

}  // namespace

CPUType GetCPUType() {
#if defined(ARCH_CPU_ARM64)
  return CPUType::kArm;
#elif defined(ARCH_CPU_X86_64)
  return ProcessIsTranslated() ? CPUType::kTranslatedIntel : CPUType::kIntel;
#else
#error Time for another chip transition?
#endif  // ARCH_CPU_*
}

std::string GetModelIdentifier() {
  std::string return_string;
  ScopedIOObject<io_service_t> platform_expert(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (platform_expert) {
    ScopedCFTypeRef<CFDataRef> model_data(
        static_cast<CFDataRef>(IORegistryEntryCreateCFProperty(
            platform_expert,
            CFSTR("model"),
            kCFAllocatorDefault,
            0)));
    if (model_data) {
      return_string =
          reinterpret_cast<const char*>(CFDataGetBytePtr(model_data));
    }
  }
  return return_string;
}

bool ParseModelIdentifier(const std::string& ident,
                          std::string* type,
                          int32_t* major,
                          int32_t* minor) {
  size_t number_loc = ident.find_first_of("0123456789");
  if (number_loc == std::string::npos)
    return false;
  size_t comma_loc = ident.find(',', number_loc);
  if (comma_loc == std::string::npos)
    return false;
  int32_t major_tmp, minor_tmp;
  std::string::const_iterator begin = ident.begin();
  if (!StringToInt(MakeStringPiece(begin + static_cast<ptrdiff_t>(number_loc),
                                   begin + static_cast<ptrdiff_t>(comma_loc)),
                   &major_tmp) ||
      !StringToInt(
          MakeStringPiece(begin + static_cast<ptrdiff_t>(comma_loc) + 1,
                          ident.end()),
          &minor_tmp))
    return false;
  *type = ident.substr(0, number_loc);
  *major = major_tmp;
  *minor = minor_tmp;
  return true;
}

std::string GetOSDisplayName() {
  std::string version_string = base::SysNSStringToUTF8(
      [[NSProcessInfo processInfo] operatingSystemVersionString]);
  return "macOS " + version_string;
}

std::string GetPlatformSerialNumber() {
  base::mac::ScopedIOObject<io_service_t> expert_device(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (!expert_device) {
    DLOG(ERROR) << "Error retrieving the machine serial number.";
    return std::string();
  }

  base::ScopedCFTypeRef<CFTypeRef> serial_number(
      IORegistryEntryCreateCFProperty(expert_device,
                                      CFSTR(kIOPlatformSerialNumberKey),
                                      kCFAllocatorDefault, 0));
  CFStringRef serial_number_cfstring =
      base::mac::CFCast<CFStringRef>(serial_number);
  if (!serial_number_cfstring) {
    DLOG(ERROR) << "Error retrieving the machine serial number.";
    return std::string();
  }

  return base::SysCFStringRefToUTF8(serial_number_cfstring);
}

void OpenSystemSettingsPane(SystemSettingsPane pane) {
  NSString* url = nil;
  NSString* pane_file = nil;
  NSData* subpane_data = nil;
  // Note: On macOS 13 and later, System Settings are implemented with app
  // extensions found at /System/Library/ExtensionKit/Extensions/. URLs to open
  // them are constructed with a scheme of "x-apple.systempreferences" and a
  // body of the the bundle ID of the app extension. (In the Info.plist there is
  // an EXAppExtensionAttributes dictionary with legacy identifiers, but given
  // that those are explicitly named "legacy", this code prefers to use the
  // bundle IDs for the URLs it uses.) It is not yet known how to definitively
  // identify the query string used to open sub-panes; the ones used below were
  // determined from historical usage, disassembly of related code, and
  // guessing. Clarity was requested from Apple in FB11753405.
  switch (pane) {
    case SystemSettingsPane::kAccessibility_Captions:
      if (IsAtLeastOS13()) {
        url = @"x-apple.systempreferences:com.apple.Accessibility-Settings."
              @"extension?Captioning";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.universalaccess?"
              @"Captioning";
      }
      break;
    case SystemSettingsPane::kDateTime:
      if (IsAtLeastOS13()) {
        url =
            @"x-apple.systempreferences:com.apple.Date-Time-Settings.extension";
      } else {
        pane_file = @"/System/Library/PreferencePanes/DateAndTime.prefPane";
      }
      break;
    case SystemSettingsPane::kNetwork_Proxies:
      if (IsAtLeastOS13()) {
        url = @"x-apple.systempreferences:com.apple.Network-Settings.extension?"
              @"Proxies";
      } else {
        pane_file = @"/System/Library/PreferencePanes/Network.prefPane";
        subpane_data = [@"Proxies" dataUsingEncoding:NSASCIIStringEncoding];
      }
      break;
    case SystemSettingsPane::kPrintersScanners:
      if (IsAtLeastOS13()) {
        url = @"x-apple.systempreferences:com.apple.Print-Scan-Settings."
              @"extension";
      } else {
        pane_file = @"/System/Library/PreferencePanes/PrintAndFax.prefPane";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Accessibility:
      if (IsAtLeastOS13()) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_Accessibility";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_Accessibility";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Bluetooth:
      if (IsAtLeastOS13()) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_Bluetooth";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_Bluetooth";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Camera:
      if (IsAtLeastOS13()) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_Camera";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_Camera";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Extensions_Sharing:
      if (IsAtLeastOS13()) {
        // See ShareKit, -[SHKSharingServicePicker openAppExtensionsPrefpane].
        url = @"x-apple.systempreferences:com.apple.ExtensionPreferences?"
              @"Sharing";
      } else {
        // This is equivalent to the implementation of AppKit's
        // +[NSSharingServicePicker openAppExtensionsPrefPane].
        pane_file = @"/System/Library/PreferencePanes/Extensions.prefPane";
        NSDictionary* subpane_dict = @{
          @"action" : @"revealExtensionPoint",
          @"protocol" : @"com.apple.share-services"
        };
        subpane_data = [NSPropertyListSerialization
            dataWithPropertyList:subpane_dict
                          format:NSPropertyListXMLFormat_v1_0
                         options:0
                           error:nil];
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_LocationServices:
      if (IsAtLeastOS13()) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_LocationServices";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_LocationServices";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Microphone:
      if (IsAtLeastOS13()) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_Microphone";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_Microphone";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_ScreenRecording:
      if (IsAtLeastOS13()) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_ScreenCapture";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_ScreenCapture";
      }
      break;
  }

  DCHECK(url != nil ^ pane_file != nil);

  if (url) {
    [NSWorkspace.sharedWorkspace openURL:[NSURL URLWithString:url]];
    return;
  }

  base::scoped_nsobject<NSAppleEventDescriptor> subpane_descriptor;
  NSArray* pane_file_urls = @[ [NSURL fileURLWithPath:pane_file] ];

  LSLaunchURLSpec launchSpec = {0};
  launchSpec.itemURLs = NSToCFCast(pane_file_urls);
  if (subpane_data) {
    subpane_descriptor.reset([[NSAppleEventDescriptor alloc]
        initWithDescriptorType:'ptru'
                          data:subpane_data]);
    launchSpec.passThruParams = subpane_descriptor.get().aeDesc;
  }
  launchSpec.launchFlags = kLSLaunchAsync | kLSLaunchDontAddToRecents;

  LSOpenFromURLSpec(&launchSpec, nullptr);
}

}  // namespace base::mac
