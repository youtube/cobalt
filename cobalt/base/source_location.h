// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_SOURCE_LOCATION_H_
#define COBALT_BASE_SOURCE_LOCATION_H_

#include <ostream>
#include <string>

#include "base/logging.h"

namespace base {

// Used by CSS and HTML parsers to define location in a source file.
//
// If file path cannot be determined, provide a name of Web API object
// which triggered the parsing, in the form of "[object ClassName]" (this
// matches JavaScript's toString() format for objects).
//
// For example, a list of CSS declarations assigned to HTMLElement.style
// should have a |file_path| set to "[object CSSStyleDeclaration]", where
// CSSStyleDeclaration is the type of HTMLElement.style property.
//
// Line and column numbers are 1-based.
struct SourceLocation {
  SourceLocation(const std::string& file_path, int line_number,
                 int column_number)
      : file_path(file_path),
        line_number(line_number),
        column_number(column_number) {
    DCHECK_GE(line_number, 1);
    DCHECK_GE(column_number, 1);
  }

  std::string file_path;
  int line_number;
  int column_number;
};

std::ostream& operator<<(std::ostream& out_stream,
                         const SourceLocation& location);

// Offsets the location within embedded source relatively to the start
// of embedded source.
//
// Line and column numbers are 1-based.
inline void AdjustForStartLocation(int start_line_number,
                                   int start_column_number, int* line_number,
                                   int* column_number) {
  DCHECK_GE(start_line_number, 1);
  DCHECK_GE(start_column_number, 1);
  DCHECK(line_number);
  DCHECK(column_number);
  DCHECK_GE(*line_number, 1);
  DCHECK_GE(*column_number, 1);

  *column_number += *line_number == 1 ? start_column_number - 1 : 0;
  *line_number += start_line_number - 1;
}

}  // namespace base

#endif  // COBALT_BASE_SOURCE_LOCATION_H_
