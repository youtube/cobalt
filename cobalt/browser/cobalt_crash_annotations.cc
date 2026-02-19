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

#include "cobalt/browser/cobalt_crash_annotations.h"

namespace cobalt {
namespace browser {

// static
CobaltCrashAnnotations* CobaltCrashAnnotations::GetInstance() {
  static base::NoDestructor<CobaltCrashAnnotations> instance;
  return instance.get();
}

CobaltCrashAnnotations::CobaltCrashAnnotations() = default;
CobaltCrashAnnotations::~CobaltCrashAnnotations() = default;

void CobaltCrashAnnotations::SetAnnotation(const std::string& name,
                                           const std::string& value) {
  if (value.empty()) {
    ClearAnnotation(name);
    return;
  }

  // NOTE: Callers are responsible for ensuring that the 'value' string does not
  // contain PII or other sensitive data, as these annotations are included in
  // crash reports uploaded to Google servers.
  base::AutoLock lock(lock_);
  auto it = annotations_.find(name);
  if (it != annotations_.end()) {
    it->second->annotation->Set(value);
    return;
  }

  if (annotations_.size() >= kMaxAnnotations) {
    return;
  }

  // Create a new slot. The name in the slot provides a stable pointer
  // for the StringAnnotation constructor.
  auto slot = std::make_unique<AnnotationSlot>(name);
  slot->annotation->Set(value);
  annotations_[name] = std::move(slot);
}

void CobaltCrashAnnotations::ClearAnnotation(const std::string& name) {
  base::AutoLock lock(lock_);
  auto it = annotations_.find(name);
  if (it != annotations_.end()) {
    it->second->annotation->Clear();
  }
}

void CobaltCrashAnnotations::ClearAllAnnotations() {
  base::AutoLock lock(lock_);
  for (auto const& [name, slot] : annotations_) {
    slot->annotation->Clear();
  }
  annotations_.clear();
}

CobaltCrashAnnotations::AnnotationSlot::AnnotationSlot(const std::string& name)
    : name(name),
      annotation(
          std::make_unique<crashpad::StringAnnotation<kMaxAnnotationSize>>(
              this->name.c_str())) {}

CobaltCrashAnnotations::AnnotationSlot::~AnnotationSlot() = default;

}  // namespace browser
}  // namespace cobalt
