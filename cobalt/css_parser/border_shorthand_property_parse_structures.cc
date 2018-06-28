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

#include "cobalt/css_parser/border_shorthand_property_parse_structures.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/property_list_value.h"

namespace cobalt {
namespace css_parser {

void BorderShorthandToLonghand::Assign4BordersBasedOnPropertyList(
    const scoped_refptr<cssom::PropertyValue>& property_list) {
  DCHECK(property_list);
  DCHECK(property_list->GetTypeId() ==
         base::GetTypeId<cssom::PropertyListValue>());

  const cssom::PropertyListValue* list =
      base::polymorphic_downcast<cssom::PropertyListValue*>(
          property_list.get());
  switch (list->value().size()) {
    case 1: {
      // If right is missing, it is the same as top. A missing left is the same
      // as right. A missing bottom is the same as top.
      border_top = list->value()[0];
      border_right = list->value()[0];
      border_bottom = list->value()[0];
      border_left = list->value()[0];
      break;
    }
    case 2: {
      // If left is mising, it is the same as right; If bottom is missing, it is
      // the same as top.
      border_top = list->value()[0];
      border_right = list->value()[1];
      border_bottom = list->value()[0];
      border_left = list->value()[1];
      break;
    }
    case 3: {
      // A missing left is the same as right.
      border_top = list->value()[0];
      border_right = list->value()[1];
      border_bottom = list->value()[2];
      border_left = list->value()[1];
      break;
    }
    case 4: {
      border_top = list->value()[0];
      border_right = list->value()[1];
      border_bottom = list->value()[2];
      border_left = list->value()[3];
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace css_parser
}  // namespace cobalt
