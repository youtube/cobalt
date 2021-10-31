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

#ifndef COBALT_DOM_DOM_EXCEPTION_H_
#define COBALT_DOM_DOM_EXCEPTION_H_

#include <string>

#include "cobalt/script/exception_state.h"
#include "cobalt/script/script_exception.h"

namespace cobalt {
namespace dom {

class DOMException : public script::ScriptException {
 public:
  // List of exceptions and codes found here:
  //   http://heycam.github.io/webidl/#idl-DOMException-error-names
  enum ExceptionCode {
    // If the error name does not have a corresponding code, set the code to 0.
    kNone = 0,

    // These errors have legacy code values (corresponding to the enum).
    kIndexSizeErr,                // Deprecated. Use RangeError instead.
    kDomstringSizeErr,            // Deprecated. Use RangeError instead.
    kHierarchyRequestErr,
    kWrongDocumentErr,
    kInvalidCharacterErr,
    kNoDataAllowedErr,            // Deprecated.
    kNoModificationAllowedErr,
    kNotFoundErr,
    kNotSupportedErr,
    kInuseAttributeErr,
    kInvalidStateErr,
    kSyntaxErr,
    kInvalidModificationErr,
    kNamespaceErr,
    // kInvalidAccessErr is Deprecated. Use TypeError for invalid arguments,
    // "NotSupportedError" DOMException for unsupported operations, and
    // "NotAllowedError" DOMException for denied requests instead.
    kInvalidAccessErr,
    kValidationErr,               // Deprecated.
    // Note that TypeMismatchErr is replaced by TypeError but we keep using it
    // to be in sync with Chrome.
    kTypeMismatchErr,
    kSecurityErr,
    kNetworkErr,
    kAbortErr,
    kUrlMismatchErr,
    kQuotaExceededErr,
    kTimeoutErr,
    kInvalidNodeTypeErr,
    kDataCloneErr,

    kHighestErrCodeValue = kDataCloneErr,

    // These errors have no legacy code values. They will use code kNone.
    kReadOnlyErr,
    kInvalidPointerIdErr,
    kNotAllowedErr
  };

  explicit DOMException(ExceptionCode code);
  DOMException(ExceptionCode code, const std::string& message);
  DOMException(const std::string& message, const std::string& name);

  uint16 code() const { return static_cast<uint16>(code_); }
  std::string name() const override { return name_; }
  std::string message() const override { return message_; }

  // Helper function to raise a DOMException in the ExceptionState passed in.
  static void Raise(ExceptionCode code,
                    script::ExceptionState* exception_state);
  static void Raise(ExceptionCode code, const std::string& message,
                    script::ExceptionState* exception_state);

  DEFINE_WRAPPABLE_TYPE(DOMException);
  JSObjectType GetJSObjectType() override { return JSObjectType::kError; }

 private:
  ExceptionCode code_;
  std::string name_;
  std::string message_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_EXCEPTION_H_
