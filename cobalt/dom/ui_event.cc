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

#include "cobalt/dom/ui_event.h"

namespace cobalt {
namespace dom {

namespace {

const char* kUp = "keyup";
const char* kDown = "keydown";
const char* kPress = "keypress";

const char* ConvertEventType(const UIEvent::Type& type) {
  switch (type) {
    case UIEvent::kKeyUp:
      return kUp;
    case UIEvent::kRawKeyDown:
      return kDown;
    case UIEvent::kChar:
      return kPress;
    case UIEvent::kKeyDown:
      DLOG(FATAL) << "The caller should disambiguate the combined event into "
                  << "RawKeyDown or Char events";
      break;
    default:
      break;
  }

  NOTREACHED();
  return kDown;
}

}  // namespace

UIEvent::UIEvent(Type type, const scoped_refptr<Window>& view)
    : type_enum_(type), type_(ConvertEventType(type)), view_(view) {}

}  // namespace dom
}  // namespace cobalt
