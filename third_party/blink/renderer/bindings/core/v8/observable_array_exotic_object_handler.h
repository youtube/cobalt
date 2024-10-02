// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_EXOTIC_OBJECT_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_EXOTIC_OBJECT_HANDLER_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/observable_array.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace blink {

namespace bindings {

// ObservableArrayExoticObjectHandler implements a handler object which is part
// of an observable array exotic object.
//
//   let observable_array_exotic_object = new Proxy(target, handler);
// where
//   target = v8::Array that has a private property to the V8 wrapper of Blink
//       implementation of observable array backing list object.
//   handler = v8::Object that has a set of trap functions implemented in
//       ObservableArrayExoticObjectHandler.
//
// https://webidl.spec.whatwg.org/#creating-an-observable-array-exotic-object
//
// Implementation notes:
// - v8::Value::ToArrayIndex returns an empty handle if the conversion fails
//   without throwing an exception despite that the return type is
//   v8::MaybeLocal.
// - The case of 'the property == an array index' has priority over the case of
//   'the property == "length"' in order to optimize the former case, which is
//   likely to happen more frequently.
template <typename BackingListWrappable, typename ElementIdlType>
class ObservableArrayExoticObjectHandler {
  STATIC_ONLY(ObservableArrayExoticObjectHandler);

 public:
  // https://webidl.spec.whatwg.org/#es-observable-array-defineProperty
  static void TrapDefineProperty(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    ExceptionState exception_state(
        isolate, ExceptionContext::Context::kOperationInvoke,
        BackingListWrappable::ObservableArrayNameInIDL(), "defineProperty");
    if (!(info[0]->IsArray() && info[1]->IsName() && info[2]->IsObject())) {
      exception_state.ThrowTypeError("Invalid argument.");
      return;
    }
    v8::Local<v8::Array> v8_target = info[0].As<v8::Array>();
    v8::Local<v8::Name> v8_property = info[1].As<v8::Name>();
    v8::Local<v8::Object> v8_desc_obj = info[2].As<v8::Object>();
    BackingListWrappable& backing_list = ToWrappableOrDie(isolate, v8_target);

    V8PropertyDescriptorBag desc_bag;
    V8ObjectToPropertyDescriptor(isolate, v8_desc_obj, desc_bag,
                                 exception_state);
    if (exception_state.HadException())
      return;

    if (v8_property->IsString()) {
      v8::Local<v8::Uint32> v8_index;
      if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
        if ((desc_bag.has_get || desc_bag.has_set) ||
            (desc_bag.has_configurable && !desc_bag.configurable) ||
            (desc_bag.has_enumerable && !desc_bag.enumerable) ||
            (desc_bag.has_writable && !desc_bag.writable)) {
          V8SetReturnValue(info, false);
          return;
        }
        uint32_t index = v8_index->Value();
        DoSetTheIndexedValue(isolate, current_context, backing_list, index,
                             desc_bag.value, exception_state);
        if (exception_state.HadException())
          return;
        V8SetReturnValue(info, true);
        return;
      }

      if (v8_property.As<v8::String>()->StringEquals(
              V8AtomicString(isolate, "length"))) {
        if ((desc_bag.has_get || desc_bag.has_set) ||
            (desc_bag.has_configurable && desc_bag.configurable) ||
            (desc_bag.has_enumerable && desc_bag.enumerable) ||
            (desc_bag.has_writable && !desc_bag.writable)) {
          V8SetReturnValue(info, false);
          return;
        }
        DoSetTheLength(isolate, current_context, backing_list, desc_bag.value,
                       exception_state);
        if (exception_state.HadException())
          return;
        V8SetReturnValue(info, true);
        return;
      }
    }

    bool is_defined = false;
    if (desc_bag.has_get || desc_bag.has_set) {
      v8::PropertyDescriptor desc(desc_bag.get, desc_bag.set);
      if (desc_bag.has_configurable)
        desc.set_configurable(desc_bag.configurable);
      if (desc_bag.has_enumerable)
        desc.set_enumerable(desc_bag.enumerable);
      if (!v8_target->DefineProperty(current_context, v8_property, desc)
               .To(&is_defined)) {
        return;
      }
    } else {
      v8::PropertyDescriptor desc(desc_bag.value, desc_bag.writable);
      if (desc_bag.has_configurable)
        desc.set_configurable(desc_bag.configurable);
      if (desc_bag.has_enumerable)
        desc.set_enumerable(desc_bag.enumerable);
      if (!v8_target->DefineProperty(current_context, v8_property, desc)
               .To(&is_defined)) {
        return;
      }
    }
    V8SetReturnValue(info, is_defined);
  }

