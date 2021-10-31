// Copyright 2016 Google Inc. All Rights Reserved.
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

// A SQLite 3 Virtual File System implementation that can be used in tests.

#ifndef SQL_TEST_VFS_H_
#define SQL_TEST_VFS_H_

#include "third_party/sqlite/sqlite3.h"

namespace sql {

// Registers the test VFS implementation with sqlite3.
void RegisterTestVfs();

// Unregisters the test VFS implementation with sqlite3.
void UnregisterTestVfs();

}  // namespace sql

#endif  // SQL_TEST_VFS_H_
