// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_ERROR_REPORT_H_
#define COBALT_SCRIPT_ERROR_REPORT_H_

#include <memory>
#include <string>

#include "cobalt/script/value_handle.h"

namespace cobalt {
namespace script {

struct ErrorReport {
 public:
  ErrorReport() : line_number(0), column_number(0), is_muted(false) {}

  std::string message;
  std::string filename;
  uint32 line_number;
  uint32 column_number;
  std::unique_ptr<script::ValueHandleHolder> error;
  bool is_muted;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_ERROR_REPORT_H_
