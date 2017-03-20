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

#include "cobalt/script/exception_message.h"

namespace cobalt {
namespace script {

ExceptionMessage kMessages[kNumMessageTypes] = {
    {kSimpleError, kError, " "},
    {kSimpleTypeError, kTypeError, " "},
    {kSimpleRangeError, kRangeError, " "},
    {kSimpleReferenceError, kReferenceError, " "},
    {kNotNullableType, kTypeError, "Value is null but type is not nullable."},
    {kNotObjectType, kTypeError, "Value is not an object."},
    {kNotObjectOrFunction, kTypeError, "Value is not an object or function."},
    {kNotInt64Type, kTypeError, "Value is not int64."},
    {kNotUint64Type, kTypeError, "Value is not uint64."},
    {kNotNumberType, kTypeError, "Value is not a number."},
    {kNotIterableType, kTypeError, "Value is not iterable."},
    {kDoesNotImplementInterface, kTypeError,
     "Value does not implement the interface type."},
    {kConvertToStringFailed, kTypeError,
     "JS value cannot be converted to string."},
    {kNotFinite, kTypeError, "Non-finite floating-point value."},
    {kNotSupportedType, kTypeError, "Value is null but type is not nullable."},
    {kConvertToUTF8Failed, kTypeError, "Failed to convert JS value to utf8."},
    {kConvertToEnumFailed, kTypeError, "Failed to convert JS value to enum."},
    {kStringifierProblem, kTypeError, "Stringifier problem."},
    {kNotFunctionValue, kTypeError, "Value is not a function."},
    {kInvalidNumberOfArguments, kTypeError, "Invalid number of arguments."},
    {kNotUnionType, kTypeError, "Value is not a member of the union type."},
    {kOutsideBounds, kRangeError, "Offset is outside the object's bounds."},
    {kInvalidLength, kRangeError, "Invalid length."},
    {kNotAnArrayBuffer, kTypeError, "Value is not an ArrayBuffer."},
    {kWrongByteOffsetMultiple, kRangeError,
     "Byte offset should be a multiple of %d."},
    {kWrongByteLengthMultiple, kRangeError,
     "Byte length should be a multiple of %d."},
    {kPropertySyntaxError, kSyntaxError, "%s."},
};

const char* GetExceptionMessageFormat(MessageType message_type) {
  DCHECK_GT(message_type, kNoError);
  DCHECK_LT(message_type, kNumMessageTypes);
  return kMessages[message_type].format;
}

SimpleExceptionType GetSimpleExceptionType(MessageType message_type) {
  DCHECK_GT(message_type, kNoError);
  DCHECK_LT(message_type, kNumMessageTypes);
  return kMessages[message_type].exception_type;
}

}  // namespace script
}  // namespace cobalt
