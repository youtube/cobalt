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
// other files. When adding a new feature/parameter for Chrobalt and/or
// Starboard, write the declaration of that feature/parameter in this file.
//
// This file, when included from another file, will help setup the necessary
// structure for that specified file. That file must define and all 6 macros,
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

FEATURE_LIST_START;
STARBOARD_FEATURE(kFeatureFoo, "foo", true);
STARBOARD_FEATURE(kFeatureBar, "bar", false);
FEATURE_LIST_END;
FEATURE_PARAM_LIST_START;
STARBOARD_FEATURE_PARAM(int, kFeatureParamBoo, kFeatureFoo, "boo", 10);
FEATURE_PARAM_LIST_END;
