// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_BUILTIN_CALL_DESCRIPTORS_H_
#define V8_COMPILER_TURBOSHAFT_BUILTIN_CALL_DESCRIPTORS_H_

#include "src/builtins/builtins.h"
#include "src/codegen/callable.h"
#include "src/codegen/interface-descriptors.h"
#include "src/compiler/frame.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/write-barrier-kind.h"

namespace v8::internal::compiler::turboshaft {

struct BuiltinCallDescriptor {
 private:
  template <typename Derived>
  struct Descriptor {
    static const TSCallDescriptor* Create(Isolate* isolate, Zone* zone) {
      Callable callable = Builtins::CallableFor(isolate, Derived::Function);
      auto descriptor = Linkage::GetStubCallDescriptor(
          zone, callable.descriptor(),
          callable.descriptor().GetStackParameterCount(),
          Derived::NeedsFrameState ? CallDescriptor::kNeedsFrameState
                                   : CallDescriptor::kNoFlags,
          Derived::Properties);
#ifdef DEBUG
      Derived::Verify(descriptor);
#endif  // DEBUG
      return TSCallDescriptor::Create(descriptor, zone);
    }

#ifdef DEBUG
    static void Verify(const CallDescriptor* desc) {
      using result_t = typename Derived::result_t;
      using arguments_t = typename Derived::arguments_t;
      if constexpr (std::is_same_v<result_t, void>) {
        DCHECK_EQ(desc->ReturnCount(), 0);
      } else {
        DCHECK_EQ(desc->ReturnCount(), 1);
        DCHECK(result_t::allows_representation(
            RegisterRepresentation::FromMachineRepresentation(
                desc->GetReturnType(0).representation())));
      }
      DCHECK_EQ(desc->NeedsFrameState(), Derived::NeedsFrameState);
      DCHECK_EQ(desc->properties(), Derived::Properties);
      DCHECK_EQ(desc->ParameterCount(), std::tuple_size_v<arguments_t> +
                                            (Derived::NeedsContext ? 1 : 0));
      DCHECK(VerifyArguments<arguments_t>(desc));
    }

    template <typename Arguments>
    static bool VerifyArguments(const CallDescriptor* desc) {
      return VerifyArgumentsImpl<Arguments>(
          desc, std::make_index_sequence<std::tuple_size_v<Arguments>>());
    }

   private:
    template <typename Arguments, size_t... Indices>
    static bool VerifyArgumentsImpl(const CallDescriptor* desc,
                                    std::index_sequence<Indices...>) {
      return (std::tuple_element_t<Indices, Arguments>::allows_representation(
                  RegisterRepresentation::FromMachineRepresentation(
                      desc->GetParameterType(Indices).representation())) &&
              ...);
    }
#endif  // DEBUG
  };

  using Boolean = Oddball;

 public:
  struct CheckTurbofanType : public Descriptor<CheckTurbofanType> {
    static constexpr auto Function = Builtin::kCheckTurbofanType;
    using arguments_t = std::tuple<V<Object>, V<TurbofanType>, V<Smi>>;
    using result_t = V<Object>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = true;
    static constexpr Operator::Properties Properties =
        Operator::kNoThrow | Operator::kNoDeopt;
  };

