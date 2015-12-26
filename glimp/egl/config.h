/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_EGL_CONFIG_H_
#define GLIMP_EGL_CONFIG_H_

#include <EGL/egl.h>

#include <map>
#include <set>
#include <vector>

namespace glimp {
namespace egl {

// Our underlying EGLConfig structure is just a mapping from config key to
// value.
typedef std::map<int, int> Config;
typedef std::map<int, int> ValidatedConfigAttribs;

// Examines the |raw_attribs| list of attributes passed in to eglGetConfig(),
// and converts it into a std::map, placing the results in |validated_attribs|.
// If the attributes are invalid, false is returned, otherwise true is returned.
// |raw_attribs| is an array of integers that alternate between keys and values
// to produce a list of key/value pairs.  The array is terminated with the
// integer EGL_NONE.
bool ValidateConfigAttribList(const EGLint* raw_attribs,
                              ValidatedConfigAttribs* out_validated_attribs);

// Filters the input configs such that only those that match the specified
// |attrib_list| are returned.
std::vector<Config*> FilterConfigs(const std::set<Config*>& configs,
                                   const ValidatedConfigAttribs& attrib_list);

// Sorts the input list in place according to what best metches the specified
// |attrib_list|.  |in_out_configs| will be sorted after this function returns.
void SortConfigs(const ValidatedConfigAttribs& attrib_list,
                 std::vector<Config*>* in_out_configs);

// Convert our internal Config pointer type into a public EGLConfig type.
EGLConfig ToEGLConfig(Config* config);

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_CONFIG_H_
