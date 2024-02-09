// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int strcasecmp(const char* s1, const char* s2) {
  return _stricmp(s1, s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
  return _strnicmp(s1, s2, n);
}

#ifdef __cplusplus
}
#endif
