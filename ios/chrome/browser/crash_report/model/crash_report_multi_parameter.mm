// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_report_multi_parameter.h"

#import <memory>

#import "base/check.h"
#import "base/json/json_writer.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

namespace {
// The maximum size of the multi parameter key.
const int kMaximumMultiParameterValueSize = 256;
}  // namespace

@implementation CrashReportMultiParameter {
  crash_reporter::CrashKeyString<kMaximumMultiParameterValueSize>* _key;
  base::Value::Dict _dictionary;
}

- (instancetype)initWithKey:
    (crash_reporter::CrashKeyString<kMaximumMultiParameterValueSize>&)key {
  if ((self = [super init])) {
    _dictionary = base::Value::Dict();
    _key = &key;
  }
  return self;
}

- (void)removeValue:(base::StringPiece)key {
  _dictionary.Remove(key);
  [self updateCrashReport];
}

- (void)setValue:(base::StringPiece)key withValue:(int)value {
  _dictionary.Set(key, value);
  [self updateCrashReport];
}

- (void)incrementValue:(base::StringPiece)key {
  const int value = _dictionary.FindInt(key).value_or(0);
  _dictionary.Set(key, value + 1);
  [self updateCrashReport];
}

- (void)decrementValue:(base::StringPiece)key {
  const absl::optional<int> maybe_value = _dictionary.FindInt(key);
  if (maybe_value.has_value()) {
    const int value = maybe_value.value();
    if (value <= 1) {
      _dictionary.Remove(key);
    } else {
      _dictionary.Set(key, value - 1);
    }
    [self updateCrashReport];
  }
}

- (void)updateCrashReport {
  std::string stateAsJson;
  base::JSONWriter::Write(_dictionary, &stateAsJson);
  if (stateAsJson.length() > (kMaximumMultiParameterValueSize - 1)) {
    NOTREACHED();
    return;
  }
  _key->Set(stateAsJson);
  [[PreviousSessionInfo sharedInstance]
      setReportParameterValue:base::SysUTF8ToNSString(stateAsJson)
                       forKey:@"user_application_state"];
}

@end
