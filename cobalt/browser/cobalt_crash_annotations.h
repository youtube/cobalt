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

#ifndef COBALT_BROWSER_COBALT_CRASH_ANNOTATIONS_H_
#define COBALT_BROWSER_COBALT_CRASH_ANNOTATIONS_H_

#include <map>
#include <memory>
#include <string>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "third_party/crashpad/crashpad/client/annotation.h"

namespace cobalt {
namespace browser {

// A singleton class to manage dynamic Crashpad annotations (Crash Keys) in
// Cobalt. This allows capturing context like application state or background
// state that changes during the process lifetime.
//
// NOTE: This class is designed to satisfy Crashpad's requirement for stable
// pointers for annotation keys. Once a key is added to the internal map, its
// name string is never moved or deleted, providing a static lifetime pointer
// for the duration of the process.
class CobaltCrashAnnotations {
 public:
  static CobaltCrashAnnotations* GetInstance();

  // The maximum number of dynamic annotations allowed.
  static constexpr size_t kMaxAnnotations = 16;

  // The maximum size for a crash annotation value in bytes.
  // Hard limit in crashpad is 20KB. We use 2k here to compromise with memory.
  static constexpr size_t kMaxAnnotationSize = 2048;

  // Sets or updates an annotation. This is thread-safe.
  // If 'value' is empty, the annotation is cleared.
  //
  // NOTE: Callers are responsible for ensuring that the 'value' string does not
  // contain PII or other sensitive data. These annotations are for high-level
  // application metadata (e.g., playback state, background status).
  void SetAnnotation(const std::string& name, const std::string& value);

  // Clears a specific annotation. This is thread-safe.
  void ClearAnnotation(const std::string& name);

  // Clears all annotations. This is thread-safe.
  void ClearAllAnnotations();

 private:
  friend class base::NoDestructor<CobaltCrashAnnotations>;
  friend class CobaltCrashAnnotationsTest;

  CobaltCrashAnnotations();
  ~CobaltCrashAnnotations();

  struct AnnotationSlot {
    AnnotationSlot(const std::string& name);
    ~AnnotationSlot();

    std::string name;
    std::unique_ptr<crashpad::StringAnnotation<kMaxAnnotationSize>> annotation;
  };

  base::Lock lock_;
  std::map<std::string, std::unique_ptr<AnnotationSlot>> annotations_;

  CobaltCrashAnnotations(const CobaltCrashAnnotations&) = delete;
  CobaltCrashAnnotations& operator=(const CobaltCrashAnnotations&) = delete;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_CRASH_ANNOTATIONS_H_
