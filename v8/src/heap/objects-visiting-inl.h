// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECTS_VISITING_INL_H_
#define V8_HEAP_OBJECTS_VISITING_INL_H_

#include "src/base/logging.h"
#include "src/heap/mark-compact.h"
#include "src/heap/object-lock.h"
#include "src/heap/objects-visiting.h"
#include "src/objects/arguments.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/synthetic-module-inl.h"
#include "src/objects/torque-defined-classes.h"
#include "src/objects/visitors.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

template <VisitorId visitor_id>
inline bool ContainsReadOnlyMap(PtrComprCageBase, HeapObject) {
  return false;
}

// List of visitor ids that can only appear in read-only maps. Unfortunately,
// these are generally contained in all other lists. Adding an instance type
// here optimizes handling in visitors that do not need to Map objects with such
// visitor ids.
#define READ_ONLY_MAPS_VISITOR_ID_LIST(V) \
  V(AccessorInfo)                         \
  V(AllocationSite)                       \
  V(BigInt)                               \
  V(BytecodeArray)                        \
  V(ByteArray)                            \
  V(CallHandlerInfo)                      \
  V(Cell)                                 \
  V(Code)                                 \
  V(DataHandler)                          \
  V(DataObject)                           \
  V(DescriptorArray)                      \
  V(EmbedderDataArray)                    \
  V(ExternalString)                       \
  V(FeedbackCell)                         \
  V(FeedbackMetadata)                     \
  V(FeedbackVector)                       \
  V(FixedArray)                           \
  V(FixedDoubleArray)                     \
  V(InstructionStream)                    \
  V(PreparseData)                         \
  V(PropertyArray)                        \
  V(PropertyCell)                         \
  V(PrototypeInfo)                        \
  V(ScopeInfo)                            \
  V(SeqOneByteString)                     \
  V(SeqTwoByteString)                     \
  V(SharedFunctionInfo)                   \
  V(ShortcutCandidate)                    \
  V(SlicedString)                         \
  V(SloppyArgumentsElements)              \
  V(Symbol)                               \
  V(ThinString)                           \
  V(TransitionArray)                      \
  V(UncompiledDataWithoutPreparseData)    \
  V(UncompiledDataWithPreparseData)       \
  V(WeakArrayList)                        \
  V(WeakFixedArray)

#define DEFINE_READ_ONLY_MAP_SPECIALIZATION(VisitorIdType)                    \
  template <>                                                                 \
  inline bool ContainsReadOnlyMap<VisitorId::kVisit##VisitorIdType>(          \
      PtrComprCageBase cage_base, HeapObject object) {                        \
    /* If you see this DCHECK fail we encountered a Map with a VisitorId that \
     * should have only ever appeared in read-only space. */                  \
    DCHECK(object.map(cage_base).InReadOnlySpace());                          \
    return true;                                                              \
  }
READ_ONLY_MAPS_VISITOR_ID_LIST(DEFINE_READ_ONLY_MAP_SPECIALIZATION)
#undef DEFINE_READ_ONLY_MAP_SPECIALIZATION
#undef READ_ONLY_MAPS_VISITOR_ID_LIST

template <typename ResultType, typename ConcreteVisitor>
HeapVisitor<ResultType, ConcreteVisitor>::HeapVisitor(
    PtrComprCageBase cage_base, PtrComprCageBase code_cage_base)
    : ObjectVisitorWithCageBases(cage_base, code_cage_base) {}

template <typename ResultType, typename ConcreteVisitor>
HeapVisitor<ResultType, ConcreteVisitor>::HeapVisitor(Isolate* isolate)
    : ObjectVisitorWithCageBases(isolate) {}

template <typename ResultType, typename ConcreteVisitor>
HeapVisitor<ResultType, ConcreteVisitor>::HeapVisitor(Heap* heap)
    : ObjectVisitorWithCageBases(heap) {}

