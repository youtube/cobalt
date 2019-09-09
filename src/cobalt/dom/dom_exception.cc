// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace dom {
namespace {

// https://heycam.github.io/webidl/#dfn-error-names-table
const char* kCodeToErrorName[] = {
  "",
  "IndexSizeError",
  "DOMStringSizeError",
  "HierarchyRequestError",
  "WrongDocumentError",
  "InvalidCharacterError",
  "NoDataAllowedError",
  "NoModificationAllowedError",
  "NotFoundError",
  "NotSupportedError",
  "InUseAttributeError",
  "InvalidStateError",
  "SyntaxError",
  "InvalidModificationError",
  "NamespaceError",
  "InvalidAccessError",
  "ValidationError",
  "TypeMismatchError",
  "SecurityError",
  "NetworkError",
  "AbortError",
  "URLMismatchError",
  "QuotaExceededError",
  "TimeoutError",
  "InvalidNodeTypeError",
  "DataCloneError",
};

const char* GetErrorName(DOMException::ExceptionCode code) {
  // Use the table for errors that have a corresponding code.
  static_assert(arraysize(kCodeToErrorName) ==
                DOMException::kHighestErrCodeValue + 1,
                "Mismatch number of entries in error table");
  if (code <= DOMException::kHighestErrCodeValue) {
    return kCodeToErrorName[code];
  }

  // Error names are enumerated here:
  //   http://heycam.github.io/webidl/#idl-DOMException-error-names
  switch (code) {
    case DOMException::kReadOnlyErr:
      return "ReadOnlyError";
    case DOMException::kInvalidPointerIdErr:
      return "InvalidPointerId";
    case DOMException::kNotAllowedErr:
      return "NotAllowedError";
    default:
      NOTREACHED();
      break;
  }

  return "";
}

DOMException::ExceptionCode GetErrorCode(const std::string& name) {
  for (size_t i = 0; i < arraysize(kCodeToErrorName); ++i) {
    if (name == kCodeToErrorName[i]) {
      return static_cast<DOMException::ExceptionCode>(i);
    }
  }
  return DOMException::kNone;
}

// The code attribute's getter must return the legacy code indicated in the
// error names table for this DOMException object's name, or 0 if no such entry
// exists in the table.
//   https://heycam.github.io/webidl/#dom-domexception-code
DOMException::ExceptionCode GetLegacyCodeValue(
    DOMException::ExceptionCode value) {
  return value > DOMException::kHighestErrCodeValue ? DOMException::kNone
                                                    : value;
}
}  // namespace

DOMException::DOMException(ExceptionCode code)
    : code_(GetLegacyCodeValue(code)), name_(GetErrorName(code)) {}

DOMException::DOMException(ExceptionCode code, const std::string& message)
    : code_(GetLegacyCodeValue(code)),
      name_(GetErrorName(code)),
      message_(message) {}

DOMException::DOMException(const std::string& message, const std::string& name)
    : code_(GetErrorCode(name)),
      name_(name),
      message_(message) {}

// static
void DOMException::Raise(ExceptionCode code,
                         script::ExceptionState* exception_state) {
  DCHECK(exception_state);
  if (exception_state) {
    exception_state->SetException(base::WrapRefCounted(new DOMException(code)));
  }
}

// static
void DOMException::Raise(ExceptionCode code, const std::string& message,
                         script::ExceptionState* exception_state) {
  DCHECK(exception_state);
  if (exception_state) {
    exception_state->SetException(
        base::WrapRefCounted(new DOMException(code, message)));
  }
}

}  // namespace dom
}  // namespace cobalt
