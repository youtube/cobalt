// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_INTERPOLATE_PROPERTY_VALUE_H_
#define COBALT_CSSOM_INTERPOLATE_PROPERTY_VALUE_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

// Interpolates between the given start and end property values, returning
// a newly created interpolated property value.
scoped_refptr<PropertyValue> InterpolatePropertyValue(
    float progress, const scoped_refptr<PropertyValue>& start_value,
    const scoped_refptr<PropertyValue>& end_value);

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_INTERPOLATE_PROPERTY_VALUE_H_
