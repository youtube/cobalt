// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/keychain_data_helper_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"

namespace extensions {
namespace {

// Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/1348251 but deprecation
// warnings are disabled in the meanwhile.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Creates an access for a generic password item to share it with other Google
// applications with teamid:EQHXZ8M8AV (taken from the signing certificate).
OSStatus CreateTargetAccess(NSString* service_name, SecAccessRef* access_ref) {
  OSStatus status = noErr;
  status =
      SecAccessCreate(base::mac::NSToCFCast(service_name), nullptr, access_ref);
  if (status != noErr)
    return status;

  base::ScopedCFTypeRef<CFArrayRef> acl_list;
  status = SecAccessCopyACLList(*access_ref, acl_list.InitializeInto());
  if (status != noErr)
    return status;

  for (id acl in base::mac::CFToNSCast(acl_list.get())) {
    base::ScopedCFTypeRef<CFArrayRef> app_list;
    base::ScopedCFTypeRef<CFStringRef> description;
    SecKeychainPromptSelector dummy_prompt_selector;
    status = SecACLCopyContents(
        static_cast<SecACLRef>(acl), app_list.InitializeInto(),
        description.InitializeInto(), &dummy_prompt_selector);
    if (status != noErr)
      return status;

    NSArray* const ns_app_list = base::mac::CFToNSCast(app_list);
    // Replace explicit non-empty app list with void to allow any application
    if (ns_app_list && ns_app_list.count != 0) {
      status = SecACLSetContents(static_cast<SecACLRef>(acl), nullptr,
                                 description, dummy_prompt_selector);
      if (status != noErr)
        return status;
    }
  }

  return noErr;
}

// Verifies whether `keychain` is currently locked. Returns the given OSStatus
// and, if the status is successful, sets `unlocked` to the value representing
// whether `keychain` is currently unlocked or not.
OSStatus VerifyKeychainUnlocked(SecKeychainRef keychain, bool* unlocked) {
  SecKeychainStatus keychain_status;
  OSStatus status = SecKeychainGetStatus(keychain, &keychain_status);
  if (status != noErr) {
    return status;
  }

  *unlocked = keychain_status & kSecUnlockStateStatus;
  return status;
}

}  // namespace

OSStatus WriteKeychainItem(const std::string& service_name,
                           const std::string& account_name,
                           const std::string& password) {
  SecKeychainAttribute attributes[] = {
      {kSecLabelItemAttr, static_cast<UInt32>(service_name.length()),
       const_cast<char*>(service_name.data())},
      {kSecDescriptionItemAttr, 0U, nullptr},
      {kSecGenericItemAttr, 0U, nullptr},
      {kSecCommentItemAttr, 0U, nullptr},
      {kSecServiceItemAttr, static_cast<UInt32>(service_name.length()),
       const_cast<char*>(service_name.data())},
      {kSecAccountItemAttr, static_cast<UInt32>(account_name.length()),
       const_cast<char*>(account_name.data())}};
  SecKeychainAttributeList attribute_list = {std::size(attributes), attributes};

  base::ScopedCFTypeRef<SecAccessRef> access_ref;
  OSStatus status = CreateTargetAccess(base::SysUTF8ToNSString(service_name),
                                       access_ref.InitializeInto());
  if (status != noErr)
    return status;
  return SecKeychainItemCreateFromContent(
      kSecGenericPasswordItemClass, &attribute_list,
      static_cast<UInt32>(password.size()), password.data(), nullptr,
      access_ref.get(), nullptr);
}

OSStatus VerifyKeychainForItemUnlocked(SecKeychainItemRef item_ref,
                                       bool* unlocked) {
  base::ScopedCFTypeRef<SecKeychainRef> keychain;
  OSStatus status =
      SecKeychainItemCopyKeychain(item_ref, keychain.InitializeInto());
  if (status != noErr) {
    return status;
  }

  return VerifyKeychainUnlocked(keychain, unlocked);
}

OSStatus VerifyDefaultKeychainUnlocked(bool* unlocked) {
  base::ScopedCFTypeRef<SecKeychainRef> keychain;
  OSStatus status = SecKeychainCopyDefault(keychain.InitializeInto());
  if (status != noErr) {
    return status;
  }

  return VerifyKeychainUnlocked(keychain, unlocked);
}

#pragma clang diagnostic pop

}  // namespace extensions