template <typename ResultType, typename ConcreteVisitor>
template <typename T>
T HeapVisitor<ResultType, ConcreteVisitor>::Cast(HeapObject object) {
  return T::cast(object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::Visit(HeapObject object) {
  return Visit(object.map(cage_base()), object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::Visit(Map map,
                                                           HeapObject object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  switch (map.visitor_id()) {
#define CASE(TypeName)               \
  case kVisit##TypeName:             \
    return visitor->Visit##TypeName( \
        map, ConcreteVisitor::template Cast<TypeName>(object));
    TYPED_VISITOR_ID_LIST(CASE)
    TORQUE_VISITOR_ID_LIST(CASE)
#undef CASE
    case kVisitShortcutCandidate:
      return visitor->VisitShortcutCandidate(
          map, ConcreteVisitor::template Cast<ConsString>(object));
    case kVisitDataObject:
      return visitor->VisitDataObject(map, object);
    case kVisitJSObjectFast:
      return visitor->VisitJSObjectFast(
          map, ConcreteVisitor::template Cast<JSObject>(object));
    case kVisitJSApiObject:
      return visitor->VisitJSApiObject(
          map, ConcreteVisitor::template Cast<JSObject>(object));
    case kVisitStruct:
      return visitor->VisitStruct(map, object);
    case kVisitFreeSpace:
      return visitor->VisitFreeSpace(map, FreeSpace::cast(object));
    case kDataOnlyVisitorIdCount:
    case kVisitorIdCount:
      UNREACHABLE();
  }
  UNREACHABLE();
  // Make the compiler happy.
  return ResultType();
}

template <typename ResultType, typename ConcreteVisitor>
template <VisitorId visitor_id>
void HeapVisitor<ResultType, ConcreteVisitor>::VisitMapPointerIfNeeded(
    HeapObject host) {
  DCHECK(!host.map_word(cage_base(), kRelaxedLoad).IsForwardingAddress());
  if constexpr (!ConcreteVisitor::ShouldVisitMapPointer()) {
    return;
  }
  if constexpr (!ConcreteVisitor::ShouldVisitReadOnlyMapPointer()) {
    if (ContainsReadOnlyMap<visitor_id>(cage_base(), host)) {
      return;
    }
  }
  static_cast<ConcreteVisitor*>(this)->VisitMapPointer(host);
}

#define VISIT(TypeName)                                                      \
  template <typename ResultType, typename ConcreteVisitor>                   \
  ResultType HeapVisitor<ResultType, ConcreteVisitor>::Visit##TypeName(      \
      Map map, TypeName object) {                                            \
    ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);          \
    /* If you see the following DCHECK fail, then the size computation of    \
     * BodyDescriptor doesn't match the size return via obj.Size(). This is  \
     * problematic as the GC requires those sizes to match for accounting    \
     * reasons. The fix likely involves adding a padding field in the object \
     * defintions. */                                                        \
    DCHECK_EQ(object.SizeFromMap(map),                                       \
              TypeName::BodyDescriptor::SizeOf(map, object));                \
    visitor->template VisitMapPointerIfNeeded<VisitorId::kVisit##TypeName>(  \
        object);                                                             \
    const int size = TypeName::BodyDescriptor::SizeOf(map, object);          \
    TypeName::BodyDescriptor::IterateBody(map, object, size, visitor);       \
    return static_cast<ResultType>(size);                                    \
  }
