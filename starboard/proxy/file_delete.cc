// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/file.h"

#include "starboard/proxy/starboard_proxy.h"

bool SbFileDelete(const char* path) {
  ::starboard::proxy::file_delete_fn_type file_delete_test_double =
      ::starboard::proxy::GetSbProxy()->GetFileDelete();
  if (file_delete_test_double != NULL) {
    return file_delete_test_double(path);
  } else {
    void* p = ::starboard::proxy::GetSbProxy()->LookupSymbol("SbFileDelete");
    typedef bool (*funcptr)(const char*);
    funcptr sb_file_delete = funcptr(p);
    return sb_file_delete(path);
  }
}
