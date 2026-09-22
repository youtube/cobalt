// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef BASE_LOGGING_LOGGING_SETTINGS_H_
#define BASE_LOGGING_LOGGING_SETTINGS_H_

// This file is intentionally empty and exists only for compatibility with
// //base/logging/logging_settings.h.
//
// Callers including "base/logging/logging_settings.h" need this file instead
// of the real one that lives in //base, otherwise the LoggingSettings
// definition will be found in copied_base's logging.h as well as base's
// logging_settings.h.
//
// See https://chromium-review.googlesource.com/c/chromium/src/+/7173024 for
// the culprit CL upstream.

#endif  // BASE_LOGGING_LOGGING_SETTINGS_H_
