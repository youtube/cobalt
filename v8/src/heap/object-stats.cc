// Copyright 2015 the V8 project authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/object-stats.h"

#include <unordered_set>

#include "src/base/bits.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-state-inl.h"
#include "src/logging/counters.h"
#include "src/objects/compilation-cache-table-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/prototype-info.h"
#include "src/objects/slots.h"
#include "src/objects/templates.h"
#include "src/objects/visitors.h"
#include "src/utils/memcopy.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

static base::LazyMutex object_stats_mutex = LAZY_MUTEX_INITIALIZER;

class FieldStatsCollector : public ObjectVisitorWithCageBases {
 public:
  FieldStatsCollector(Heap* heap, size_t* tagged_fields_count,
                      size_t* embedder_fields_count,
                      size_t* inobject_smi_fields_count,
                      size_t* boxed_double_fields_count,
                      size_t* string_data_count, size_t* raw_fields_count)
      : ObjectVisitorWithCageBases(heap),
        tagged_fields_count_(tagged_fields_count),
        embedder_fields_count_(embedder_fields_count),
        inobject_smi_fields_count_(inobject_smi_fields_count),
        boxed_double_fields_count_(boxed_double_fields_count),
        string_data_count_(string_data_count),
        raw_fields_count_(raw_fields_count) {}

  void RecordStats(HeapObject host) {
    size_t old_pointer_fields_count = *tagged_fields_count_;
    host.Iterate(cage_base(), this);
    size_t tagged_fields_count_in_object =
        *tagged_fields_count_ - old_pointer_fields_count;

    int object_size_in_words = host.Size(cage_base()) / kTaggedSize;
    DCHECK_LE(tagged_fields_count_in_object, object_size_in_words);
    size_t raw_fields_count_in_object =
        object_size_in_words - tagged_fields_count_in_object;

    if (host.IsJSObject(cage_base())) {
      JSObjectFieldStats field_stats = GetInobjectFieldStats(host.map());
      // Embedder fields are already included into pointer words.
      DCHECK_LE(field_stats.embedded_fields_count_,
                tagged_fields_count_in_object);
      tagged_fields_count_in_object -= field_stats.embedded_fields_count_;
      *tagged_fields_count_ -= field_stats.embedded_fields_count_;
      *embedder_fields_count_ += field_stats.embedded_fields_count_;

      // Smi fields are also included into pointer words.
      tagged_fields_count_in_object -= field_stats.smi_fields_count_;
      *tagged_fields_count_ -= field_stats.smi_fields_count_;
      *inobject_smi_fields_count_ += field_stats.smi_fields_count_;
    } else if (host.IsHeapNumber(cage_base())) {
      DCHECK_LE(kDoubleSize / kTaggedSize, raw_fields_count_in_object);
      raw_fields_count_in_object -= kDoubleSize / kTaggedSize;
      *boxed_double_fields_count_ += 1;
    } else if (host.IsSeqString(cage_base())) {
      int string_data = SeqString::cast(host).length(kAcquireLoad) *
                        (String::cast(host).IsOneByteRepresentation() ? 1 : 2) /
                        kTaggedSize;
      DCHECK_LE(string_data, raw_fields_count_in_object);
      raw_fields_count_in_object -= string_data;
      *string_data_count_ += string_data;
    }
    *raw_fields_count_ += raw_fields_count_in_object;
  }

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    *tagged_fields_count_ += (end - start);
  }
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    *tagged_fields_count_ += (end - start);
  }

  V8_INLINE void VisitCodePointer(Code host, CodeObjectSlot slot) override {
    *tagged_fields_count_ += 1;
  }

  void VisitCodeTarget(InstructionStream host, RelocInfo* rinfo) override {
    // InstructionStream target is most likely encoded as a relative 32-bit
    // offset and not as a full tagged value, so there's nothing to count.
  }

  void VisitEmbeddedPointer(InstructionStream host, RelocInfo* rinfo) override {
    *tagged_fields_count_ += 1;
  }

  void VisitMapPointer(HeapObject host) override {
    // Just do nothing, but avoid the inherited UNREACHABLE implementation.
  }

 private:
  struct JSObjectFieldStats {
    JSObjectFieldStats() : embedded_fields_count_(0), smi_fields_count_(0) {}

    unsigned embedded_fields_count_ : kDescriptorIndexBitCount;
    unsigned smi_fields_count_ : kDescriptorIndexBitCount;
  };
  std::unordered_map<Map, JSObjectFieldStats, Object::Hasher>
      field_stats_cache_;

  JSObjectFieldStats GetInobjectFieldStats(Map map);

  size_t* const tagged_fields_count_;
  size_t* const embedder_fields_count_;
  size_t* const inobject_smi_fields_count_;
  size_t* const boxed_double_fields_count_;
  size_t* const string_data_count_;
  size_t* const raw_fields_count_;
};

FieldStatsCollector::JSObjectFieldStats
FieldStatsCollector::GetInobjectFieldStats(Map map) {
  auto iter = field_stats_cache_.find(map);
  if (iter != field_stats_cache_.end()) {
    return iter->second;
  }
  // Iterate descriptor array and calculate stats.
  JSObjectFieldStats stats;
  stats.embedded_fields_count_ = JSObject::GetEmbedderFieldCount(map);
  if (!map.is_dictionary_map()) {
    DescriptorArray descriptors = map.instance_descriptors();
    for (InternalIndex descriptor : map.IterateOwnDescriptors()) {
      PropertyDetails details = descriptors.GetDetails(descriptor);
      if (details.location() == PropertyLocation::kField) {
        FieldIndex index = FieldIndex::ForDetails(map, details);
        // Stop on first out-of-object field.
        if (!index.is_inobject()) break;
        if (details.representation().IsSmi()) {
          ++stats.smi_fields_count_;
        }
      }
    }
  }
  field_stats_cache_.insert(std::make_pair(map, stats));
  return stats;
}

void ObjectStats::ClearObjectStats(bool clear_last_time_stats) {
  memset(object_counts_, 0, sizeof(object_counts_));
  memset(object_sizes_, 0, sizeof(object_sizes_));
  memset(over_allocated_, 0, sizeof(over_allocated_));
  memset(size_histogram_, 0, sizeof(size_histogram_));
  memset(over_allocated_histogram_, 0, sizeof(over_allocated_histogram_));
  if (clear_last_time_stats) {
    memset(object_counts_last_time_, 0, sizeof(object_counts_last_time_));
    memset(object_sizes_last_time_, 0, sizeof(object_sizes_last_time_));
  }
  tagged_fields_count_ = 0;
  embedder_fields_count_ = 0;
  inobject_smi_fields_count_ = 0;
  boxed_double_fields_count_ = 0;
  string_data_count_ = 0;
  raw_fields_count_ = 0;
}

