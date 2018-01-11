// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_EXCEPTION_MESSAGE_H_
#define COBALT_SCRIPT_EXCEPTION_MESSAGE_H_

#include <string>

#include "cobalt/script/script_exception.h"

namespace cobalt {
namespace script {

// Simple exceptions as defined in:
// http://heycam.github.io/webidl/#dfn-simple-exception
enum SimpleExceptionType {
  kError,
  kTypeError,
  kRangeError,
  kReferenceError,
  kSyntaxError,
  kURIError
};

// Custom exception message type.
enum MessageType {
  kNoError = -1,

  kSimpleError,
  kSimpleTypeError,
  kSimpleRangeError,
  kSimpleReferenceError,
  kNotDateType,
  kNotNullableType,
  kNotObjectType,
  kNotObjectOrFunction,
  kNotInt64Type,
  kNotUint64Type,
  kNotNumberType,
  kNotIterableType,
  kDoesNotImplementInterface,
  kConvertToStringFailed,
  kNotFinite,
  kNotSupportedType,
  kConvertToUTF8Failed,
  kConvertToEnumFailed,
  kStringifierProblem,
  kNotFunctionValue,
  kInvalidNumberOfArguments,
  kNotUnionType,
  kOutsideBounds,
  kInvalidLength,
  kNotAnArrayBuffer,

  kNumMessageTypes,
};

const char* GetExceptionMessage(MessageType message_type);
SimpleExceptionType GetSimpleExceptionType(MessageType message_type);

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_EXCEPTION_MESSAGE_H_
