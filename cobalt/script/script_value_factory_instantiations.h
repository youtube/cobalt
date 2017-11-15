// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_SCRIPT_VALUE_FACTORY_INSTANTIATIONS_H_
#define COBALT_SCRIPT_SCRIPT_VALUE_FACTORY_INSTANTIATIONS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace script {

// Explicit template instantiations for all Promise types that will be used
// in Cobalt.
#define PROMISE_TEMPLATE_INSTANTIATION(TYPE)        \
  template scoped_ptr<ScriptValue<Promise<TYPE> > > \
  ScriptValueFactory::CreatePromise<TYPE>();

PROMISE_TEMPLATE_INSTANTIATION(void);
PROMISE_TEMPLATE_INSTANTIATION(bool);
PROMISE_TEMPLATE_INSTANTIATION(int8_t);
PROMISE_TEMPLATE_INSTANTIATION(int16_t);
PROMISE_TEMPLATE_INSTANTIATION(int32_t);
PROMISE_TEMPLATE_INSTANTIATION(int64_t);
PROMISE_TEMPLATE_INSTANTIATION(uint8_t);
PROMISE_TEMPLATE_INSTANTIATION(uint16_t);
PROMISE_TEMPLATE_INSTANTIATION(uint32_t);
PROMISE_TEMPLATE_INSTANTIATION(uint64_t);
PROMISE_TEMPLATE_INSTANTIATION(float);
PROMISE_TEMPLATE_INSTANTIATION(double);
PROMISE_TEMPLATE_INSTANTIATION(std::string);
PROMISE_TEMPLATE_INSTANTIATION(scoped_refptr<Wrappable>);
PROMISE_TEMPLATE_INSTANTIATION(Sequence<scoped_refptr<Wrappable>>);

#undef PROMISE_TEMPLATE_INSTANTIATION

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_VALUE_FACTORY_INSTANTIATIONS_H_
