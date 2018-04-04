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

#include "cobalt/overlay_info/overlay_info_registry.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "starboard/mutex.h"
#include "starboard/string.h"

namespace cobalt {
namespace overlay_info {
namespace {

const auto kDelimiter = OverlayInfoRegistry::kDelimiter;

class OverlayInfoRegistryImpl {
 public:
  static OverlayInfoRegistryImpl* GetInstance();

  void Disable();

  void Register(const char* category, const char* str);
  void Register(const char* category, const void* data, size_t data_size);
  void RetrieveAndClear(std::vector<uint8_t>* infos);

 private:
  // Reserve enough data for |infos_| to avoid extra allcations.
  static const size_t kReservedSize = 4096;

  OverlayInfoRegistryImpl() = default;

  // To make our private constructor available to
  // Singleton<OverlayInfoRegistryImpl,
  // LeakySingletonTraits<OverlayInfoRegistryImpl>>.
  friend struct DefaultSingletonTraits<OverlayInfoRegistryImpl>;

  bool enabled_ = true;
  starboard::Mutex mutex_;
  std::vector<uint8_t> infos_;
};

// static
OverlayInfoRegistryImpl* OverlayInfoRegistryImpl::GetInstance() {
  return Singleton<OverlayInfoRegistryImpl,
                   LeakySingletonTraits<OverlayInfoRegistryImpl>>::get();
}

void OverlayInfoRegistryImpl::Disable() {
  starboard::ScopedLock scoped_lock(mutex_);
  enabled_ = false;
  infos_.clear();
}

void OverlayInfoRegistryImpl::Register(const char* category, const char* str) {
  auto length = SbStringGetLength(str);
  Register(category, reinterpret_cast<const uint8_t*>(str), length);
}

void OverlayInfoRegistryImpl::Register(const char* category, const void* data,
                                       size_t data_size) {
  DCHECK(SbStringFindCharacter(
             category, static_cast<char>(OverlayInfoRegistry::kDelimiter)) ==
         NULL)
      << "Category " << category
      << " cannot contain the delimiter:" << OverlayInfoRegistry::kDelimiter;
  auto category_size = SbStringGetLength(category);
  auto total_size = category_size + 1 + data_size;

  DCHECK_GT(category_size, 0u);
  // Use |kMaxSizeOfData + 0| to avoid link error caused by DCHECK_LE.
  DCHECK_LE(total_size, OverlayInfoRegistry::kMaxSizeOfData + 0);

  starboard::ScopedLock scoped_lock(mutex_);
  // Use |kMaxNumberOfPendingOverlayInfo + 0| to avoid link error caused by
  // DCHECK_LE.
  DCHECK_LE(infos_.size() + total_size,
            OverlayInfoRegistry::kMaxNumberOfPendingOverlayInfo + 0);
  if (enabled_) {
    infos_.push_back(static_cast<uint8_t>(total_size));
    infos_.insert(infos_.end(), category, category + category_size);
    infos_.push_back(kDelimiter);
    infos_.insert(infos_.end(), static_cast<const uint8_t*>(data),
                  static_cast<const uint8_t*>(data) + data_size);
  }
}

void OverlayInfoRegistryImpl::RetrieveAndClear(std::vector<uint8_t>* infos) {
  DCHECK(infos);

  starboard::ScopedLock scoped_lock(mutex_);
  if (enabled_) {
    infos->swap(infos_);
    infos_.clear();
    infos_.reserve(kReservedSize);
  }
}

}  // namespace

void OverlayInfoRegistry::Disable() {
  OverlayInfoRegistryImpl::GetInstance()->Disable();
}

void OverlayInfoRegistry::Register(const char* category, const char* str) {
  OverlayInfoRegistryImpl::GetInstance()->Register(category, str);
}

void OverlayInfoRegistry::Register(const char* category, const void* data,
                                   size_t data_size) {
  OverlayInfoRegistryImpl::GetInstance()->Register(category, data, data_size);
}

void OverlayInfoRegistry::RetrieveAndClear(std::vector<uint8_t>* infos) {
  OverlayInfoRegistryImpl::GetInstance()->RetrieveAndClear(infos);
}

}  // namespace overlay_info
}  // namespace cobalt