  // https://webidl.spec.whatwg.org/#es-observable-array-deleteProperty
  static void TrapDeleteProperty(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    if (!(info[0]->IsArray() && info[1]->IsName())) {
      ExceptionState exception_state(
          isolate, ExceptionContext::Context::kOperationInvoke,
          BackingListWrappable::ObservableArrayNameInIDL(), "deleteProperty");
      exception_state.ThrowTypeError("Invalid argument.");
      return;
    }
    v8::Local<v8::Array> v8_target = info[0].As<v8::Array>();
    v8::Local<v8::Name> v8_property = info[1].As<v8::Name>();
    BackingListWrappable& backing_list = ToWrappableOrDie(isolate, v8_target);

    if (v8_property->IsString()) {
      v8::Local<v8::Uint32> v8_index;
      if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
        uint32_t index = v8_index->Value();
        if (!(backing_list.size() != 0 && index == backing_list.size() - 1)) {
          V8SetReturnValue(info, false);
          return;
        }
        ScriptState* script_state = ScriptState::From(current_context);
        ExceptionState exception_state(
            isolate, ExceptionContext::Context::kIndexedPropertyDelete,
            BackingListWrappable::ObservableArrayNameInIDL());
        if (!RunDeleteAlgorithm(script_state, backing_list, index,
                                exception_state)) {
          return;
        }
        backing_list.pop_back();
        V8SetReturnValue(info, true);
        return;
      }

      if (v8_property.As<v8::String>()->StringEquals(
              V8AtomicString(isolate, "length"))) {
        V8SetReturnValue(info, false);
        return;
      }
    }

