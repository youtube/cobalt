// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/common/mutex.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace overlay_info {
namespace {

const auto kDelimiter = OverlayInfoRegistry::kDelimiter;

class OverlayInfoRegistryImpl {
 public:
  static OverlayInfoRegistryImpl* GetInstance();

  void Disable();

  void Register(const char* category, const char* str);
  void RetrieveAndClear(std::string* infos);

 private:
  // Reserve enough data for |infos_| to avoid extra allocations.
  static const size_t kReservedSize = 4096;

  OverlayInfoRegistryImpl() = default;

  // To make our private constructor available to
  // base::Singleton<OverlayInfoRegistryImpl,
  // DefaultSingletonTraits<OverlayInfoRegistryImpl>>.
  friend struct base::DefaultSingletonTraits<OverlayInfoRegistryImpl>;

  bool enabled_ = true;
  starboard::Mutex mutex_;
  std::string infos_;
};

// static
OverlayInfoRegistryImpl* OverlayInfoRegistryImpl::GetInstance() {
  return base::Singleton<
      OverlayInfoRegistryImpl,
      base::DefaultSingletonTraits<OverlayInfoRegistryImpl>>::get();
}

void OverlayInfoRegistryImpl::Disable() {
  starboard::ScopedLock scoped_lock(mutex_);
  enabled_ = false;
  infos_.clear();
}

void OverlayInfoRegistryImpl::Register(const char* category, const char* data) {
  DCHECK(strchr(category, static_cast<char>(OverlayInfoRegistry::kDelimiter)) ==
         NULL)
      << "Category " << category
      << " cannot contain the delimiter:" << OverlayInfoRegistry::kDelimiter;
  DCHECK(strchr(data, static_cast<char>(OverlayInfoRegistry::kDelimiter)) ==
         NULL)
      << "Data " << data
      << " cannot contain the delimiter:" << OverlayInfoRegistry::kDelimiter;
  if (!infos_.empty()) {
    infos_ += kDelimiter;
  }
  infos_ += category;
  infos_ += kDelimiter;
  infos_ += data;
}

void OverlayInfoRegistryImpl::RetrieveAndClear(std::string* infos) {
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

void OverlayInfoRegistry::Register(const char* category, const char* data) {
  OverlayInfoRegistryImpl::GetInstance()->Register(category, data);
}

void OverlayInfoRegistry::Register(const char* category, const void* data,
                                   size_t data_size) {
  const char kHex[] = "0123456789abcdef";

  const uint8_t* data_as_bytes = static_cast<const uint8_t*>(data);
  std::string data_in_hex;

  data_in_hex.reserve(data_size * 2);

  while (data_size > 0) {
    data_in_hex += kHex[*data_as_bytes / 16];
    data_in_hex += kHex[*data_as_bytes % 16];
    ++data_as_bytes;
    --data_size;
  }
  OverlayInfoRegistryImpl::GetInstance()->Register(category,
                                                   data_in_hex.c_str());
}

void OverlayInfoRegistry::RetrieveAndClear(std::string* infos) {
  OverlayInfoRegistryImpl::GetInstance()->RetrieveAndClear(infos);
}

}  // namespace overlay_info
}  // namespace cobalt