TYPED_VISITOR_ID_LIST(VISIT)
TORQUE_VISITOR_ID_LIST(VISIT)
#undef VISIT

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitShortcutCandidate(
    Map map, ConsString object) {
  return static_cast<ConcreteVisitor*>(this)->VisitConsString(map, object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitDataObject(
    Map map, HeapObject object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = map.instance_size();
  visitor->template VisitMapPointerIfNeeded<VisitorId::kVisitDataObject>(
      object);
#ifdef V8_ENABLE_SANDBOX
  // The following types have external pointers, which must be visited.
  // TODO(v8:10391) Consider adding custom visitor IDs for these and making
  // this block not depend on V8_ENABLE_SANDBOX.
  if (object.IsForeign(cage_base())) {
    Foreign::BodyDescriptor::IterateBody(map, object, size, visitor);
  }
#endif  // V8_ENABLE_SANDBOX
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitJSObjectFast(
    Map map, JSObject object) {
  return VisitJSObjectSubclass<JSObject, JSObject::FastBodyDescriptor>(map,
                                                                       object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitJSApiObject(
    Map map, JSObject object) {
  return VisitJSObjectSubclass<JSObject, JSObject::BodyDescriptor>(map, object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitStruct(
    Map map, HeapObject object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = map.instance_size();
  visitor->template VisitMapPointerIfNeeded<VisitorId::kVisitStruct>(object);
  StructBodyDescriptor::IterateBody(map, object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitFreeSpace(
    Map map, FreeSpace object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  visitor->template VisitMapPointerIfNeeded<VisitorId::kVisitFreeSpace>(object);
  return static_cast<ResultType>(object.size(kRelaxedLoad));
}

template <typename ResultType, typename ConcreteVisitor>
template <typename T, typename TBodyDescriptor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitJSObjectSubclass(
    Map map, T object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  visitor->template VisitMapPointerIfNeeded<VisitorId::kVisitJSObject>(object);
  const int size = TBodyDescriptor::SizeOf(map, object);
  const int used_size = map.UsedInstanceSize();
  DCHECK_LE(used_size, size);
  DCHECK_GE(used_size, JSObject::GetHeaderSize(map));
  // It is important to visit only the used field and ignore the slack fields
  // because the slack fields may be trimmed concurrently. For non-concurrent
  // visitors this merely is an optimization in that we only visit the actually
  // used fields.
  TBodyDescriptor::IterateBody(map, object, used_size, visitor);
  return size;
}

template <typename ResultType, typename ConcreteVisitor>
ConcurrentHeapVisitor<ResultType, ConcreteVisitor>::ConcurrentHeapVisitor(
    Isolate* isolate)
    : HeapVisitor<ResultType, ConcreteVisitor>(isolate) {}

template <typename T>
struct ConcurrentVisitorCastHelper {
  static V8_INLINE T Cast(HeapObject object) { return T::cast(object); }
};

#define UNCHECKED_CAST(VisitorId, TypeName)                       \
  template <>                                                     \
  V8_INLINE TypeName ConcurrentVisitorCastHelper<TypeName>::Cast( \
      HeapObject object) {                                        \
    return TypeName::unchecked_cast(object);                      \
  }
SAFE_STRING_TRANSITION_SOURCES(UNCHECKED_CAST)
// Casts are also needed for unsafe ones for the initial dispatch in
// HeapVisitor.
UNSAFE_STRING_TRANSITION_SOURCES(UNCHECKED_CAST)
#undef UNCHECKED_CAST

template <typename ResultType, typename ConcreteVisitor>
template <typename T>
T ConcurrentHeapVisitor<ResultType, ConcreteVisitor>::Cast(HeapObject object) {
  if constexpr (ConcreteVisitor::EnableConcurrentVisitation()) {
    return ConcurrentVisitorCastHelper<T>::Cast(object);
  }
  return T::cast(object);
}

#define VISIT_AS_LOCKED_STRING(VisitorId, TypeName)                           \
  template <typename ResultType, typename ConcreteVisitor>                    \
  ResultType                                                                  \
      ConcurrentHeapVisitor<ResultType, ConcreteVisitor>::Visit##TypeName(    \
          Map map, TypeName object) {                                         \
    if constexpr (ConcreteVisitor::EnableConcurrentVisitation()) {            \
      return VisitStringLocked(object);                                       \
    }                                                                         \
    return HeapVisitor<ResultType, ConcreteVisitor>::Visit##TypeName(map,     \
                                                                     object); \
  }

UNSAFE_STRING_TRANSITION_SOURCES(VISIT_AS_LOCKED_STRING)
#undef VISIT_AS_LOCKED_STRING

template <typename ResultType, typename ConcreteVisitor>
template <typename T>
ResultType ConcurrentHeapVisitor<ResultType,
                                 ConcreteVisitor>::VisitStringLocked(T object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  SharedObjectLockGuard guard(object);
  // The object has been locked. At this point shared read access is
  // guaranteed but we must re-read the map and check whether the string has
  // transitioned.
  Map map = object.map(visitor->cage_base());
  int size;
  switch (map.visitor_id()) {
#define UNSAFE_STRING_TRANSITION_TARGET_CASE(VisitorIdType, TypeName)         \
  case kVisit##VisitorIdType:                                                 \
    visitor                                                                   \
        ->template VisitMapPointerIfNeeded<VisitorId::kVisit##VisitorIdType>( \
            object);                                                          \
    size = TypeName::BodyDescriptor::SizeOf(map, object);                     \
    TypeName::BodyDescriptor::IterateBody(                                    \
        map, TypeName::unchecked_cast(object), size, visitor);                \
    break;

    UNSAFE_STRING_TRANSITION_TARGETS(UNSAFE_STRING_TRANSITION_TARGET_CASE)
#undef UNSAFE_STRING_TRANSITION_TARGET_CASE
    default:
      UNREACHABLE();
  }
  return static_cast<ResultType>(size);
  ;
}

template <typename ConcreteVisitor>
NewSpaceVisitor<ConcreteVisitor>::NewSpaceVisitor(Isolate* isolate)
    : ConcurrentHeapVisitor<int, ConcreteVisitor>(isolate) {}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECTS_VISITING_INL_H_
