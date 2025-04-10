// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_COMPRESSED_SLOTS_INL_H_
#define V8_OBJECTS_COMPRESSED_SLOTS_INL_H_

#ifdef V8_COMPRESS_POINTERS

#include "src/common/ptr-compr-inl.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/maybe-object-inl.h"

namespace v8::internal {

//
// CompressedObjectSlot implementation.
//

CompressedObjectSlot::CompressedObjectSlot(Object* object)
    : SlotBase(reinterpret_cast<Address>(&object->ptr_)) {}

bool CompressedObjectSlot::contains_value(Address raw_value) const {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location());
  return static_cast<uint32_t>(value) ==
         static_cast<uint32_t>(static_cast<Tagged_t>(raw_value));
}

bool CompressedObjectSlot::contains_map_value(Address raw_value) const {
  // Simply forward to contains_value because map packing is not supported with
  // pointer compression.
  DCHECK(!V8_MAP_PACKING_BOOL);
  return contains_value(raw_value);
}

Object CompressedObjectSlot::operator*() const {
  Tagged_t value = *location();
  return Object(TCompressionScheme::DecompressTagged(address(), value));
}

Object CompressedObjectSlot::load(PtrComprCageBase cage_base) const {
  Tagged_t value = *location();
  return Object(TCompressionScheme::DecompressTagged(cage_base, value));
}

void CompressedObjectSlot::store(Object value) const {
  *location() = TCompressionScheme::CompressObject(value.ptr());
}

void CompressedObjectSlot::store_map(Map map) const {
  // Simply forward to store because map packing is not supported with pointer
  // compression.
  DCHECK(!V8_MAP_PACKING_BOOL);
  store(map);
}

Map CompressedObjectSlot::load_map() const {
  // Simply forward to Relaxed_Load because map packing is not supported with
  // pointer compression.
  DCHECK(!V8_MAP_PACKING_BOOL);
  return Map::unchecked_cast(Relaxed_Load());
}

Object CompressedObjectSlot::Acquire_Load() const {
  AtomicTagged_t value = AsAtomicTagged::Acquire_Load(location());
  return Object(TCompressionScheme::DecompressTagged(address(), value));
}

Object CompressedObjectSlot::Relaxed_Load() const {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location());
  return Object(TCompressionScheme::DecompressTagged(address(), value));
}

Object CompressedObjectSlot::Relaxed_Load(PtrComprCageBase cage_base) const {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location());
  return Object(TCompressionScheme::DecompressTagged(cage_base, value));
}

void CompressedObjectSlot::Relaxed_Store(Object value) const {
  Tagged_t ptr = TCompressionScheme::CompressObject(value.ptr());
  AsAtomicTagged::Relaxed_Store(location(), ptr);
}

void CompressedObjectSlot::Release_Store(Object value) const {
  Tagged_t ptr = TCompressionScheme::CompressObject(value.ptr());
  AsAtomicTagged::Release_Store(location(), ptr);
}

Object CompressedObjectSlot::Release_CompareAndSwap(Object old,
                                                    Object target) const {
  Tagged_t old_ptr = TCompressionScheme::CompressObject(old.ptr());
  Tagged_t target_ptr = TCompressionScheme::CompressObject(target.ptr());
  Tagged_t result =
      AsAtomicTagged::Release_CompareAndSwap(location(), old_ptr, target_ptr);
  return Object(TCompressionScheme::DecompressTagged(address(), result));
}

//
// CompressedMaybeObjectSlot implementation.
//

MaybeObject CompressedMaybeObjectSlot::operator*() const {
  Tagged_t value = *location();
  return MaybeObject(TCompressionScheme::DecompressTagged(address(), value));
}

MaybeObject CompressedMaybeObjectSlot::load(PtrComprCageBase cage_base) const {
  Tagged_t value = *location();
  return MaybeObject(TCompressionScheme::DecompressTagged(cage_base, value));
}

void CompressedMaybeObjectSlot::store(MaybeObject value) const {
  *location() = TCompressionScheme::CompressAny(value.ptr());
}

MaybeObject CompressedMaybeObjectSlot::Relaxed_Load() const {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location());
  return MaybeObject(TCompressionScheme::DecompressTagged(address(), value));
}

