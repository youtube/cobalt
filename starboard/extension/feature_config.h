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
//
// Feature configuration file for Starboard features and feature parameters.
// This file will help create the structure that is defined in
// other files.
//
// This file, when included from another file, will help setup the necessary
// structure for that specified file. That file must define all 6 macros,
// then include this file, and then undefine all 6 macros. Each file will have
// their own definition of each macro for their own specific use.
//
// Doing it this way guarantees that features defined in the Chrobalt side and
// starboard side share the same initial default values.

#ifndef STARBOARD_FEATURE
#error "STARBOARD_FEATURE has to be defined before including this file"
#endif

#ifndef STARBOARD_FEATURE_PARAM
#error "STARBOARD_FEATURE_PARAM has to be defined before including this file"
#endif

#ifndef FEATURE_LIST_START
#error "FEATURE_LIST_START has to be defined before including this file"
#endif

#ifndef FEATURE_LIST_END
#error "FEATURE_LIST_END has to be defined before including this file"
#endif

#ifndef FEATURE_PARAM_LIST_START
#error "FEATURE_PARAM_LIST_START has to be defined before including this file"
#endif

#ifndef FEATURE_PARAM_LIST_END
#error "FEATURE_PARAM_LIST_END has to be defined before including this file"
#endif

#ifndef TIME
#error "TIME has to be defined before including this file"
#endif

// To add a feature to Starboard, use the macro
//
// STARBOARD_FEATURE(kFeature, featureName, defaultState)
//
// where:
//
// - `kFeature` is the C++ identifier that will be used for the Starboard
// Features and `base::Feature`.
// - `featureName` is the feature name, which must be globally unique. This name
// is
//   used to enable/disable features via experiments and command-line flags.
// - `default_state` is the default state to use for the feature, i.e true or
// false.
FEATURE_LIST_START
STARBOARD_FEATURE(kFeatureFoo, "foo", true)
STARBOARD_FEATURE(kFeatureBar, "bar", false)
FEATURE_LIST_END

// To add a parameter to Starboard, use the macro
//
// STARBOARD_FEATURE_PARAM(T, kParam, kFeature, paramName, defaultValue)
//
// where:
//
// - `T` is the value type of the parameter.  We currently support 5 different
// types:
//
// - bool
// - int
// - std::string
// - double
// - base::TimeDelta (represented as int64_t in Starboard)
//
// When wanting to use base::TimeDelta, use the |TIME| macro. Starboard does not
// support base::TimeDelta, so a base::TimeDelta value is read as an int64_t
// value instead. The |TIME| macro is used to help differentiate whether
// base::TimeDelta or int64_t is used. When a parameter using type |TIME| is
// used on the Chrobalt level, |TIME| is defined as base::TimeDelta. When a
// parameter using type |TIME| is used on the Starboard level, |TIME| is defined
// as int64_t.

FEATURE_PARAM_LIST_START
STARBOARD_FEATURE_PARAM(int, kFeatureParamBoo0, kFeatureFoo, "boo1", 10)
STARBOARD_FEATURE_PARAM(bool, kFeatureParamBoo1, kFeatureFoo, "boo2", false)
STARBOARD_FEATURE_PARAM(std::string,
                        kFeatureParamBoo2,
                        kFeatureFoo,
                        "boo3",
                        "hello world")
STARBOARD_FEATURE_PARAM(double, kFeatureParamBoo3, kFeatureFoo, "boo4", 3.14)
STARBOARD_FEATURE_PARAM(TIME,
                        kFeatureParamTime5,
                        kFeatureFoo,
                        "timeParam",
                        base::TimeDelta::Min())
FEATURE_PARAM_LIST_END