  struct CopyFastSmiOrObjectElements
      : public Descriptor<CopyFastSmiOrObjectElements> {
    static constexpr auto Function = Builtin::kCopyFastSmiOrObjectElements;
    using arguments_t = std::tuple<V<Object>>;
    using result_t = V<Object>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = true;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  template <Builtin B>
  struct FindOrderedHashEntry : public Descriptor<FindOrderedHashEntry<B>> {
    static constexpr auto Function = B;
    using arguments_t = std::tuple<V<Object>, V<Smi>>;
    using result_t = V<Smi>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = true;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };
  using FindOrderedHashMapEntry =
      FindOrderedHashEntry<Builtin::kFindOrderedHashMapEntry>;
  using FindOrderedHashSetEntry =
      FindOrderedHashEntry<Builtin::kFindOrderedHashSetEntry>;

  template <Builtin B>
  struct GrowFastElements : public Descriptor<GrowFastElements<B>> {
    static constexpr auto Function = B;
    using arguments_t = std::tuple<V<Object>, V<Smi>>;
    using result_t = V<Object>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = true;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };
  using GrowFastDoubleElements =
      GrowFastElements<Builtin::kGrowFastDoubleElements>;
  using GrowFastSmiOrObjectElements =
      GrowFastElements<Builtin::kGrowFastSmiOrObjectElements>;

  template <Builtin B>
  struct NewArgumentsElements : public Descriptor<NewArgumentsElements<B>> {
    static constexpr auto Function = B;
    // TODO(nicohartmann@): First argument should be replaced by a proper
    // RawPtr.
    using arguments_t = std::tuple<V<WordPtr>, V<WordPtr>, V<Smi>>;
    using result_t = V<FixedArray>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };
  using NewSloppyArgumentsElements =
      NewArgumentsElements<Builtin::kNewSloppyArgumentsElements>;
  using NewStrictArgumentsElements =
      NewArgumentsElements<Builtin::kNewStrictArgumentsElements>;
  using NewRestArgumentsElements =
      NewArgumentsElements<Builtin::kNewRestArgumentsElements>;

  struct NumberToString : public Descriptor<NumberToString> {
    static constexpr auto Function = Builtin::kNumberToString;
    using arguments_t = std::tuple<V<Number>>;
    using result_t = V<String>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct PlainPrimitiveToNumber : public Descriptor<PlainPrimitiveToNumber> {
    static constexpr auto Function = Builtin::kPlainPrimitiveToNumber;
    using arguments_t = std::tuple<V<PlainPrimitive>>;
    using result_t = V<Number>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct SameValue : public Descriptor<SameValue> {
    static constexpr auto Function = Builtin::kSameValue;
    using arguments_t = std::tuple<V<Object>, V<Object>>;
    using result_t = V<Boolean>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct SameValueNumbersOnly : public Descriptor<SameValueNumbersOnly> {
    static constexpr auto Function = Builtin::kSameValueNumbersOnly;
    using arguments_t = std::tuple<V<Object>, V<Object>>;
    using result_t = V<Boolean>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct StringAdd_CheckNone : public Descriptor<StringAdd_CheckNone> {
    static constexpr auto Function = Builtin::kStringAdd_CheckNone;
    using arguments_t = std::tuple<V<String>, V<String>>;
    using result_t = V<String>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = true;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct StringEqual : public Descriptor<StringEqual> {
    static constexpr auto Function = Builtin::kStringEqual;
    using arguments_t = std::tuple<V<String>, V<String>, V<WordPtr>>;
    using result_t = V<Boolean>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct StringFromCodePointAt : public Descriptor<StringFromCodePointAt> {
    static constexpr auto Function = Builtin::kStringFromCodePointAt;
    using arguments_t = std::tuple<V<String>, V<WordPtr>>;
    using result_t = V<String>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct StringIndexOf : public Descriptor<StringIndexOf> {
    static constexpr auto Function = Builtin::kStringIndexOf;
    using arguments_t = std::tuple<V<String>, V<String>, V<Smi>>;
    using result_t = V<Smi>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  template <Builtin B>
  struct StringComparison : public Descriptor<StringComparison<B>> {
    static constexpr auto Function = B;
    using arguments_t = std::tuple<V<String>, V<String>>;
    using result_t = V<Boolean>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };
  using StringLessThan = StringComparison<Builtin::kStringLessThan>;
  using StringLessThanOrEqual =
      StringComparison<Builtin::kStringLessThanOrEqual>;

  struct StringSubstring : public Descriptor<StringSubstring> {
    static constexpr auto Function = Builtin::kStringSubstring;
    using arguments_t = std::tuple<V<String>, V<WordPtr>, V<WordPtr>>;
    using result_t = V<String>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

#ifdef V8_INTL_SUPPORT
  struct StringToLowerCaseIntl : public Descriptor<StringToLowerCaseIntl> {
    static constexpr auto Function = Builtin::kStringToLowerCaseIntl;
    using arguments_t = std::tuple<V<String>>;
    using result_t = V<String>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = true;
    static constexpr Operator::Properties Properties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };
#endif  // V8_INTL_SUPPORT

  struct StringToNumber : public Descriptor<StringToNumber> {
    static constexpr auto Function = Builtin::kStringToNumber;
    using arguments_t = std::tuple<V<String>>;
    using result_t = V<Number>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct ToBoolean : public Descriptor<ToBoolean> {
    static constexpr auto Function = Builtin::kToBoolean;
    using arguments_t = std::tuple<V<Object>>;
    using result_t = V<Boolean>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct ToObject : public Descriptor<ToObject> {
    static constexpr auto Function = Builtin::kToObject;
    using arguments_t = std::tuple<V<Object>>;
    using result_t = V<Object>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = true;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };

  struct Typeof : public Descriptor<Typeof> {
    static constexpr auto Function = Builtin::kTypeof;
    using arguments_t = std::tuple<V<Object>>;
    using result_t = V<String>;

    static constexpr bool NeedsFrameState = false;
    static constexpr bool NeedsContext = false;
    static constexpr Operator::Properties Properties = Operator::kEliminatable;
  };
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_BUILTIN_CALL_DESCRIPTORS_H_
