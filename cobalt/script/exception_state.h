/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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
#ifndef COBALT_SCRIPT_EXCEPTION_STATE_H_
#define COBALT_SCRIPT_EXCEPTION_STATE_H_

#include <string>

#include "cobalt/script/exception_message.h"
#include "cobalt/script/script_exception.h"

namespace cobalt {
namespace script {

class ExceptionState {
 public:
  // IDL for this object must be an exception interface.
  virtual void SetException(
      const scoped_refptr<ScriptException>& exception) = 0;
  virtual void SetSimpleException(MessageType message_type, ...) = 0;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_EXCEPTION_STATE_H_
