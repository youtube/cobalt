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

#ifndef COBALT_H5VCC_H5VCC_TRACE_EVENT_H_
#define COBALT_H5VCC_H5VCC_TRACE_EVENT_H_

#include <memory>
#include <string>

#include "base/trace_event/trace_event.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

namespace cobalt {
namespace h5vcc {

#define TRACE_EVENT0_FOR_EACH(MacroOp) MacroOp(Begin, BEGIN) MacroOp(End, END)

#define DEFINE_H5VCC_TRACE_EVENT0(Name, FunctionName)                      \
  void Trace##Name(const std::string& category, const std::string& name) { \
    TRACE_EVENT_COPY_##FunctionName##0(category.c_str(), name.c_str());    \
  }

#define TRACE_EVENT1_FOR_EACH(MacroOp)                                \
  MacroOp(IntBegin, BEGIN, int32) MacroOp(IntEnd, END, int32)         \
      MacroOp(FloatBegin, BEGIN, float) MacroOp(FloatEnd, END, float) \
          MacroOp(StringBegin, BEGIN, const std::string&)             \
              MacroOp(StringEnd, END, const std::string&)

#define DEFINE_H5VCC_TRACE_EVENT1(TraceName, FunctionName, CppType)           \
  void Trace##TraceName(const std::string& category, const std::string& name, \
                        const std::string& arg1_name, CppType arg1_value) {   \
    TRACE_EVENT_COPY_##FunctionName##1(category.c_str(), name.c_str(),        \
                                       arg1_name.c_str(), arg1_value);        \
  }

class H5vccTraceEvent : public script::Wrappable {
 public:
  H5vccTraceEvent();

  void Start(const std::string& output_filename);
  void Stop();

  TRACE_EVENT0_FOR_EACH(DEFINE_H5VCC_TRACE_EVENT0)
  TRACE_EVENT1_FOR_EACH(DEFINE_H5VCC_TRACE_EVENT1)

  DEFINE_WRAPPABLE_TYPE(H5vccTraceEvent);

 private:
  // This object can be set to start a trace.
  // While initialized, it means that a trace is on-going.
  std::unique_ptr<trace_event::ScopedTraceToFile> trace_to_file_;

  DISALLOW_COPY_AND_ASSIGN(H5vccTraceEvent);
};

#undef DEFINE_H5VCC_TRACE_EVENT0
#undef TRACE_EVENT0_FOR_EACH
#undef DEFINE_H5VCC_TRACE_EVENT1
#undef TRACE_EVENT1_FOR_EACH

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_TRACE_EVENT_H_
