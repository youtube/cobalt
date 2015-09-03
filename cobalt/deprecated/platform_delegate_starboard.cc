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
#include "cobalt/deprecated/platform_delegate.h"

#include "base/logging.h"
#include "lbshell/src/lb_globals.h"
#include "starboard/types.h"


namespace cobalt {
namespace deprecated {

// static
void PlatformDelegate::PlatformInit() {
  NOTIMPLEMENTED();
}

std::string PlatformDelegate::GetSystemLanguage() {
  NOTIMPLEMENTED();
  return "en-US";
}

// static
void PlatformDelegate::PlatformUpdateDuringStartup() {
  NOTIMPLEMENTED();
}

// static
void PlatformDelegate::PlatformTeardown() {
  NOTIMPLEMENTED();
}

// static
bool PlatformDelegate::ExitGameRequested() {
  NOTIMPLEMENTED();
  return false;
}

// static
const char *PlatformDelegate::GameTitleId() {
  NOTIMPLEMENTED();
  return "";
}

// static
void PlatformDelegate::PlatformMediaInit() {
  NOTIMPLEMENTED();
}

// static
void PlatformDelegate::PlatformMediaTeardown() {
  NOTIMPLEMENTED();
}

// static
void PlatformDelegate::CheckParentalControl() {
  NOTIMPLEMENTED();
}

// static
bool PlatformDelegate::ProtectOutput() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace deprecated
}  // namespace cobalt