// Tell the compiler to never inline this: occasionally, the optimizer will
// decide to inline this and unroll the loop, making the compiled code more than
// 100KB larger.
V8_NOINLINE static void PrintJSONArray(size_t* array, const int len) {
  PrintF("[ ");
  for (int i = 0; i < len; i++) {
    PrintF("%zu", array[i]);
    if (i != (len - 1)) PrintF(", ");
  }
  PrintF(" ]");
}

V8_NOINLINE static void DumpJSONArray(std::stringstream& stream, size_t* array,
                                      const int len) {
  stream << PrintCollection(base::Vector<size_t>(array, len));
}

void ObjectStats::PrintKeyAndId(const char* key, int gc_count) {
  PrintF("\"isolate\": \"%p\", \"id\": %d, \"key\": \"%s\", ",
         reinterpret_cast<void*>(isolate()), gc_count, key);
}

void ObjectStats::PrintInstanceTypeJSON(const char* key, int gc_count,
                                        const char* name, int index) {
  PrintF("{ ");
  PrintKeyAndId(key, gc_count);
  PrintF("\"type\": \"instance_type_data\", ");
  PrintF("\"instance_type\": %d, ", index);
  PrintF("\"instance_type_name\": \"%s\", ", name);
  PrintF("\"overall\": %zu, ", object_sizes_[index]);
  PrintF("\"count\": %zu, ", object_counts_[index]);
  PrintF("\"over_allocated\": %zu, ", over_allocated_[index]);
  PrintF("\"histogram\": ");
  PrintJSONArray(size_histogram_[index], kNumberOfBuckets);
  PrintF(",");
  PrintF("\"over_allocated_histogram\": ");
  PrintJSONArray(over_allocated_histogram_[index], kNumberOfBuckets);
  PrintF(" }\n");
}

