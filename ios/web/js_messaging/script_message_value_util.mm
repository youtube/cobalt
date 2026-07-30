// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/script_message_value_util.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/script_message_dict_value.h"
#import "ios/web/public/js_messaging/script_message_list_value.h"
#import "ios/web/public/js_messaging/script_message_value.h"

namespace web {

ScriptMessageValue CreateScriptMessageValue(id element) {
  if (!element) {
    return ScriptMessageValue();
  }
  CFTypeID type_id = CFGetTypeID((__bridge CFTypeRef)element);
  if (type_id == CFStringGetTypeID()) {
    return ScriptMessageValue(base::SysNSStringToUTF8(element));
  }
  if (type_id == CFNumberGetTypeID()) {
    if (CFNumberIsFloatType((CFNumberRef)element)) {
      return ScriptMessageValue([element doubleValue]);
    }
    return ScriptMessageValue([element intValue]);
  }
  if (type_id == CFBooleanGetTypeID()) {
    return ScriptMessageValue([element boolValue]);
  }
  if (type_id == CFDictionaryGetTypeID()) {
    return ScriptMessageValue(ScriptMessageDictValue(element));
  }
  if (type_id == CFArrayGetTypeID()) {
    return ScriptMessageValue(ScriptMessageListValue(element));
  }
  NOTREACHED();
}

}  // namespace web
