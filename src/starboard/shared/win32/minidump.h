// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_MINIDUMP_H_
#define STARBOARD_SHARED_WIN32_MINIDUMP_H_

namespace starboard {
namespace shared {
namespace win32 {

// After this call, any crashes will cause a mini dump file to be written
// to the file path. Note that the dump file will only be written if the
// process is not running in a debugger (e.g. inside of Visual Studio).
//
// Example 1:
//  InitMiniDumpHandler("cobalt.exe.dmp");  // Relative path to current working
//                                          // directory
// Example 2:
//  InitMiniDumpHandler("C:\\...\\cobalt.exe.dmp")  // Absolute path.
void InitMiniDumpHandler(const char* file_path);

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_MINIDUMP_H_
