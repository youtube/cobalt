// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/user_log.h"

#include "base/logging.h"

namespace base {
namespace {

// The max size of the label is 15 ASCII characters, plus a NULL character.
const int kMaxLabelSize = 16;

}  // namespace

// static
bool UserLog::IsRegistrationSupported() {
  // TODO: Reimplement with starboard core dump
  NOTIMPLEMENTED();
  return false;
}

// static
bool UserLog::Register(Index index, const char* label, const void* address,
                       size_t size) {
  DCHECK_LT(strlen(label), kMaxLabelSize);
  NOTIMPLEMENTED();
  return true;
}

bool UserLog::Deregister(Index index) {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace base