MaybeObject CompressedMaybeObjectSlot::Relaxed_Load(
    PtrComprCageBase cage_base) const {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location());
  return MaybeObject(TCompressionScheme::DecompressTagged(cage_base, value));
}

void CompressedMaybeObjectSlot::Relaxed_Store(MaybeObject value) const {
  Tagged_t ptr = TCompressionScheme::CompressAny(value.ptr());
  AsAtomicTagged::Relaxed_Store(location(), ptr);
}

void CompressedMaybeObjectSlot::Release_CompareAndSwap(
    MaybeObject old, MaybeObject target) const {
  Tagged_t old_ptr = TCompressionScheme::CompressAny(old.ptr());
  Tagged_t target_ptr = TCompressionScheme::CompressAny(target.ptr());
  AsAtomicTagged::Release_CompareAndSwap(location(), old_ptr, target_ptr);
}

//
// CompressedHeapObjectSlot implementation.
//

HeapObjectReference CompressedHeapObjectSlot::operator*() const {
  Tagged_t value = *location();
  return HeapObjectReference(
      TCompressionScheme::DecompressTagged(address(), value));
}

HeapObjectReference CompressedHeapObjectSlot::load(
    PtrComprCageBase cage_base) const {
  Tagged_t value = *location();
  return HeapObjectReference(
      TCompressionScheme::DecompressTagged(cage_base, value));
}

void CompressedHeapObjectSlot::store(HeapObjectReference value) const {
  *location() = TCompressionScheme::CompressObject(value.ptr());
}

HeapObject CompressedHeapObjectSlot::ToHeapObject() const {
  Tagged_t value = *location();
  DCHECK(HAS_STRONG_HEAP_OBJECT_TAG(value));
  return HeapObject::cast(
      Object(TCompressionScheme::DecompressTagged(address(), value)));
}

void CompressedHeapObjectSlot::StoreHeapObject(HeapObject value) const {
  *location() = TCompressionScheme::CompressObject(value.ptr());
}

//
// OffHeapCompressedObjectSlot implementation.
//

template <typename CompressionScheme>
Object OffHeapCompressedObjectSlot<CompressionScheme>::load(
    PtrComprCageBase cage_base) const {
  Tagged_t value = *TSlotBase::location();
  return Object(CompressionScheme::DecompressTagged(cage_base, value));
}

template <typename CompressionScheme>
void OffHeapCompressedObjectSlot<CompressionScheme>::store(Object value) const {
  *TSlotBase::location() = CompressionScheme::CompressObject(value.ptr());
}

template <typename CompressionScheme>
Object OffHeapCompressedObjectSlot<CompressionScheme>::Relaxed_Load(
    PtrComprCageBase cage_base) const {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(TSlotBase::location());
  return Object(CompressionScheme::DecompressTagged(cage_base, value));
}

template <typename CompressionScheme>
Object OffHeapCompressedObjectSlot<CompressionScheme>::Acquire_Load(
    PtrComprCageBase cage_base) const {
  AtomicTagged_t value = AsAtomicTagged::Acquire_Load(TSlotBase::location());
  return Object(CompressionScheme::DecompressTagged(cage_base, value));
}

template <typename CompressionScheme>
void OffHeapCompressedObjectSlot<CompressionScheme>::Relaxed_Store(
    Object value) const {
  Tagged_t ptr = CompressionScheme::CompressObject(value.ptr());
  AsAtomicTagged::Relaxed_Store(TSlotBase::location(), ptr);
}

template <typename CompressionScheme>
void OffHeapCompressedObjectSlot<CompressionScheme>::Release_Store(
    Object value) const {
  Tagged_t ptr = CompressionScheme::CompressObject(value.ptr());
  AsAtomicTagged::Release_Store(TSlotBase::location(), ptr);
}

template <typename CompressionScheme>
void OffHeapCompressedObjectSlot<CompressionScheme>::Release_CompareAndSwap(
    Object old, Object target) const {
  Tagged_t old_ptr = CompressionScheme::CompressObject(old.ptr());
  Tagged_t target_ptr = CompressionScheme::CompressObject(target.ptr());
  AsAtomicTagged::Release_CompareAndSwap(TSlotBase::location(), old_ptr,
                                         target_ptr);
}

}  // namespace v8::internal

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_OBJECTS_COMPRESSED_SLOTS_INL_H_
