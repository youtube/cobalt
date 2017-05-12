// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "starboard/string.h"

#include "starboard/log.h"
#include "starboard/memory.h"

char* SbStringDuplicate(const char* source) {
  size_t length = SbStringGetLength(source);
  char* result = static_cast<char*>(SbMemoryAllocate(length + 1));
  SB_DCHECK(length < kSbInt32Max);
  int int_length = static_cast<int>(length + 1);
  SbStringCopy(result, source, int_length);

  return result;
}
