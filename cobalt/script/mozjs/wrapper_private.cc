/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/script/mozjs/wrapper_private.h"

#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

void WrapperPrivate::AddReferencedObject(JS::HandleObject referee) {
  referenced_objects_.push_back(new JS::Heap<JSObject*>(referee));
}

void WrapperPrivate::RemoveReferencedObject(JS::HandleObject referee) {
  for (ReferencedObjectVector::iterator it = referenced_objects_.begin();
       it != referenced_objects_.end(); ++it) {
    if ((**it) == referee) {
      referenced_objects_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

// static
void WrapperPrivate::AddPrivateData(JS::HandleObject wrapper,
                                    const scoped_refptr<Wrappable>& wrappable) {
  WrapperPrivate* private_data = new WrapperPrivate(wrappable, wrapper);
  JS_SetPrivate(wrapper, private_data);
  DCHECK_EQ(JS_GetPrivate(wrapper), private_data);
}

// static
WrapperPrivate* WrapperPrivate::GetFromWrappable(
    const scoped_refptr<Wrappable>& wrappable, JSContext* context,
    WrapperFactory* wrapper_factory) {
  JS::RootedObject wrapper(context, wrapper_factory->GetWrapper(wrappable));
  WrapperPrivate* private_data =
      static_cast<WrapperPrivate*>(JS_GetPrivate(wrapper));
  DCHECK(private_data);
  DCHECK_EQ(private_data->wrappable_, wrappable);
  return private_data;
}

// static
void WrapperPrivate::Finalizer(JSFreeOp* /* free_op */, JSObject* object) {
  WrapperPrivate* wrapper_private =
      reinterpret_cast<WrapperPrivate*>(JS_GetPrivate(object));
  DCHECK(wrapper_private);
  delete wrapper_private;
}

// static
void WrapperPrivate::Trace(JSTracer* trace, JSObject* object) {
  WrapperPrivate* wrapper_private =
      reinterpret_cast<WrapperPrivate*>(JS_GetPrivate(object));
  DCHECK(wrapper_private);
  for (ReferencedObjectVector::iterator it =
           wrapper_private->referenced_objects_.begin();
       it != wrapper_private->referenced_objects_.end(); ++it) {
    JS::Heap<JSObject*>* referenced_object = *it;
    JS_CallHeapObjectTracer(trace, referenced_object, "WrapperPrivate::Trace");
  }
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
