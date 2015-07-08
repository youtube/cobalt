/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace dom {
namespace {
const char* GetErrorName(DOMException::ExceptionCode code) {
  // Error names are enumerated here:
  //   http://heycam.github.io/webidl/#idl-DOMException-error-names
  switch (code) {
    case DOMException::kNone:
      return "";
    case DOMException::kIndexSizeErr:
      return "IndexSizeError";
  }
  NOTREACHED();
  return "";
}
}  // namespace

DOMException::DOMException(ExceptionCode code)
    : code_(code), name_(GetErrorName(code)) {}

DOMException::DOMException(ExceptionCode code, const std::string& message)
    : code_(code), name_(GetErrorName(code)), message_(message) {}

}  // namespace dom
}  // namespace cobalt
