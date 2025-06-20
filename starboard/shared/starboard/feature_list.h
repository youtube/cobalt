// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_FEATURE_LIST_H_
#define STARBOARD_SHARED_STARBOARD_FEATURE_LIST_H_

#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include "starboard/common/mutex.h"
#include "starboard/extension/features.h"

namespace starboard::features {

template <typename T>
struct SbFeatureParamExt;

class FeatureList {
 public:
  // ...
  static FeatureList* GetInstance();
  // ...
  static void InitializeFeatureList(const SbFeature* features,
                                    size_t number_of_features,
                                    const SbFeatureParam* params,
                                    size_t number_of_params);
  // ...
  // Add SB_CHECK() for uninitialized feature.
  static bool IsEnabled(const SbFeature& feature);
  // ...
  // Add SB_CHECK() for uninitiliazed param.

  template <typename T>
  static T GetParam(const SbFeatureParamExt<T>& param);

 private:
  FeatureList() {}
  ~FeatureList();

  Mutex mutex_;
  std::unordered_map<std::string, bool> features_;
  std::unordered_map<
      std::string,
      std::unordered_map<
          std::string,
          std::variant<bool, std::string, int, double, size_t, int64_t>>>
      params_;
  bool IS_INITIALIZED_ = 0;
};

// SbFeatureParamExt is a bridge structure used in starboard to support
// params of different data types, like base::FeatureParam.
template <typename T>
struct SbFeatureParamExt : public SbFeatureParam {
  // Prevent usage with unsupported types.
  static_assert(std::is_same_v<bool, T> || std::is_same_v<int, T> ||
                    std::is_same_v<size_t, T> || std::is_same_v<double, T> ||
                    std::is_same_v<std::string, T> ||
                    std::is_same_v<int64_t, T>,
                "Unsupported Starboard FeatureParam<> type");

  constexpr SbFeatureParamExt(const SbFeature& feature, const char* name)
      : SbFeatureParam{feature.name, name} {}

  T Get() const { return FeatureList::GetParam(*this); }
};

template <>
bool FeatureList::GetParam(const SbFeatureParamExt<bool>& param);
template <>
int FeatureList::GetParam(const SbFeatureParamExt<int>& param);
template <>
std::string FeatureList::GetParam(const SbFeatureParamExt<std::string>& param);
template <>
size_t FeatureList::GetParam(const SbFeatureParamExt<size_t>& param);
template <>
int64_t FeatureList::GetParam(const SbFeatureParamExt<int64_t>& param);
template <>
double FeatureList::GetParam(const SbFeatureParamExt<double>& param);

#define SB_DECLARE_FEATURE(kFeature) extern const SbFeature kFeature

// Features should *not* be defined in header files; do not use this macro in
// header files.
#define SB_FEATURE(feature, name, default_state) \
  constexpr SbFeature feature = {name, default_state};

#define SB_DECLARE_FEATURE_PARAM(T, kFeatureParam) \
  extern const SbFeatureParamExt<T> kFeatureParam

// Params should *not* be defined in header files; do not use this macro in
// header files.
#define SB_FEATURE_PARAM(T, param_object_name, feature, name, default_value) \
  constexpr SbFeatureParamExt<T> param_object_name(feature, name)

}  // namespace starboard::features

#endif  // STARBOARD_SHARED_STARBOARD_FEATURE_LIST_H_
