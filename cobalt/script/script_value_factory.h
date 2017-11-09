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

#ifndef COBALT_SCRIPT_SCRIPT_VALUE_FACTORY_H_
#define COBALT_SCRIPT_SCRIPT_VALUE_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"

namespace cobalt {
namespace script {
class ScriptValueFactory {
 public:
  typedef ScriptValue<Promise<scoped_refptr<Wrappable> > >
      WrappablePromiseValue;

  // Create a Promise<T> where T is void or a primitive type.
  template <typename T>
  scoped_ptr<ScriptValue<Promise<T> > > CreateBasicPromise() {
    return CreatePromise<T>();
  }

  // Note that Promise<T> where T is an interface will return a
  // Promise<scoped_refptr<Wrappable>>. Calling code could supply any Wrappable
  // when calling Resolve. This is because we need to explicitly instantiate
  // the template specializations for creating Promises. It is not
  // realistic to instantiate the template specialization for every interface
  // defined in Cobalt.
  // TODO: Figure out how to make it so only a scoped_refptr<T> can be accepted.
  template <typename T>
  scoped_ptr<WrappablePromiseValue> CreateInterfacePromise() {
    return CreatePromise<scoped_refptr<Wrappable> >();
  }

  virtual ~ScriptValueFactory() {}

 private:
  // Non-virtual template function that will be implemented in the
  // engine-specific implementation for ScriptValueFactory.
  template <typename T>
  scoped_ptr<ScriptValue<Promise<T> > > CreatePromise();
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_VALUE_FACTORY_H_
