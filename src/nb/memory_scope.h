/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NB_MEMORY_SCOPE_H_
#define NB_MEMORY_SCOPE_H_

///////////////////////////////////////////////////////////////////////////////
// Macros to define the memory scope objects. These are objects that are used
// to annotate sections of the code base as belonging to a particular memory
// scope. Note that this is an annotation and does not memory allocation.
///////////////////////////////////////////////////////////////////////////////

// Macro to track the memory scope inside a function or block of code. The
// memory scope is in effect until the end of the code block.
// Example:
//   void Foo() {
//     TRACK_MEMORY_SCOPE("FooMemoryScope");
//     // pops the memory scope at the end.
//   }

#if !defined(__cplusplus)
// Disallow macro use for non-C++ builds.
#define TRACK_MEMORY_SCOPE(STR) error_forbidden_in_non_c_plus_plus_code
#define TRACK_MEMORY_SCOPE_DYNAMIC(STR) error_forbidden_in_non_c_plus_plus_code
#elif defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
#define TRACK_MEMORY_SCOPE(STR) TRACK_MEMORY_STATIC_CACHED(STR)
#define TRACK_MEMORY_SCOPE_DYNAMIC(STR) TRACK_MEMORY_STATIC_NOT_CACHED(STR)
#else
// No-op when starboard does not allow memory tracking.
#define TRACK_MEMORY_SCOPE(STR)
#define TRACK_MEMORY_SCOPE_DYNAMIC(STR)
#endif

// Preprocessor needs double expansion in order to __FILE__, __LINE__ and
// __FUNCTION__ properly.
#define TRACK_MEMORY_STATIC_CACHED(STR) \
  TRACK_MEMORY_STATIC_CACHED_IMPL_2(STR, __FILE__, __LINE__, __FUNCTION__)

#define TRACK_MEMORY_STATIC_NOT_CACHED(STR) \
  TRACK_MEMORY_STATIC_NOT_CACHED_IMPL_2(STR, __FILE__, __LINE__, __FUNCTION__)

// Only enable TRACK_MEMORY_STATIC_CACHED_IMPL_2 if starboard allows memory
// tracking.
#define TRACK_MEMORY_STATIC_CACHED_IMPL_2(Str, FileStr, LineNum, FuncStr) \
  static NbMemoryScopeInfo memory_scope_handle_##LineNum =                \
      { NULL, Str, FileStr, LineNum, FuncStr, true };                     \
  NbPushMemoryScope(&memory_scope_handle_##LineNum);                      \
  NbPopMemoryScopeOnScopeEnd pop_on_scope_end_##LineNum;                  \
  (void)pop_on_scope_end_##LineNum;

#define TRACK_MEMORY_STATIC_NOT_CACHED_IMPL_2(Str, FileStr, LineNum, FuncStr) \
  NbMemoryScopeInfo memory_scope_handle_##LineNum = {                         \
      NULL, Str, FileStr, LineNum, FuncStr, false};                           \
  NbPushMemoryScope(&memory_scope_handle_##LineNum);                          \
  NbPopMemoryScopeOnScopeEnd pop_on_scope_end_##LineNum;                      \
  (void)pop_on_scope_end_##LineNum;

#ifdef __cplusplus
extern "C" {
#endif

struct NbMemoryScopeReporter;
struct NbMemoryScopeInfo;

// Sets the memory reporter. Returns true on success, false something
// goes wrong.
bool NbSetMemoryScopeReporter(NbMemoryScopeReporter* reporter);

// Note that we pass by pointer because the memory scope contains a
// variable allowing the result to be cached.
void NbPushMemoryScope(NbMemoryScopeInfo* memory_scope);
void NbPopMemoryScope();

///////////////////////////////////////////////////////////////////////////////
// Implementation
///////////////////////////////////////////////////////////////////////////////
// Interface for handling memory scopes.
typedef void (*NbReportPushMemoryScopeCallback)(void* context,
                                                NbMemoryScopeInfo* info);
typedef void (*NbReportPopMemoryScopeCallback)(void* context);

struct NbMemoryScopeReporter {
  // Callback to report pushing of memory scope.
  NbReportPushMemoryScopeCallback push_memory_scope_cb;

  // Callback to report poping of the memory scope.
  NbReportPopMemoryScopeCallback pop_memory_scope_cb;

  // Optional, is passed to the callbacks as first argument.
  void* context;
};

// This MemoryScope must remain a POD data type so that it can be statically
// initialized.
struct NbMemoryScopeInfo {
  // cached_handle_ allows a cached result of the the fields represented in
  // this struct to be generated and the handle be placed into this field.
  // See also allows_caching_.
  void* cached_handle_;

  // Represents the name of the memory scope. I.E. "Javascript" or "Gfx".
  const char* memory_scope_name_;

  // Represents the file name that this memory scope was created at.
  const char* file_name_;

  // Represents the line number that this memory scope was created at.
  int line_number_;

  // Represents the function name that this memory scope was created at.
  const char* function_name_;

  // When true, if cached_handle_ is 0 then an object may be created that
  // represents the fields of this object. The handle that represents this
  // cached object is then placed in cached_hanlde_.
  bool allows_caching_;
};

// NbPopMemoryScopeOnScopeEnd is only allowed for C++ builds.
#ifdef __cplusplus
// A helper that pops the memory scope at the end of the current code block.
struct NbPopMemoryScopeOnScopeEnd {
  ~NbPopMemoryScopeOnScopeEnd() { NbPopMemoryScope(); }
};
#endif


#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // NB_MEMORY_SCOPE_H_
