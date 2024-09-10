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

// Module Overview: Starboard User module
//
// Defines a user management API. This module defines functions only for
// managing signed-in users. Platforms that do not have users must still
// implement this API, always reporting a single user that is current and
// signed in.
//
// These APIs are NOT expected to be thread-safe, so either call them from a
// single thread, or perform proper synchronization around all calls.

#ifndef STARBOARD_USER_H_
#define STARBOARD_USER_H_

#error This file is deprecated with SB_API_VERSION 16.

#endif  // STARBOARD_USER_H_