    bool is_deleted = false;
    if (!v8_target->Delete(current_context, v8_property).To(&is_deleted))
      return;
    V8SetReturnValue(info, is_deleted);
  }

  // https://webidl.spec.whatwg.org/#es-observable-array-get
  static void TrapGet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    if (!(info[0]->IsArray() && info[1]->IsName())) {
      ExceptionState exception_state(
          isolate, ExceptionContext::Context::kOperationInvoke,
          BackingListWrappable::ObservableArrayNameInIDL(), "get");
      exception_state.ThrowTypeError("Invalid argument.");
      return;
    }
    v8::Local<v8::Array> v8_target = info[0].As<v8::Array>();
    v8::Local<v8::Name> v8_property = info[1].As<v8::Name>();
    BackingListWrappable& backing_list = ToWrappableOrDie(isolate, v8_target);

    if (v8_property->IsString()) {
      v8::Local<v8::Uint32> v8_index;
      if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
        uint32_t index = v8_index->Value();
        if (backing_list.size() <= index) {
          V8SetReturnValue(info, v8::Undefined(isolate));
          return;
        }
        v8::Local<v8::Value> v8_element;
        ScriptState* script_state = ScriptState::From(current_context);
        if (!ToV8Traits<ElementIdlType>::ToV8(script_state, backing_list[index])
                 .ToLocal(&v8_element)) {
          return;
        }
        V8SetReturnValue(info, v8_element);
        return;
      }

      if (v8_property.As<v8::String>()->StringEquals(
              V8AtomicString(isolate, "length"))) {
        V8SetReturnValue(info, backing_list.size());
        return;
      }
    }

    v8::Local<v8::Value> v8_value;
    if (!v8_target->Get(current_context, v8_property).ToLocal(&v8_value))
      return;
    V8SetReturnValue(info, v8_value);
  }

  // https://webidl.spec.whatwg.org/#es-observable-array-getOwnPropertyDescriptor
  static void TrapGetOwnPropertyDescriptor(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    if (!(info[0]->IsArray() && info[1]->IsName())) {
      ExceptionState exception_state(
          isolate, ExceptionContext::Context::kOperationInvoke,
          BackingListWrappable::ObservableArrayNameInIDL(),
          "getOwnPropertyDescriptor");
      exception_state.ThrowTypeError("Invalid argument.");
      return;
    }
    v8::Local<v8::Array> v8_target = info[0].As<v8::Array>();
    v8::Local<v8::Name> v8_property = info[1].As<v8::Name>();
    BackingListWrappable& backing_list = ToWrappableOrDie(isolate, v8_target);

    if (v8_property->IsString()) {
      v8::Local<v8::Uint32> v8_index;
      if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
        uint32_t index = v8_index->Value();
        if (backing_list.size() <= index) {
          V8SetReturnValue(info, v8::Undefined(isolate));
          return;
        }
        v8::Local<v8::Value> v8_element;
        ScriptState* script_state = ScriptState::From(current_context);
        if (!ToV8Traits<ElementIdlType>::ToV8(script_state, backing_list[index])
                 .ToLocal(&v8_element)) {
          return;
        }
        v8::PropertyDescriptor prop_desc(v8_element, true);
        prop_desc.set_configurable(true);
        prop_desc.set_enumerable(true);
        V8SetReturnValue(info, prop_desc);
        return;
      }

      if (v8_property.As<v8::String>()->StringEquals(
              V8AtomicString(isolate, "length"))) {
        v8::PropertyDescriptor prop_desc(
            v8::Integer::NewFromUnsigned(isolate, backing_list.size()), true);
        prop_desc.set_configurable(false);
        prop_desc.set_enumerable(false);
        V8SetReturnValue(info, prop_desc);
        return;
      }
    }

    v8::Local<v8::Value> v8_value;
    if (!v8_target->GetOwnPropertyDescriptor(current_context, v8_property)
             .ToLocal(&v8_value)) {
      return;
    }
    V8SetReturnValue(info, v8_value);
  }

  // https://webidl.spec.whatwg.org/#es-observable-array-has
  static void TrapHas(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    if (!(info[0]->IsArray() && info[1]->IsName())) {
      ExceptionState exception_state(
          isolate, ExceptionContext::Context::kOperationInvoke,
          BackingListWrappable::ObservableArrayNameInIDL(), "has");
      exception_state.ThrowTypeError("Invalid argument.");
      return;
    }
    v8::Local<v8::Array> v8_target = info[0].As<v8::Array>();
    v8::Local<v8::Name> v8_property = info[1].As<v8::Name>();
    BackingListWrappable& backing_list = ToWrappableOrDie(isolate, v8_target);

    if (v8_property->IsString()) {
      v8::Local<v8::Uint32> v8_index;
      if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
        uint32_t index = v8_index->Value();
        V8SetReturnValue(info, index < backing_list.size());
        return;
      }

      if (v8_property.As<v8::String>()->StringEquals(
              V8AtomicString(isolate, "length"))) {
        V8SetReturnValue(info, true);
        return;
      }
    }

    bool is_has = false;
    if (!v8_target->Has(current_context, v8_property).To(&is_has))
      return;
    V8SetReturnValue(info, is_has);
  }

  // https://webidl.spec.whatwg.org/#es-observable-array-ownKeys
  static void TrapOwnKeys(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    if (!info[0]->IsArray()) {
      ExceptionState exception_state(
          isolate, ExceptionContext::Context::kOperationInvoke,
          BackingListWrappable::ObservableArrayNameInIDL(), "ownKeys");
      exception_state.ThrowTypeError("Invalid argument.");
      return;
    }
    v8::Local<v8::Array> v8_target = info[0].As<v8::Array>();
    BackingListWrappable& backing_list = ToWrappableOrDie(isolate, v8_target);

    // 2. Let length be handler.[[BackingList]]'s size.
    // 3. Let keys be an empty list.
    // 4. Let i be 0.
    // 5. While i < length :
    // 5.1. Append !ToString(i) to keys.
    // 5.2. Set i to i + 1.
    Vector<String> keys_vector;
    keys_vector.ReserveInitialCapacity(backing_list.size());
    for (uint32_t index = 0; index < backing_list.size(); ++index)
      keys_vector.push_back(String::Number(index));
    v8::Local<v8::Value> own_keys_as_value;
    if (!ToV8Traits<IDLSequence<IDLString>>::ToV8(
             ScriptState::From(current_context), keys_vector)
             .ToLocal(&own_keys_as_value)) {
      return;
    }
    v8::Local<v8::Array> own_keys = own_keys_as_value.As<v8::Array>();

    // 6. Extend keys with ! O.[[OwnPropertyKeys]]().
    uint32_t own_keys_index = backing_list.size();
    v8::Local<v8::Array> own_props;
    if (!v8_target->GetOwnPropertyNames(current_context, v8::ALL_PROPERTIES)
             .ToLocal(&own_props)) {
      return;
    }
    const uint32_t own_props_length = own_props->Length();
    for (uint32_t index = 0; index < own_props_length; ++index) {
      v8::Local<v8::Value> prop_name;
      if (!own_props->Get(current_context, index).ToLocal(&prop_name))
        return;
      bool is_created = false;
      if (!own_keys
               ->CreateDataProperty(current_context, own_keys_index++,
                                    prop_name)
               .To(&is_created)) {
        return;
      }
      DCHECK(is_created);
    }

    // 7. Return !CreateArrayFromList(keys).
    V8SetReturnValue(info, own_keys);
  }

  // https://webidl.spec.whatwg.org/#es-observable-array-preventExtensions
  static void TrapPreventExtensions(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    V8SetReturnValue(info, false);
  }

  // https://webidl.spec.whatwg.org/#es-observable-array-set
  static void TrapSet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    if (!(info[0]->IsArray() && info[1]->IsName())) {
      ExceptionState exception_state(
          isolate, ExceptionContext::Context::kOperationInvoke,
          BackingListWrappable::ObservableArrayNameInIDL(), "set");
      exception_state.ThrowTypeError("Invalid argument.");
      return;
    }
    v8::Local<v8::Array> v8_target = info[0].As<v8::Array>();
    v8::Local<v8::Name> v8_property = info[1].As<v8::Name>();
    v8::Local<v8::Value> v8_value = info[2];
    BackingListWrappable& backing_list = ToWrappableOrDie(isolate, v8_target);

    if (v8_property->IsString()) {
      v8::Local<v8::Uint32> v8_index;
      if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
        ExceptionState exception_state(
            isolate, ExceptionContext::Context::kIndexedPropertySet,
            BackingListWrappable::ObservableArrayNameInIDL());
        uint32_t index = v8_index->Value();
        bool result =
            DoSetTheIndexedValue(isolate, current_context, backing_list, index,
                                 v8_value, exception_state);
        V8SetReturnValue(info, result);
        return;
      }

      if (v8_property.As<v8::String>()->StringEquals(
              V8AtomicString(isolate, "length"))) {
        ExceptionState exception_state(
            isolate, ExceptionContext::Context::kAttributeSet,
            BackingListWrappable::ObservableArrayNameInIDL(), "length");
        bool result = DoSetTheLength(isolate, current_context, backing_list,
                                     v8_value, exception_state);
        V8SetReturnValue(info, result);
        return;
      }
    }

    bool is_set = false;
    if (!v8_target->Set(current_context, v8_property, v8_value).To(&is_set))
      return;
    V8SetReturnValue(info, is_set);
  }

  // https://webidl.spec.whatwg.org/#dfn-attribute-setter
  // step 4.5.10. If attribute’s type is an observable array type with type
  //   argument T:
  static void PerformAttributeSet(ScriptState* script_state,
                                  BackingListWrappable& backing_list,
                                  v8::Local<v8::Value> v8_value,
                                  ExceptionState& exception_state) {
    v8::Isolate* isolate = script_state->GetIsolate();
    // step 4.5.10.1. Let newValues be the result of converting V to an IDL
    //   value of type sequence<T>.
    auto&& blink_value =
        NativeValueTraits<IDLSequence<ElementIdlType>>::NativeValue(
            isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    // step 4.5.10.3. Set the length of oa.[[ProxyHandler]] to 0.
    if (!DoSetTheLength(isolate, script_state->GetContext(), backing_list, 0,
                        exception_state)) {
      return;
    }
    // step 4.5.10.4. Let i be 0.
    // step 4.5.10.5. While i < newValues’s size:
    for (typename BackingListWrappable::size_type i = 0; i < blink_value.size();
         ++i) {
      // step 4.5.10.5.1. Perform the algorithm steps given by
      //   oa.[[ProxyHandler]].[[SetAlgorithm]], given newValues[i] and i.
      if (!RunSetAlgorithm(script_state, backing_list, i, blink_value[i],
                           exception_state)) {
        return;
      }
      // step 4.5.10.5.2. Append newValues[i] to
      //   oa.[[ProxyHandler]].[[BackingList]].
      backing_list.push_back(std::move(blink_value[i]));
    }
  }

 private:
  static BackingListWrappable& ToWrappableOrDie(v8::Isolate* isolate,
                                                v8::Local<v8::Array> target) {
    bindings::ObservableArrayBase* base =
        bindings::ObservableArrayExoticObjectImpl::
            ProxyTargetToObservableArrayBaseOrDie(isolate, target);
    return *static_cast<BackingListWrappable*>(base);
  }

  // https://webidl.spec.whatwg.org/#observable-array-exotic-object-set-the-length
  static bool DoSetTheLength(v8::Isolate* isolate,
                             v8::Local<v8::Context> current_context,
                             BackingListWrappable& backing_list,
                             v8::Local<v8::Value> v8_length,
                             ExceptionState& exception_state) {
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Uint32> v8_length_uint32;
    if (!v8_length->ToUint32(current_context).ToLocal(&v8_length_uint32)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return false;
    }
    v8::Local<v8::Number> v8_length_number;
    if (!v8_length->ToNumber(current_context).ToLocal(&v8_length_number)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return false;
    }
    if (v8_length_uint32->Value() != v8_length_number->Value()) {
      exception_state.ThrowRangeError("The provided length is invalid.");
      return false;
    }
    uint32_t length = v8_length_uint32->Value();

    return DoSetTheLength(isolate, current_context, backing_list, length,
                          exception_state);
  }

  // https://webidl.spec.whatwg.org/#observable-array-exotic-object-set-the-length
  static bool DoSetTheLength(v8::Isolate* isolate,
                             v8::Local<v8::Context> current_context,
                             BackingListWrappable& backing_list,
                             uint32_t length,
                             ExceptionState& exception_state) {
    if (backing_list.size() < length)
      return false;

    if (backing_list.size() == 0)
      return true;

    ScriptState* script_state = ScriptState::From(current_context);
    uint32_t index_to_delete = backing_list.size() - 1;
    while (length <= index_to_delete) {
      if (!RunDeleteAlgorithm(script_state, backing_list, index_to_delete,
                              exception_state)) {
        return false;
      }

      backing_list.pop_back();
      if (index_to_delete == 0)
        break;
      --index_to_delete;
    }
    return true;
  }

  // https://webidl.spec.whatwg.org/#observable-array-exotic-object-set-the-indexed-value
  static bool DoSetTheIndexedValue(v8::Isolate* isolate,
                                   v8::Local<v8::Context> current_context,
                                   BackingListWrappable& backing_list,
                                   uint32_t index,
                                   v8::Local<v8::Value> v8_value,
                                   ExceptionState& exception_state) {
    if (backing_list.size() < index)
      return false;

    // The return type of NativeValueTraits<T>::NativeValue may be different
    // from BackingListWrappable::value_type.  Convert the type here only once,
    // and use the same value when running RunSetAlgorithm and when storing the
    // value into the backing list.
    typename BackingListWrappable::value_type blink_value =
        NativeValueTraits<ElementIdlType>::NativeValue(isolate, v8_value,
                                                       exception_state);
    if (exception_state.HadException())
      return false;

    ScriptState* script_state = ScriptState::From(current_context);
    if (index < backing_list.size()) {
      if (!RunDeleteAlgorithm(script_state, backing_list, index,
                              exception_state)) {
        return false;
      }
    }
    if (!RunSetAlgorithm(script_state, backing_list, index, blink_value,
                         exception_state)) {
      return false;
    }

    if (index == backing_list.size())
      backing_list.push_back(std::move(blink_value));
    else
      backing_list[index] = std::move(blink_value);
    return true;
  }

  // https://webidl.spec.whatwg.org/#observable-array-attribute-set-an-indexed-value
  static bool RunSetAlgorithm(ScriptState* script_state,
                              BackingListWrappable& backing_list,
                              typename BackingListWrappable::size_type index,
                              typename BackingListWrappable::value_type& value,
                              ExceptionState& exception_state) {
    if (!backing_list.set_algorithm_callback_)
      return true;

    (backing_list.GetPlatformObject()->*backing_list.set_algorithm_callback_)(
        script_state, backing_list, index, value, exception_state);
    return !exception_state.HadException();
  }

  // https://webidl.spec.whatwg.org/#observable-array-attribute-delete-an-indexed-value
  static bool RunDeleteAlgorithm(ScriptState* script_state,
                                 BackingListWrappable& backing_list,
                                 typename BackingListWrappable::size_type index,
                                 ExceptionState& exception_state) {
    if (!backing_list.delete_algorithm_callback_)
      return true;

    (backing_list.GetPlatformObject()
         ->*backing_list.delete_algorithm_callback_)(script_state, backing_list,
                                                     index, exception_state);
    return !exception_state.HadException();
  }
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_EXOTIC_OBJECT_HANDLER_H_
