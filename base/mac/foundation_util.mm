// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"

namespace base {
namespace mac {

static bool g_override_am_i_bundled = false;
static bool g_override_am_i_bundled_value = false;

// Adapted from http://developer.apple.com/carbon/tipsandtricks.html#AmIBundled
static bool UncachedAmIBundled() {
  if (g_override_am_i_bundled)
    return g_override_am_i_bundled_value;

  ProcessSerialNumber psn = {0, kCurrentProcess};

  FSRef fsref;
  OSStatus pbErr;
  if ((pbErr = GetProcessBundleLocation(&psn, &fsref)) != noErr) {
    LOG(ERROR) << "GetProcessBundleLocation failed: error " << pbErr;
    return false;
  }

  FSCatalogInfo info;
  OSErr fsErr;
  if ((fsErr = FSGetCatalogInfo(&fsref, kFSCatInfoNodeFlags, &info,
                                NULL, NULL, NULL)) != noErr) {
    LOG(ERROR) << "FSGetCatalogInfo failed: error " << fsErr;
    return false;
  }

  return info.nodeFlags & kFSNodeIsDirectoryMask;
}

bool AmIBundled() {
  // If the return value is not cached, this function will return different
  // values depending on when it's called. This confuses some client code, see
  // http://crbug.com/63183 .
  static bool result = UncachedAmIBundled();
  DCHECK_EQ(result, UncachedAmIBundled())
      << "The return value of AmIBundled() changed. This will confuse tests. "
      << "Call SetAmIBundled() override manually if your test binary "
      << "delay-loads the framework.";
  return result;
}

void SetOverrideAmIBundled(bool value) {
  g_override_am_i_bundled = true;
  g_override_am_i_bundled_value = value;
}

bool IsBackgroundOnlyProcess() {
  // This function really does want to examine NSBundle's idea of the main
  // bundle dictionary, and not the overriden MainAppBundle.  It needs to look
  // at the actual running .app's Info.plist to access its LSUIElement
  // property.
  NSDictionary* info_dictionary = [[NSBundle mainBundle] infoDictionary];
  return [[info_dictionary objectForKey:@"LSUIElement"] boolValue] != NO;
}

// No threading worries since NSBundle isn't thread safe.
static NSBundle* g_override_app_bundle = nil;

NSBundle* MainAppBundle() {
  if (g_override_app_bundle)
    return g_override_app_bundle;
  return [NSBundle mainBundle];
}

FilePath MainAppBundlePath() {
  NSBundle* bundle = MainAppBundle();
  return FilePath([[bundle bundlePath] fileSystemRepresentation]);
}

FilePath PathForMainAppBundleResource(CFStringRef resourceName) {
  NSBundle* bundle = MainAppBundle();
  NSString* resourcePath = [bundle pathForResource:(NSString*)resourceName
                                            ofType:nil];
  if (!resourcePath)
    return FilePath();
  return FilePath([resourcePath fileSystemRepresentation]);
}

void SetOverrideAppBundle(NSBundle* bundle) {
  if (bundle != g_override_app_bundle) {
    [g_override_app_bundle release];
    g_override_app_bundle = [bundle retain];
  }
}

void SetOverrideAppBundlePath(const FilePath& file_path) {
  NSString* path = base::SysUTF8ToNSString(file_path.value());
  NSBundle* bundle = [NSBundle bundleWithPath:path];
  CHECK(bundle) << "Failed to load the bundle at " << file_path.value();

  SetOverrideAppBundle(bundle);
}

OSType CreatorCodeForCFBundleRef(CFBundleRef bundle) {
  OSType creator = kUnknownType;
  CFBundleGetPackageInfo(bundle, NULL, &creator);
  return creator;
}

OSType CreatorCodeForApplication() {
  CFBundleRef bundle = CFBundleGetMainBundle();
  if (!bundle)
    return kUnknownType;

  return CreatorCodeForCFBundleRef(bundle);
}

bool GetSearchPathDirectory(NSSearchPathDirectory directory,
                            NSSearchPathDomainMask domain_mask,
                            FilePath* result) {
  DCHECK(result);
  NSArray* dirs =
      NSSearchPathForDirectoriesInDomains(directory, domain_mask, YES);
  if ([dirs count] < 1) {
    return false;
  }
  NSString* path = [dirs objectAtIndex:0];
  *result = FilePath([path fileSystemRepresentation]);
  return true;
}

bool GetLocalDirectory(NSSearchPathDirectory directory, FilePath* result) {
  return GetSearchPathDirectory(directory, NSLocalDomainMask, result);
}

bool GetUserDirectory(NSSearchPathDirectory directory, FilePath* result) {
  return GetSearchPathDirectory(directory, NSUserDomainMask, result);
}

FilePath GetUserLibraryPath() {
  FilePath user_library_path;
  if (!GetUserDirectory(NSLibraryDirectory, &user_library_path)) {
    LOG(WARNING) << "Could not get user library path";
  }
  return user_library_path;
}

// Takes a path to an (executable) binary and tries to provide the path to an
// application bundle containing it. It takes the outermost bundle that it can
// find (so for "/Foo/Bar.app/.../Baz.app/..." it produces "/Foo/Bar.app").
//   |exec_name| - path to the binary
//   returns - path to the application bundle, or empty on error
FilePath GetAppBundlePath(const FilePath& exec_name) {
  const char kExt[] = ".app";
  const size_t kExtLength = arraysize(kExt) - 1;

  // Split the path into components.
  std::vector<std::string> components;
  exec_name.GetComponents(&components);

  // It's an error if we don't get any components.
  if (!components.size())
    return FilePath();

  // Don't prepend '/' to the first component.
  std::vector<std::string>::const_iterator it = components.begin();
  std::string bundle_name = *it;
  DCHECK(it->length() > 0);
  // If the first component ends in ".app", we're already done.
  if (it->length() > kExtLength &&
      !it->compare(it->length() - kExtLength, kExtLength, kExt, kExtLength))
    return FilePath(bundle_name);

  // The first component may be "/" or "//", etc. Only append '/' if it doesn't
  // already end in '/'.
  if (bundle_name[bundle_name.length() - 1] != '/')
    bundle_name += '/';

  // Go through the remaining components.
  for (++it; it != components.end(); ++it) {
    DCHECK(it->length() > 0);

    bundle_name += *it;

    // If the current component ends in ".app", we're done.
    if (it->length() > kExtLength &&
        !it->compare(it->length() - kExtLength, kExtLength, kExt, kExtLength))
      return FilePath(bundle_name);

    // Separate this component from the next one.
    bundle_name += '/';
  }

  return FilePath();
}

CFTypeRef GetValueFromDictionary(CFDictionaryRef dict,
                                 CFStringRef key,
                                 CFTypeID expected_type) {
  CFTypeRef value = CFDictionaryGetValue(dict, key);
  if (!value)
    return value;

  if (CFGetTypeID(value) != expected_type) {
    ScopedCFTypeRef<CFStringRef> expected_type_ref(
        CFCopyTypeIDDescription(expected_type));
    ScopedCFTypeRef<CFStringRef> actual_type_ref(
        CFCopyTypeIDDescription(CFGetTypeID(value)));
    LOG(WARNING) << "Expected value for key "
                 << base::SysCFStringRefToUTF8(key)
                 << " to be "
                 << base::SysCFStringRefToUTF8(expected_type_ref)
                 << " but it was "
                 << base::SysCFStringRefToUTF8(actual_type_ref)
                 << " instead";
    return NULL;
  }

  return value;
}

void NSObjectRetain(void* obj) {
  id<NSObject> nsobj = static_cast<id<NSObject> >(obj);
  [nsobj retain];
}

void NSObjectRelease(void* obj) {
  id<NSObject> nsobj = static_cast<id<NSObject> >(obj);
  [nsobj release];
}

}  // namespace mac
}  // namespace base
