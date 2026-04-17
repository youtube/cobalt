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

#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

#include "starboard/extension/features.h"

namespace starboard::features {

template <typename T>
struct SbFeatureParamExt;

class FeatureList {
  typedef std::variant<bool, int, double, std::string, int64_t> ParamValue;
  typedef std::pair<SbFeatureParamType, ParamValue> ParamTypeAndValue;

 public:
  // Initialize the FeatureList instance with the received features and
  // parameters from the Cobalt side. When this function is completed,
  // |is_initialized_| will be set to true.
  static void InitializeFeatureList(const SbFeature* features,
                                    size_t number_of_features,
                                    const SbFeatureParam* params,
                                    size_t number_of_params);

  // Check to see if the given SbFeature is enabled or not.
  static bool IsEnabled(const SbFeature& feature);

  // Check to see if the given SbFeature is enabled or not by name. SbFeatures
  // must be initialized before use. There's an SB_CHECK() to ensure that the
  // given SbFeature must exist in the FeatureList.
  static bool IsEnabledByName(const std::string& feature_name);

  // Template function to retrieve a parameter based on the value type of the
  // parameter.SbParams must be initialized before use. There's an SB_CHECK() to
  // ensure that the given SbFeature must exist in the FeatureList.
  template <typename T>
  static T GetParam(const SbFeatureParamExt<T>& param);

 private:
  FeatureList() {}
  ~FeatureList() = default;

  // Retrieve the singleton instance of the FeatureList. There can only be one
  // instance of features and parameters under the starboard level. We ensure
  // that we use the same instance with this function.
  static FeatureList* GetInstance();

  // This helper function will check for the following:
  //
  // 1) The FeatureList Instance is initialized.
  // 2) The feature that owns the param has been declared.
  // 3) The param object itself has been declared.
  // 4) The type of the param object is the same as the type it was declared as.
  // These are all guarded by SB_CHECKS.
  void ValidateParam(const std::string& feature_name,
                     const std::string& param_name,
                     const SbFeatureParamType& param_type);

  // Helper function to check if the singleton instance of the FeatureList is
  // initialized. Called by other FeatureList class functions. If the
  // FeatureList is accessed before it is initialized, the app will crash.
  bool IsInitialized() { return is_initialized_; }

  // Mutex to ensure that in the rare chance that the FeatureList is
  // being accessed while it is initializing, we can let the instance
  // fully initialize before being accessed.
  std::mutex mutex_;

  // Starboard features will be stored in an std::unordered_map, where the keys
  // are the string representations of the features, and the values are the
  // associated boolean value of the feature.
  std::unordered_map<std::string, bool> features_;  // Guarded by |mutex_|.

  // Starboard feature parameters will be stored in a 2D std::unordered_map,
  // where the outer map's keys will be the string of the feature associated
  // with the parameter. Each key will be associated with an inner
  // std::unordered_map whose key-value pairing is the string representation of
  // the specified parameter and the associated type and value of that
  // parameter, respectively.
  std::unordered_map<std::string,
                     std::unordered_map<std::string, ParamTypeAndValue>>
      params_;

  // When InitializeStarboardFeatures() is completed, this value will be set to
  // true.
  bool is_initialized_ = false;
};

// SbFeatureParamExt is a bridge structure used in starboard to support params
// of different data types, like base::FeatureParam.
template <typename T>
struct SbFeatureParamExt : public SbFeatureParam {
  // Prevent usage with unsupported types.
  static_assert(std::is_same_v<bool, T> || std::is_same_v<int, T> ||
                    std::is_same_v<double, T> ||
                    std::is_same_v<std::string, T> ||
                    std::is_same_v<int64_t, T>,
                "Unsupported Starboard FeatureParam<> type");

  constexpr SbFeatureParamExt(const SbFeature& feature, const char* name)
      : SbFeatureParam{feature.name, name} {}

  // Function used to retrieve the parameter value for a given param. Outside
  // code will call this function, which will then call the corresponding
  // FeatureList::GetParam function.
  T Get() const { return FeatureList::GetParam(*this); }
};

template <>
bool FeatureList::GetParam(const SbFeatureParamExt<bool>& param);
template <>
int FeatureList::GetParam(const SbFeatureParamExt<int>& param);
template <>
double FeatureList::GetParam(const SbFeatureParamExt<double>& param);
template <>
std::string FeatureList::GetParam(const SbFeatureParamExt<std::string>& param);
template <>
int64_t FeatureList::GetParam(const SbFeatureParamExt<int64_t>& param);

#define SB_DECLARE_FEATURE(kFeature) extern const SbFeature kFeature;

// Features should *not* be defined in header files; do not use this macro in
// header files.
#define SB_FEATURE(feature, name, default_state) \
  constexpr SbFeature feature = {name, default_state};

#define SB_DECLARE_FEATURE_PARAM(T, kFeatureParam) \
  extern const SbFeatureParamExt<T> kFeatureParam;

// Params should *not* be defined in header files; do not use this macro in
// header files.
#define SB_FEATURE_PARAM(T, param_object_name, feature, name, default_value) \
  constexpr SbFeatureParamExt<T> param_object_name(feature, name);

}  // namespace starboard::features

#endif  // STARBOARD_SHARED_STARBOARD_FEATURE_LIST_H_
