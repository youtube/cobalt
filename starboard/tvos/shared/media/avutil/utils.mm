// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/tvos/shared/media/avutil/utils.h"

#include <string>

@interface KVOProxyObserver : NSObject {
  starboard::avutil::KVOProxyObserverCallback callback_;
}

- (instancetype)initWithCB:
    (starboard::avutil::KVOProxyObserverCallback)callback;

@end

@implementation KVOProxyObserver

- (instancetype)initWithCB:
    (starboard::avutil::KVOProxyObserverCallback)callback {
  if ([super init]) {
    callback_ = callback;
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  // The callback is called on main thread.
  callback_(keyPath);
}

@end

namespace starboard {
namespace avutil {
namespace {

bool ContainsValueOfClass(NSDictionary* dictionary, NSString* key, Class cla) {
  return dictionary && dictionary[key] && [dictionary[key] isKindOfClass:cla];
}

}  // namespace

NSObject* CreateKVOProxyObserver(KVOProxyObserverCallback callback) {
  return [[KVOProxyObserver alloc] initWithCB:callback];
}

void AppendAVErrorDetails(NSError* error, std::stringstream* ss) {
  if (!error) {
    return;
  }
  *ss << "Error " << error.code << "(" << error.domain.UTF8String << ")";
  if (!error.userInfo) {
    return;
  }
  if (ContainsValueOfClass(error.userInfo, AVErrorMediaTypeKey,
                           [NSString class])) {
    NSString* type = error.userInfo[AVErrorMediaTypeKey];
    *ss << ", type: " << type.UTF8String;
  }
  if (ContainsValueOfClass(error.userInfo, AVErrorMediaSubTypeKey,
                           [NSString class])) {
    NSString* sub_type = error.userInfo[AVErrorMediaSubTypeKey];
    *ss << ", sub_type: " << sub_type.UTF8String;
  }
  if (ContainsValueOfClass(error.userInfo, NSUnderlyingErrorKey,
                           [NSError class])) {
    NSError* underlying_error = error.userInfo[NSUnderlyingErrorKey];
    *ss << ", underlying_err: " << underlying_error.description.UTF8String;
  }
  if (ContainsValueOfClass(error.userInfo, AVErrorPresentationTimeStampKey,
                           [NSValue class])) {
    NSValue* pts = error.userInfo[AVErrorPresentationTimeStampKey];
    *ss << ", pts: " << pts.description.UTF8String;
  }
}

}  // namespace avutil
}  // namespace starboard
