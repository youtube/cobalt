// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/platform_mime_util.h"

#include <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>

#include <string>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"

// SPI declaration; see the commentary in GetPlatformExtensionsForMimeType.

@interface NSURLFileTypeMappings : NSObject
+ (NSURLFileTypeMappings*)sharedMappings;
- (NSArray*)extensionsForMIMEType:(NSString*)mimeType;
@end

namespace net {

bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const FilePath::StringType& ext, std::string* result) const {
  std::string ext_nodot = ext;
  if (ext_nodot.length() >= 1 && ext_nodot[0] == L'.')
    ext_nodot.erase(ext_nodot.begin());
  base::mac::ScopedCFTypeRef<CFStringRef> ext_ref(
      base::SysUTF8ToCFStringRef(ext_nodot));
  if (!ext_ref)
    return false;
  base::mac::ScopedCFTypeRef<CFStringRef> uti(
      UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                            ext_ref,
                                            NULL));
  if (!uti)
    return false;
  base::mac::ScopedCFTypeRef<CFStringRef> mime_ref(
      UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType));
  if (!mime_ref)
    return false;

  *result = base::SysCFStringRefToUTF8(mime_ref);
  return true;
}

bool PlatformMimeUtil::GetPreferredExtensionForMimeType(
    const std::string& mime_type, FilePath::StringType* ext) const {
  base::mac::ScopedCFTypeRef<CFStringRef> mime_ref(
      base::SysUTF8ToCFStringRef(mime_type));
  if (!mime_ref)
    return false;
  base::mac::ScopedCFTypeRef<CFStringRef> uti(
      UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType,
                                            mime_ref,
                                            NULL));
  if (!uti)
    return false;
  base::mac::ScopedCFTypeRef<CFStringRef> ext_ref(
      UTTypeCopyPreferredTagWithClass(uti, kUTTagClassFilenameExtension));
  if (!ext_ref)
    return false;

  *ext = base::SysCFStringRefToUTF8(ext_ref);
  return true;
}

void PlatformMimeUtil::GetPlatformExtensionsForMimeType(
    const std::string& mime_type,
    base::hash_set<FilePath::StringType>* extensions) const {
  // There is no API for this that uses UTIs. The WebKitSystemInterface call
  // WKGetExtensionsForMIMEType() is a thin wrapper around
  // [[NSURLFileTypeMappings sharedMappings] extensionsForMIMEType:], which is
  // used by Firefox as well.
  //
  // See:
  // http://mxr.mozilla.org/mozilla-central/search?string=extensionsForMIMEType
  // http://www.openradar.me/11384153
  // rdar://11384153
  NSArray* extensions_list =
      [[NSURLFileTypeMappings sharedMappings]
          extensionsForMIMEType:base::SysUTF8ToNSString(mime_type)];

  if (extensions_list) {
    for (NSString* extension in extensions_list)
      extensions->insert(base::SysNSStringToUTF8(extension));
  } else {
    // Huh? Give up.
    FilePath::StringType ext;
    if (GetPreferredExtensionForMimeType(mime_type, &ext))
      extensions->insert(ext);
  }
}

}  // namespace net
