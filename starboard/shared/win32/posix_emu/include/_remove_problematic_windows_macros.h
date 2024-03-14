// NOLINT(build/header_guard)
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

// This file should be included in other include files within posix_emu, after
// including MSVC headers that may define macros that collide with Cobalt code.
// When undefining a macro here, provide a comment of which specific Windows
// header file defined it for future reference.
#undef NO_ERROR        // winerror.h; b/302733082#comment15
#undef PostMessage     // winuser.h
#undef GetCurrentTime  // winbase.h; b/324981660
