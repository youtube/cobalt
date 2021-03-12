// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_CRASHPAD_WRAPPER_ANNOTATIONS_H_
#define THIRD_PARTY_CRASHPAD_WRAPPER_ANNOTATIONS_H_

#define CRASHPAD_ANNOTATION_DEFAULT_LENGTH 64
#define USER_AGENT_STRING_MAX_SIZE 2048

#ifdef __cplusplus
extern "C" {
#endif

// Annotations that Evergreen will add to Crashpad for more detailed crash
// reports.
typedef struct CrashpadAnnotations {
  char product[CRASHPAD_ANNOTATION_DEFAULT_LENGTH];
  char version[CRASHPAD_ANNOTATION_DEFAULT_LENGTH];
  char user_agent_string[USER_AGENT_STRING_MAX_SIZE];
} CrashpadAnnotations;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // THIRD_PARTY_CRASHPAD_WRAPPER_ANNOTATIONS_H_