void ObjectStats::PrintJSON(const char* key) {
  double time = isolate()->time_millis_since_init();
  int gc_count = heap()->gc_count();

  // gc_descriptor
  PrintF("{ ");
  PrintKeyAndId(key, gc_count);
  PrintF("\"type\": \"gc_descriptor\", \"time\": %f }\n", time);
  // field_data
  PrintF("{ ");
  PrintKeyAndId(key, gc_count);
  PrintF("\"type\": \"field_data\"");
  PrintF(", \"tagged_fields\": %zu", tagged_fields_count_ * kTaggedSize);
  PrintF(", \"embedder_fields\": %zu",
         embedder_fields_count_ * kEmbedderDataSlotSize);
  PrintF(", \"inobject_smi_fields\": %zu",
         inobject_smi_fields_count_ * kTaggedSize);
  PrintF(", \"boxed_double_fields\": %zu",
         boxed_double_fields_count_ * kDoubleSize);
  PrintF(", \"string_data\": %zu", string_data_count_ * kTaggedSize);
  PrintF(", \"other_raw_fields\": %zu", raw_fields_count_ * kSystemPointerSize);
  PrintF(" }\n");
  // bucket_sizes
  PrintF("{ ");
  PrintKeyAndId(key, gc_count);
  PrintF("\"type\": \"bucket_sizes\", \"sizes\": [ ");
  for (int i = 0; i < kNumberOfBuckets; i++) {
    PrintF("%d", 1 << (kFirstBucketShift + i));
    if (i != (kNumberOfBuckets - 1)) PrintF(", ");
  }
  PrintF(" ] }\n");

#define INSTANCE_TYPE_WRAPPER(name) \
  PrintInstanceTypeJSON(key, gc_count, #name, name);

#define VIRTUAL_INSTANCE_TYPE_WRAPPER(name) \
  PrintInstanceTypeJSON(key, gc_count, #name, FIRST_VIRTUAL_TYPE + name);

  INSTANCE_TYPE_LIST(INSTANCE_TYPE_WRAPPER)
  VIRTUAL_INSTANCE_TYPE_LIST(VIRTUAL_INSTANCE_TYPE_WRAPPER)

#undef INSTANCE_TYPE_WRAPPER
#undef VIRTUAL_INSTANCE_TYPE_WRAPPER
}

void ObjectStats::DumpInstanceTypeData(std::stringstream& stream,
                                       const char* name, int index) {
  stream << "\"" << name << "\":{";
  stream << "\"type\":" << static_cast<int>(index) << ",";
  stream << "\"overall\":" << object_sizes_[index] << ",";
  stream << "\"count\":" << object_counts_[index] << ",";
  stream << "\"over_allocated\":" << over_allocated_[index] << ",";
  stream << "\"histogram\":";
  DumpJSONArray(stream, size_histogram_[index], kNumberOfBuckets);
  stream << ",\"over_allocated_histogram\":";
  DumpJSONArray(stream, over_allocated_histogram_[index], kNumberOfBuckets);
  stream << "},";
}

void ObjectStats::Dump(std::stringstream& stream) {
  double time = isolate()->time_millis_since_init();
  int gc_count = heap()->gc_count();

  stream << "{";
  stream << "\"isolate\":\"" << reinterpret_cast<void*>(isolate()) << "\",";
  stream << "\"id\":" << gc_count << ",";
  stream << "\"time\":" << time << ",";

  // field_data
  stream << "\"field_data\":{";
  stream << "\"tagged_fields\":" << (tagged_fields_count_ * kTaggedSize);
  stream << ",\"embedder_fields\":"
         << (embedder_fields_count_ * kEmbedderDataSlotSize);
  stream << ",\"inobject_smi_fields\": "
         << (inobject_smi_fields_count_ * kTaggedSize);
  stream << ",\"boxed_double_fields\": "
         << (boxed_double_fields_count_ * kDoubleSize);
  stream << ",\"string_data\": " << (string_data_count_ * kTaggedSize);
  stream << ",\"other_raw_fields\":"
         << (raw_fields_count_ * kSystemPointerSize);
  stream << "}, ";

  stream << "\"bucket_sizes\":[";
  for (int i = 0; i < kNumberOfBuckets; i++) {
    stream << (1 << (kFirstBucketShift + i));
    if (i != (kNumberOfBuckets - 1)) stream << ",";
  }
  stream << "],";
  stream << "\"type_data\":{";

#define INSTANCE_TYPE_WRAPPER(name) DumpInstanceTypeData(stream, #name, name);

#define VIRTUAL_INSTANCE_TYPE_WRAPPER(name) \
  DumpInstanceTypeData(stream, #name, FIRST_VIRTUAL_TYPE + name);

  INSTANCE_TYPE_LIST(INSTANCE_TYPE_WRAPPER);
  VIRTUAL_INSTANCE_TYPE_LIST(VIRTUAL_INSTANCE_TYPE_WRAPPER)
  stream << "\"END\":{}}}";

#undef INSTANCE_TYPE_WRAPPER
#undef VIRTUAL_INSTANCE_TYPE_WRAPPER
}

void ObjectStats::CheckpointObjectStats() {
  base::MutexGuard lock_guard(object_stats_mutex.Pointer());
  MemCopy(object_counts_last_time_, object_counts_, sizeof(object_counts_));
  MemCopy(object_sizes_last_time_, object_sizes_, sizeof(object_sizes_));
  ClearObjectStats();
}

namespace {

int Log2ForSize(size_t size) {
  DCHECK_GT(size, 0);
  return kSizetSize * 8 - 1 - base::bits::CountLeadingZeros(size);
}

}  // namespace

int ObjectStats::HistogramIndexFromSize(size_t size) {
  if (size == 0) return 0;
  return std::min({std::max(Log2ForSize(size) + 1 - kFirstBucketShift, 0),
                   kLastValueBucketIndex});
}

void ObjectStats::RecordObjectStats(InstanceType type, size_t size,
                                    size_t over_allocated) {
  DCHECK_LE(type, LAST_TYPE);
  object_counts_[type]++;
  object_sizes_[type] += size;
  size_histogram_[type][HistogramIndexFromSize(size)]++;
  over_allocated_[type] += over_allocated;
  over_allocated_histogram_[type][HistogramIndexFromSize(size)]++;
}

void ObjectStats::RecordVirtualObjectStats(VirtualInstanceType type,
                                           size_t size, size_t over_allocated) {
  DCHECK_LE(type, LAST_VIRTUAL_TYPE);
  object_counts_[FIRST_VIRTUAL_TYPE + type]++;
  object_sizes_[FIRST_VIRTUAL_TYPE + type] += size;
  size_histogram_[FIRST_VIRTUAL_TYPE + type][HistogramIndexFromSize(size)]++;
  over_allocated_[FIRST_VIRTUAL_TYPE + type] += over_allocated;
  over_allocated_histogram_[FIRST_VIRTUAL_TYPE + type]
                           [HistogramIndexFromSize(size)]++;
}

Isolate* ObjectStats::isolate() { return heap()->isolate(); }

class ObjectStatsCollectorImpl {
 public:
  enum Phase {
    kPhase1,
    kPhase2,
  };
  static const int kNumberOfPhases = kPhase2 + 1;

  ObjectStatsCollectorImpl(Heap* heap, ObjectStats* stats);

  void CollectGlobalStatistics();

  enum class CollectFieldStats { kNo, kYes };
  void CollectStatistics(HeapObject obj, Phase phase,
                         CollectFieldStats collect_field_stats);

 private:
  enum CowMode {
    kCheckCow,
    kIgnoreCow,
  };

  Isolate* isolate() { return heap_->isolate(); }

  bool RecordVirtualObjectStats(HeapObject parent, HeapObject obj,
                                ObjectStats::VirtualInstanceType type,
                                size_t size, size_t over_allocated,
                                CowMode check_cow_array = kCheckCow);
  void RecordExternalResourceStats(Address resource,
                                   ObjectStats::VirtualInstanceType type,
                                   size_t size);
  // Gets size from |ob| and assumes no over allocating.
  bool RecordSimpleVirtualObjectStats(HeapObject parent, HeapObject obj,
                                      ObjectStats::VirtualInstanceType type);
  // For HashTable it is possible to compute over allocated memory.
  template <typename Derived, typename Shape>
  void RecordHashTableVirtualObjectStats(HeapObject parent,
                                         HashTable<Derived, Shape> hash_table,
                                         ObjectStats::VirtualInstanceType type);

  bool SameLiveness(HeapObject obj1, HeapObject obj2);
  bool CanRecordFixedArray(FixedArrayBase array);
  bool IsCowArray(FixedArrayBase array);

  // Blocklist for objects that should not be recorded using
  // VirtualObjectStats and RecordSimpleVirtualObjectStats. For recording those
  // objects dispatch to the low level ObjectStats::RecordObjectStats manually.
  bool ShouldRecordObject(HeapObject object, CowMode check_cow_array);

  void RecordObjectStats(
      HeapObject obj, InstanceType type, size_t size,
      size_t over_allocated = ObjectStats::kNoOverAllocation);

  // Specific recursion into constant pool or embedded code objects. Records
  // FixedArrays and Tuple2.
  void RecordVirtualObjectsForConstantPoolOrEmbeddedObjects(
      HeapObject parent, HeapObject object,
      ObjectStats::VirtualInstanceType type);

  // Details.
  void RecordVirtualAllocationSiteDetails(AllocationSite site);
  void RecordVirtualBytecodeArrayDetails(BytecodeArray bytecode);
  void RecordVirtualCodeDetails(InstructionStream code);
  void RecordVirtualContext(Context context);
  void RecordVirtualFeedbackVectorDetails(FeedbackVector vector);
  void RecordVirtualFixedArrayDetails(FixedArray array);
  void RecordVirtualFunctionTemplateInfoDetails(FunctionTemplateInfo fti);
  void RecordVirtualJSGlobalObjectDetails(JSGlobalObject object);
  void RecordVirtualJSObjectDetails(JSObject object);
  void RecordVirtualMapDetails(Map map);
  void RecordVirtualScriptDetails(Script script);
  void RecordVirtualExternalStringDetails(ExternalString script);
  void RecordVirtualSharedFunctionInfoDetails(SharedFunctionInfo info);

  void RecordVirtualArrayBoilerplateDescription(
      ArrayBoilerplateDescription description);

  PtrComprCageBase cage_base() const {
    return field_stats_collector_.cage_base();
  }

  Heap* const heap_;
  ObjectStats* const stats_;
  NonAtomicMarkingState* const marking_state_;
  std::unordered_set<HeapObject, Object::Hasher, Object::KeyEqualSafe>
      virtual_objects_;
  std::unordered_set<Address> external_resources_;
  FieldStatsCollector field_stats_collector_;
};

ObjectStatsCollectorImpl::ObjectStatsCollectorImpl(Heap* heap,
                                                   ObjectStats* stats)
    : heap_(heap),
      stats_(stats),
      marking_state_(heap->non_atomic_marking_state()),
      field_stats_collector_(
          heap_, &stats->tagged_fields_count_, &stats->embedder_fields_count_,
          &stats->inobject_smi_fields_count_,
          &stats->boxed_double_fields_count_, &stats->string_data_count_,
          &stats->raw_fields_count_) {}

bool ObjectStatsCollectorImpl::ShouldRecordObject(HeapObject obj,
                                                  CowMode check_cow_array) {
  if (obj.IsFixedArrayExact()) {
    FixedArray fixed_array = FixedArray::cast(obj);
    bool cow_check = check_cow_array == kIgnoreCow || !IsCowArray(fixed_array);
    return CanRecordFixedArray(fixed_array) && cow_check;
  }
  if (obj.SafeEquals(ReadOnlyRoots(heap_).empty_property_array())) return false;
  return true;
}

template <typename Derived, typename Shape>
void ObjectStatsCollectorImpl::RecordHashTableVirtualObjectStats(
    HeapObject parent, HashTable<Derived, Shape> hash_table,
    ObjectStats::VirtualInstanceType type) {
  size_t over_allocated =
      (hash_table.Capacity() -
       (hash_table.NumberOfElements() + hash_table.NumberOfDeletedElements())) *
      HashTable<Derived, Shape>::kEntrySize * kTaggedSize;
  RecordVirtualObjectStats(parent, hash_table, type, hash_table.Size(),
                           over_allocated);
}

bool ObjectStatsCollectorImpl::RecordSimpleVirtualObjectStats(
    HeapObject parent, HeapObject obj, ObjectStats::VirtualInstanceType type) {
  return RecordVirtualObjectStats(parent, obj, type, obj.Size(cage_base()),
                                  ObjectStats::kNoOverAllocation, kCheckCow);
}

bool ObjectStatsCollectorImpl::RecordVirtualObjectStats(
    HeapObject parent, HeapObject obj, ObjectStats::VirtualInstanceType type,
    size_t size, size_t over_allocated, CowMode check_cow_array) {
  CHECK_LT(over_allocated, size);
  if (!SameLiveness(parent, obj) || !ShouldRecordObject(obj, check_cow_array)) {
    return false;
  }

  if (virtual_objects_.find(obj) == virtual_objects_.end()) {
    virtual_objects_.insert(obj);
    stats_->RecordVirtualObjectStats(type, size, over_allocated);
    return true;
  }
  return false;
}

void ObjectStatsCollectorImpl::RecordExternalResourceStats(
    Address resource, ObjectStats::VirtualInstanceType type, size_t size) {
  if (external_resources_.find(resource) == external_resources_.end()) {
    external_resources_.insert(resource);
    stats_->RecordVirtualObjectStats(type, size, 0);
  }
}

void ObjectStatsCollectorImpl::RecordVirtualAllocationSiteDetails(
    AllocationSite site) {
  if (!site.PointsToLiteral()) return;
  JSObject boilerplate = site.boilerplate();
  if (boilerplate.IsJSArray()) {
    RecordSimpleVirtualObjectStats(site, boilerplate,
                                   ObjectStats::JS_ARRAY_BOILERPLATE_TYPE);
    // Array boilerplates cannot have properties.
  } else {
    RecordVirtualObjectStats(
        site, boilerplate, ObjectStats::JS_OBJECT_BOILERPLATE_TYPE,
        boilerplate.Size(), ObjectStats::kNoOverAllocation);
    if (boilerplate.HasFastProperties()) {
      // We'll mis-classify the empty_property_array here. Given that there is a
      // single instance, this is negligible.
      PropertyArray properties = boilerplate.property_array();
      RecordSimpleVirtualObjectStats(
          site, properties, ObjectStats::BOILERPLATE_PROPERTY_ARRAY_TYPE);
    } else {
      NameDictionary properties = boilerplate.property_dictionary();
      RecordSimpleVirtualObjectStats(
          site, properties, ObjectStats::BOILERPLATE_PROPERTY_DICTIONARY_TYPE);
    }
  }
  FixedArrayBase elements = boilerplate.elements();
  RecordSimpleVirtualObjectStats(site, elements,
                                 ObjectStats::BOILERPLATE_ELEMENTS_TYPE);
}

void ObjectStatsCollectorImpl::RecordVirtualFunctionTemplateInfoDetails(
    FunctionTemplateInfo fti) {
  // named_property_handler and indexed_property_handler are recorded as
  // INTERCEPTOR_INFO_TYPE.
  HeapObject call_code = fti.call_code(kAcquireLoad);
  if (!call_code.IsUndefined(isolate())) {
    RecordSimpleVirtualObjectStats(
        fti, CallHandlerInfo::cast(call_code),
        ObjectStats::FUNCTION_TEMPLATE_INFO_ENTRIES_TYPE);
  }
  if (!fti.GetInstanceCallHandler().IsUndefined(isolate())) {
    RecordSimpleVirtualObjectStats(
        fti, CallHandlerInfo::cast(fti.GetInstanceCallHandler()),
        ObjectStats::FUNCTION_TEMPLATE_INFO_ENTRIES_TYPE);
  }
}

void ObjectStatsCollectorImpl::RecordVirtualJSGlobalObjectDetails(
    JSGlobalObject object) {
  // Properties.
  GlobalDictionary properties = object.global_dictionary(kAcquireLoad);
  RecordHashTableVirtualObjectStats(object, properties,
                                    ObjectStats::GLOBAL_PROPERTIES_TYPE);
  // Elements.
  FixedArrayBase elements = object.elements();
  RecordSimpleVirtualObjectStats(object, elements,
                                 ObjectStats::GLOBAL_ELEMENTS_TYPE);
}

void ObjectStatsCollectorImpl::RecordVirtualJSObjectDetails(JSObject object) {
  // JSGlobalObject is recorded separately.
  if (object.IsJSGlobalObject()) return;

  // Uncompiled JSFunction has a separate type.
  if (object.IsJSFunction() && !JSFunction::cast(object).is_compiled()) {
    RecordSimpleVirtualObjectStats(HeapObject(), object,
                                   ObjectStats::JS_UNCOMPILED_FUNCTION_TYPE);
  }

  // Properties.
  if (object.HasFastProperties()) {
    PropertyArray properties = object.property_array();
    if (properties != ReadOnlyRoots(heap_).empty_property_array()) {
      size_t over_allocated = object.map().UnusedPropertyFields() * kTaggedSize;
      RecordVirtualObjectStats(object, properties,
                               object.map().is_prototype_map()
                                   ? ObjectStats::PROTOTYPE_PROPERTY_ARRAY_TYPE
                                   : ObjectStats::OBJECT_PROPERTY_ARRAY_TYPE,
                               properties.Size(), over_allocated);
    }
  } else {
    NameDictionary properties = object.property_dictionary();
    RecordHashTableVirtualObjectStats(
        object, properties,
        object.map().is_prototype_map()
            ? ObjectStats::PROTOTYPE_PROPERTY_DICTIONARY_TYPE
            : ObjectStats::OBJECT_PROPERTY_DICTIONARY_TYPE);
  }

  // Elements.
  FixedArrayBase elements = object.elements();
  if (object.HasDictionaryElements()) {
    RecordHashTableVirtualObjectStats(
        object, NumberDictionary::cast(elements),
        object.IsJSArray() ? ObjectStats::ARRAY_DICTIONARY_ELEMENTS_TYPE
                           : ObjectStats::OBJECT_DICTIONARY_ELEMENTS_TYPE);
  } else if (object.IsJSArray()) {
    if (elements != ReadOnlyRoots(heap_).empty_fixed_array()) {
      size_t element_size =
          (elements.Size() - FixedArrayBase::kHeaderSize) / elements.length();
      uint32_t length = JSArray::cast(object).length().Number();
      size_t over_allocated = (elements.length() - length) * element_size;
      RecordVirtualObjectStats(object, elements,
                               ObjectStats::ARRAY_ELEMENTS_TYPE,
                               elements.Size(), over_allocated);
    }
  } else {
    RecordSimpleVirtualObjectStats(object, elements,
                                   ObjectStats::OBJECT_ELEMENTS_TYPE);
  }

  // JSCollections.
  if (object.IsJSCollection()) {
    Object maybe_table = JSCollection::cast(object).table();
    if (!maybe_table.IsUndefined(isolate())) {
      DCHECK(maybe_table.IsFixedArray(isolate()));
      // TODO(bmeurer): Properly compute over-allocation here.
      RecordSimpleVirtualObjectStats(object, HeapObject::cast(maybe_table),
                                     ObjectStats::JS_COLLECTION_TABLE_TYPE);
    }
  }
}

static ObjectStats::VirtualInstanceType GetFeedbackSlotType(
    MaybeObject maybe_obj, FeedbackSlotKind kind, Isolate* isolate) {
  if (maybe_obj->IsCleared())
    return ObjectStats::FEEDBACK_VECTOR_SLOT_OTHER_TYPE;
  Object obj = maybe_obj->GetHeapObjectOrSmi();
  switch (kind) {
    case FeedbackSlotKind::kCall:
      if (obj == *isolate->factory()->uninitialized_symbol()) {
        return ObjectStats::FEEDBACK_VECTOR_SLOT_CALL_UNUSED_TYPE;
      }
      return ObjectStats::FEEDBACK_VECTOR_SLOT_CALL_TYPE;

    case FeedbackSlotKind::kLoadProperty:
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
    case FeedbackSlotKind::kLoadKeyed:
    case FeedbackSlotKind::kHasKeyed:
      if (obj == *isolate->factory()->uninitialized_symbol()) {
        return ObjectStats::FEEDBACK_VECTOR_SLOT_LOAD_UNUSED_TYPE;
      }
      return ObjectStats::FEEDBACK_VECTOR_SLOT_LOAD_TYPE;

    case FeedbackSlotKind::kSetNamedSloppy:
    case FeedbackSlotKind::kSetNamedStrict:
    case FeedbackSlotKind::kDefineNamedOwn:
    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict:
    case FeedbackSlotKind::kSetKeyedSloppy:
    case FeedbackSlotKind::kSetKeyedStrict:
      if (obj == *isolate->factory()->uninitialized_symbol()) {
        return ObjectStats::FEEDBACK_VECTOR_SLOT_STORE_UNUSED_TYPE;
      }
      return ObjectStats::FEEDBACK_VECTOR_SLOT_STORE_TYPE;

    case FeedbackSlotKind::kBinaryOp:
    case FeedbackSlotKind::kCompareOp:
      return ObjectStats::FEEDBACK_VECTOR_SLOT_ENUM_TYPE;

    default:
      return ObjectStats::FEEDBACK_VECTOR_SLOT_OTHER_TYPE;
  }
}

void ObjectStatsCollectorImpl::RecordVirtualFeedbackVectorDetails(
    FeedbackVector vector) {
  if (virtual_objects_.find(vector) != virtual_objects_.end()) return;
  // Manually insert the feedback vector into the virtual object list, since
  // we're logging its component parts separately.
  virtual_objects_.insert(vector);

  size_t calculated_size = 0;

  // Log the feedback vector's header (fixed fields).
  size_t header_size = vector.slots_start().address() - vector.address();
  stats_->RecordVirtualObjectStats(ObjectStats::FEEDBACK_VECTOR_HEADER_TYPE,
                                   header_size, ObjectStats::kNoOverAllocation);
  calculated_size += header_size;

  // Iterate over the feedback slots and log each one.
  if (!vector.shared_function_info().HasFeedbackMetadata()) return;

  FeedbackMetadataIterator it(vector.metadata());
  while (it.HasNext()) {
    FeedbackSlot slot = it.Next();
    // Log the entry (or entries) taken up by this slot.
    size_t slot_size = it.entry_size() * kTaggedSize;
    stats_->RecordVirtualObjectStats(
        GetFeedbackSlotType(vector.Get(slot), it.kind(), heap_->isolate()),
        slot_size, ObjectStats::kNoOverAllocation);
    calculated_size += slot_size;

    // Log the monomorphic/polymorphic helper objects that this slot owns.
    for (int i = 0; i < it.entry_size(); i++) {
      MaybeObject raw_object = vector.Get(slot.WithOffset(i));
      HeapObject object;
      if (raw_object->GetHeapObject(&object)) {
        if (object.IsCell(cage_base()) ||
            object.IsWeakFixedArray(cage_base())) {
          RecordSimpleVirtualObjectStats(
              vector, object, ObjectStats::FEEDBACK_VECTOR_ENTRY_TYPE);
        }
      }
    }
  }

  CHECK_EQ(calculated_size, vector.Size());
}

void ObjectStatsCollectorImpl::RecordVirtualFixedArrayDetails(
    FixedArray array) {
  if (IsCowArray(array)) {
    RecordVirtualObjectStats(HeapObject(), array, ObjectStats::COW_ARRAY_TYPE,
                             array.Size(), ObjectStats::kNoOverAllocation,
                             kIgnoreCow);
  }
}

void ObjectStatsCollectorImpl::CollectStatistics(
    HeapObject obj, Phase phase, CollectFieldStats collect_field_stats) {
  DisallowGarbageCollection no_gc;
  Map map = obj.map(cage_base());
  InstanceType instance_type = map.instance_type();
  switch (phase) {
    case kPhase1:
      if (InstanceTypeChecker::IsFeedbackVector(instance_type)) {
        RecordVirtualFeedbackVectorDetails(FeedbackVector::cast(obj));
      } else if (InstanceTypeChecker::IsMap(instance_type)) {
        RecordVirtualMapDetails(Map::cast(obj));
      } else if (InstanceTypeChecker::IsBytecodeArray(instance_type)) {
        RecordVirtualBytecodeArrayDetails(BytecodeArray::cast(obj));
      } else if (InstanceTypeChecker::IsInstructionStream(instance_type)) {
        RecordVirtualCodeDetails(InstructionStream::cast(obj));
      } else if (InstanceTypeChecker::IsFunctionTemplateInfo(instance_type)) {
        RecordVirtualFunctionTemplateInfoDetails(
            FunctionTemplateInfo::cast(obj));
      } else if (InstanceTypeChecker::IsJSGlobalObject(instance_type)) {
        RecordVirtualJSGlobalObjectDetails(JSGlobalObject::cast(obj));
      } else if (InstanceTypeChecker::IsJSObject(instance_type)) {
        // This phase needs to come after RecordVirtualAllocationSiteDetails
        // to properly split among boilerplates.
        RecordVirtualJSObjectDetails(JSObject::cast(obj));
      } else if (InstanceTypeChecker::IsSharedFunctionInfo(instance_type)) {
        RecordVirtualSharedFunctionInfoDetails(SharedFunctionInfo::cast(obj));
      } else if (InstanceTypeChecker::IsContext(instance_type)) {
        RecordVirtualContext(Context::cast(obj));
      } else if (InstanceTypeChecker::IsScript(instance_type)) {
        RecordVirtualScriptDetails(Script::cast(obj));
      } else if (InstanceTypeChecker::IsArrayBoilerplateDescription(
                     instance_type)) {
        RecordVirtualArrayBoilerplateDescription(
            ArrayBoilerplateDescription::cast(obj));
      } else if (InstanceTypeChecker::IsFixedArrayExact(instance_type)) {
        // Has to go last as it triggers too eagerly.
        RecordVirtualFixedArrayDetails(FixedArray::cast(obj));
      }
      break;
    case kPhase2:
      if (InstanceTypeChecker::IsExternalString(instance_type)) {
        // This has to be in Phase2 to avoid conflicting with recording Script
        // sources. We still want to run RecordObjectStats after though.
        RecordVirtualExternalStringDetails(ExternalString::cast(obj));
      }
      size_t over_allocated = ObjectStats::kNoOverAllocation;
      if (InstanceTypeChecker::IsJSObject(instance_type)) {
        over_allocated = map.instance_size() - map.UsedInstanceSize();
      }
      RecordObjectStats(obj, instance_type, obj.Size(cage_base()),
                        over_allocated);
      if (collect_field_stats == CollectFieldStats::kYes) {
        field_stats_collector_.RecordStats(obj);
      }
      break;
  }
}

void ObjectStatsCollectorImpl::CollectGlobalStatistics() {
  // Iterate boilerplates first to disambiguate them from regular JS objects.
  Object list = heap_->allocation_sites_list();
  while (list.IsAllocationSite(cage_base())) {
    AllocationSite site = AllocationSite::cast(list);
    RecordVirtualAllocationSiteDetails(site);
    list = site.weak_next();
  }

  // FixedArray.
  RecordSimpleVirtualObjectStats(HeapObject(), heap_->serialized_objects(),
                                 ObjectStats::SERIALIZED_OBJECTS_TYPE);
  RecordSimpleVirtualObjectStats(HeapObject(), heap_->number_string_cache(),
                                 ObjectStats::NUMBER_STRING_CACHE_TYPE);
  RecordSimpleVirtualObjectStats(
      HeapObject(), heap_->single_character_string_table(),
      ObjectStats::SINGLE_CHARACTER_STRING_TABLE_TYPE);
  RecordSimpleVirtualObjectStats(HeapObject(), heap_->string_split_cache(),
                                 ObjectStats::STRING_SPLIT_CACHE_TYPE);
  RecordSimpleVirtualObjectStats(HeapObject(), heap_->regexp_multiple_cache(),
                                 ObjectStats::REGEXP_MULTIPLE_CACHE_TYPE);

  // WeakArrayList.
  RecordSimpleVirtualObjectStats(HeapObject(),
                                 WeakArrayList::cast(heap_->script_list()),
                                 ObjectStats::SCRIPT_LIST_TYPE);
}

void ObjectStatsCollectorImpl::RecordObjectStats(HeapObject obj,
                                                 InstanceType type, size_t size,
                                                 size_t over_allocated) {
  if (virtual_objects_.find(obj) == virtual_objects_.end()) {
    stats_->RecordObjectStats(type, size, over_allocated);
  }
}

bool ObjectStatsCollectorImpl::CanRecordFixedArray(FixedArrayBase array) {
  ReadOnlyRoots roots(heap_);
  return array != roots.empty_fixed_array() &&
         array != roots.empty_slow_element_dictionary() &&
         array != roots.empty_property_dictionary();
}

bool ObjectStatsCollectorImpl::IsCowArray(FixedArrayBase array) {
  return array.map(cage_base()) == ReadOnlyRoots(heap_).fixed_cow_array_map();
}

bool ObjectStatsCollectorImpl::SameLiveness(HeapObject obj1, HeapObject obj2) {
  if (obj1.is_null() || obj2.is_null()) return true;
  const auto obj1_marked =
      obj1.InReadOnlySpace() || marking_state_->IsMarked(obj1);
  const auto obj2_marked =
      obj2.InReadOnlySpace() || marking_state_->IsMarked(obj2);
  return obj1_marked == obj2_marked;
}

void ObjectStatsCollectorImpl::RecordVirtualMapDetails(Map map) {
  // TODO(mlippautz): map->dependent_code(): DEPENDENT_CODE_TYPE.

  // For Map we want to distinguish between various different states
  // to get a better picture of what's going on in MapSpace. This
  // method computes the virtual instance type to use for a given map,
  // using MAP_TYPE for regular maps that aren't special in any way.
  if (map.is_prototype_map()) {
    if (map.is_dictionary_map()) {
      RecordSimpleVirtualObjectStats(
          HeapObject(), map, ObjectStats::MAP_PROTOTYPE_DICTIONARY_TYPE);
    } else if (map.is_abandoned_prototype_map()) {
      RecordSimpleVirtualObjectStats(HeapObject(), map,
                                     ObjectStats::MAP_ABANDONED_PROTOTYPE_TYPE);
    } else {
      RecordSimpleVirtualObjectStats(HeapObject(), map,
                                     ObjectStats::MAP_PROTOTYPE_TYPE);
    }
  } else if (map.is_deprecated()) {
    RecordSimpleVirtualObjectStats(HeapObject(), map,
                                   ObjectStats::MAP_DEPRECATED_TYPE);
  } else if (map.is_dictionary_map()) {
    RecordSimpleVirtualObjectStats(HeapObject(), map,
                                   ObjectStats::MAP_DICTIONARY_TYPE);
  } else if (map.is_stable()) {
    RecordSimpleVirtualObjectStats(HeapObject(), map,
                                   ObjectStats::MAP_STABLE_TYPE);
  } else {
    // This will be logged as MAP_TYPE in Phase2.
  }

  DescriptorArray array = map.instance_descriptors(cage_base());
  if (map.owns_descriptors() &&
      array != ReadOnlyRoots(heap_).empty_descriptor_array()) {
    // Generally DescriptorArrays have their own instance type already
    // (DESCRIPTOR_ARRAY_TYPE), but we'd like to be able to tell which
    // of those are for (abandoned) prototypes, and which of those are
    // owned by deprecated maps.
    if (map.is_prototype_map()) {
      RecordSimpleVirtualObjectStats(
          map, array, ObjectStats::PROTOTYPE_DESCRIPTOR_ARRAY_TYPE);
    } else if (map.is_deprecated()) {
      RecordSimpleVirtualObjectStats(
          map, array, ObjectStats::DEPRECATED_DESCRIPTOR_ARRAY_TYPE);
    }

    EnumCache enum_cache = array.enum_cache();
    RecordSimpleVirtualObjectStats(array, enum_cache.keys(),
                                   ObjectStats::ENUM_KEYS_CACHE_TYPE);
    RecordSimpleVirtualObjectStats(array, enum_cache.indices(),
                                   ObjectStats::ENUM_INDICES_CACHE_TYPE);
  }

  if (map.is_prototype_map()) {
    PrototypeInfo prototype_info;
    if (map.TryGetPrototypeInfo(&prototype_info)) {
      Object users = prototype_info.prototype_users();
      if (users.IsWeakFixedArray(cage_base())) {
        RecordSimpleVirtualObjectStats(map, WeakArrayList::cast(users),
                                       ObjectStats::PROTOTYPE_USERS_TYPE);
      }
    }
  }
}

void ObjectStatsCollectorImpl::RecordVirtualScriptDetails(Script script) {
  RecordSimpleVirtualObjectStats(
      script, script.shared_function_infos(),
      ObjectStats::SCRIPT_SHARED_FUNCTION_INFOS_TYPE);

  // Log the size of external source code.
  Object raw_source = script.source();
  if (raw_source.IsExternalString(cage_base())) {
    // The contents of external strings aren't on the heap, so we have to record
    // them manually. The on-heap String object is recorded independently in
    // the normal pass.
    ExternalString string = ExternalString::cast(raw_source);
    Address resource = string.resource_as_address();
    size_t off_heap_size = string.ExternalPayloadSize();
    RecordExternalResourceStats(
        resource,
        string.IsOneByteRepresentation()
            ? ObjectStats::SCRIPT_SOURCE_EXTERNAL_ONE_BYTE_TYPE
            : ObjectStats::SCRIPT_SOURCE_EXTERNAL_TWO_BYTE_TYPE,
        off_heap_size);
  } else if (raw_source.IsString(cage_base())) {
    String source = String::cast(raw_source);
    RecordSimpleVirtualObjectStats(
        script, source,
        source.IsOneByteRepresentation()
            ? ObjectStats::SCRIPT_SOURCE_NON_EXTERNAL_ONE_BYTE_TYPE
            : ObjectStats::SCRIPT_SOURCE_NON_EXTERNAL_TWO_BYTE_TYPE);
  }
}

void ObjectStatsCollectorImpl::RecordVirtualExternalStringDetails(
    ExternalString string) {
  // Track the external string resource size in a separate category.

  Address resource = string.resource_as_address();
  size_t off_heap_size = string.ExternalPayloadSize();
  RecordExternalResourceStats(
      resource,
      string.IsOneByteRepresentation(cage_base())
          ? ObjectStats::STRING_EXTERNAL_RESOURCE_ONE_BYTE_TYPE
          : ObjectStats::STRING_EXTERNAL_RESOURCE_TWO_BYTE_TYPE,
      off_heap_size);
}

void ObjectStatsCollectorImpl::RecordVirtualSharedFunctionInfoDetails(
    SharedFunctionInfo info) {
  // Uncompiled SharedFunctionInfo gets its own category.
  if (!info.is_compiled()) {
    RecordSimpleVirtualObjectStats(
        HeapObject(), info, ObjectStats::UNCOMPILED_SHARED_FUNCTION_INFO_TYPE);
  }
}

void ObjectStatsCollectorImpl::RecordVirtualArrayBoilerplateDescription(
    ArrayBoilerplateDescription description) {
  RecordVirtualObjectsForConstantPoolOrEmbeddedObjects(
      description, description.constant_elements(),
      ObjectStats::ARRAY_BOILERPLATE_DESCRIPTION_ELEMENTS_TYPE);
}

void ObjectStatsCollectorImpl::
    RecordVirtualObjectsForConstantPoolOrEmbeddedObjects(
        HeapObject parent, HeapObject object,
        ObjectStats::VirtualInstanceType type) {
  if (!RecordSimpleVirtualObjectStats(parent, object, type)) return;
  if (object.IsFixedArrayExact(cage_base())) {
    FixedArray array = FixedArray::cast(object);
    for (int i = 0; i < array.length(); i++) {
      Object entry = array.get(i);
      if (!entry.IsHeapObject()) continue;
      RecordVirtualObjectsForConstantPoolOrEmbeddedObjects(
          array, HeapObject::cast(entry), type);
    }
  }
}

void ObjectStatsCollectorImpl::RecordVirtualBytecodeArrayDetails(
    BytecodeArray bytecode) {
  RecordSimpleVirtualObjectStats(
      bytecode, bytecode.constant_pool(),
      ObjectStats::BYTECODE_ARRAY_CONSTANT_POOL_TYPE);
  // FixedArrays on constant pool are used for holding descriptor information.
  // They are shared with optimized code.
  FixedArray constant_pool = FixedArray::cast(bytecode.constant_pool());
  for (int i = 0; i < constant_pool.length(); i++) {
    Object entry = constant_pool.get(i);
    if (entry.IsFixedArrayExact(cage_base())) {
      RecordVirtualObjectsForConstantPoolOrEmbeddedObjects(
          constant_pool, HeapObject::cast(entry),
          ObjectStats::EMBEDDED_OBJECT_TYPE);
    }
  }
  RecordSimpleVirtualObjectStats(
      bytecode, bytecode.handler_table(),
      ObjectStats::BYTECODE_ARRAY_HANDLER_TABLE_TYPE);
  if (bytecode.HasSourcePositionTable()) {
    RecordSimpleVirtualObjectStats(bytecode, bytecode.SourcePositionTable(),
                                   ObjectStats::SOURCE_POSITION_TABLE_TYPE);
  }
}

namespace {

ObjectStats::VirtualInstanceType CodeKindToVirtualInstanceType(CodeKind kind) {
  switch (kind) {
#define CODE_KIND_CASE(type) \
  case CodeKind::type:       \
    return ObjectStats::type;
    CODE_KIND_LIST(CODE_KIND_CASE)
#undef CODE_KIND_CASE
  }
  UNREACHABLE();
}

}  // namespace

void ObjectStatsCollectorImpl::RecordVirtualCodeDetails(
    InstructionStream istream) {
  Code code;
  if (!istream.TryGetCode(&code, kAcquireLoad)) return;
  RecordSimpleVirtualObjectStats(HeapObject(), istream,
                                 CodeKindToVirtualInstanceType(code.kind()));
  RecordSimpleVirtualObjectStats(istream, istream.relocation_info(),
                                 ObjectStats::RELOC_INFO_TYPE);
  if (CodeKindIsOptimizedJSFunction(code.kind())) {
    Object source_position_table = code.source_position_table();
    if (source_position_table.IsHeapObject()) {
      RecordSimpleVirtualObjectStats(istream,
                                     HeapObject::cast(source_position_table),
                                     ObjectStats::SOURCE_POSITION_TABLE_TYPE);
    }
    RecordSimpleVirtualObjectStats(istream, code.deoptimization_data(),
                                   ObjectStats::DEOPTIMIZATION_DATA_TYPE);
    DeoptimizationData input_data =
        DeoptimizationData::cast(code.deoptimization_data());
    if (input_data.length() > 0) {
      RecordSimpleVirtualObjectStats(code.deoptimization_data(),
                                     input_data.LiteralArray(),
                                     ObjectStats::OPTIMIZED_CODE_LITERALS_TYPE);
    }
  }
  int const mode_mask = RelocInfo::EmbeddedObjectModeMask();
  for (RelocIterator it(code, mode_mask); !it.done(); it.next()) {
    DCHECK(RelocInfo::IsEmbeddedObjectMode(it.rinfo()->rmode()));
    Object target = it.rinfo()->target_object(cage_base());
    if (target.IsFixedArrayExact(cage_base())) {
      RecordVirtualObjectsForConstantPoolOrEmbeddedObjects(
          istream, HeapObject::cast(target), ObjectStats::EMBEDDED_OBJECT_TYPE);
    }
  }
}

void ObjectStatsCollectorImpl::RecordVirtualContext(Context context) {
  if (context.IsNativeContext()) {
    RecordObjectStats(context, NATIVE_CONTEXT_TYPE, context.Size());
    if (context.retained_maps().IsWeakArrayList(cage_base())) {
      RecordSimpleVirtualObjectStats(
          context, WeakArrayList::cast(context.retained_maps()),
          ObjectStats::RETAINED_MAPS_TYPE);
    }

  } else if (context.IsFunctionContext()) {
    RecordObjectStats(context, FUNCTION_CONTEXT_TYPE, context.Size());
  } else {
    RecordSimpleVirtualObjectStats(HeapObject(), context,
                                   ObjectStats::OTHER_CONTEXT_TYPE);
  }
}

class ObjectStatsVisitor {
 public:
  ObjectStatsVisitor(Heap* heap, ObjectStatsCollectorImpl* live_collector,
                     ObjectStatsCollectorImpl* dead_collector,
                     ObjectStatsCollectorImpl::Phase phase)
      : live_collector_(live_collector),
        dead_collector_(dead_collector),
        marking_state_(heap->non_atomic_marking_state()),
        phase_(phase) {}

  void Visit(HeapObject obj) {
    if (obj.InReadOnlySpace() || marking_state_->IsMarked(obj)) {
      live_collector_->CollectStatistics(
          obj, phase_, ObjectStatsCollectorImpl::CollectFieldStats::kYes);
    } else {
      dead_collector_->CollectStatistics(
          obj, phase_, ObjectStatsCollectorImpl::CollectFieldStats::kNo);
    }
  }

 private:
  ObjectStatsCollectorImpl* const live_collector_;
  ObjectStatsCollectorImpl* const dead_collector_;
  NonAtomicMarkingState* const marking_state_;
  ObjectStatsCollectorImpl::Phase phase_;
};

namespace {

void IterateHeap(Heap* heap, ObjectStatsVisitor* visitor) {
  // We don't perform a GC while collecting object stats but need this scope for
  // the nested SafepointScope inside CombinedHeapObjectIterator.
  AllowGarbageCollection allow_gc;
  CombinedHeapObjectIterator iterator(heap);
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    visitor->Visit(obj);
  }
}

}  // namespace

void ObjectStatsCollector::Collect() {
  ObjectStatsCollectorImpl live_collector(heap_, live_);
  ObjectStatsCollectorImpl dead_collector(heap_, dead_);
  live_collector.CollectGlobalStatistics();
  for (int i = 0; i < ObjectStatsCollectorImpl::kNumberOfPhases; i++) {
    ObjectStatsVisitor visitor(heap_, &live_collector, &dead_collector,
                               static_cast<ObjectStatsCollectorImpl::Phase>(i));
    IterateHeap(heap_, &visitor);
  }
}

}  // namespace internal
}  // namespace v8
