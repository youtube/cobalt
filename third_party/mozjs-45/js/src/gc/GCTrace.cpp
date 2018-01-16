/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(COBALT) && defined(COBALT_JS_GC_TRACE)

#include "gc/GCTrace.h"
#include "jscntxt.h"
#include "jsfriendapi.h"
#include "vm/Runtime.h"

#include "nb/thread_local_object.h"
#include "starboard/log.h"

#include <unordered_map>

namespace js {
namespace gc {

namespace {

// A simple struct to bundle up both when and where a |GCThing| was allocated.
struct AllocationInfo {
  std::string stack;
  SbTimeMonotonic time;
};

// A simple struct to bundle up all information relevant to the current thread
// that we are tracing allocations for, which includes a mapping from |Cell|s
// to their corresponding |AllocationInfo|, as well as the last time that we
// dumped |thing2info|.
struct TraceData {
  std::unordered_map<Cell*, AllocationInfo> thing2info;
  SbTimeMonotonic last_dump_time;
};
nb::ThreadLocalObject<TraceData> tlo_trace_data;

const SbTimeMonotonic kDumpInterval = 15 * 60 * 1000 * 1000;
const int kStackSize = 10;

// Return the top first |n| stack frames of |cx| as a single string.  |cx|
// must not be NULL.
std::string DumpBacktrace(JSContext* cx, int n) {
  SB_DCHECK(cx != NULL);

  Sprinter sprinter(cx);
  sprinter.init();
  size_t depth = 0;

  for (AllFramesIter it(cx); !it.done(); ++it, ++depth) {
    if (depth > n) {
      break;
    }

    const char* filename = JS_GetScriptFilename(it.script());
    unsigned column = 0;
    unsigned line = PCToLineNumber(it.script(), it.pc(), &column);

    if (depth != 0) {
      sprinter.printf(" ");
    }

    sprinter.printf("#%d %s:%d:%d", depth, filename, line, column);
  }

  return std::string(sprinter.string(),
                     sprinter.string() + sprinter.getOffset());
}

// Add |thing| and its corresponding |AllocationInfo| to |thing2info|.
void StartTrackingThing(Cell* thing) {
  if (!thing) {
    return;
  }

  auto* rt = thing->runtimeFromMainThread();
  auto* cx = rt->contextList.getFirst();
  // JavaScript allocations can still happen during the brief period of time
  // in between when we destroy the sole |JSContext| and then finally destroy
  // the entire |JSRuntime|.  They should be counted as having had no stack.
  auto stack = cx ? DumpBacktrace(cx, kStackSize) : "";
  auto& thing2info = tlo_trace_data.GetOrCreate()->thing2info;
  thing2info[thing] = {stack, SbTimeGetMonotonicNow()};
}

// Remove |thing| from |tlo_thing2info|.  |tlo_thing2info| is expected to
// actually contain |thing|, and will error if it does not.
void StopTrackingThing(Cell* thing) {
  if (!thing) {
    return;
  }

  auto& thing2info = tlo_trace_data.GetOrCreate()->thing2info;
  auto erased_count = thing2info.erase(thing);
  SB_DCHECK(erased_count == 1);
}

// Dump the current state of the heap if |kDumpInterval| time has passed since
// the last dump.
void MaybeDump() {
  auto& trace_data = *tlo_trace_data.GetOrCreate();
  auto now = SbTimeGetMonotonicNow();
  auto diff = now - trace_data.last_dump_time;
  if (diff > kDumpInterval) {
    trace_data.last_dump_time = now;
    auto thread_id = SbThreadGetId();
    SB_LOG(INFO) << "CJGT_BEGIN_HEAP_DUMP: " << thread_id << " " << now;
    for (const auto& pair : trace_data.thing2info) {
      SB_LOG(INFO) << "CJGT_HEAP_DUMP: " << thread_id << " "
                   << static_cast<void*>(pair.first) << " " << pair.second.time
                   << " " << pair.second.stack;
    }
    SB_LOG(INFO) << "CJGT_END_HEAP_DUMP: " << thread_id << " " << now;
  }
}

std::string GetThreadName() {
  char buffer[120];
  SbThreadGetName(buffer, SB_ARRAY_SIZE_INT(buffer));
  return &buffer[0];
}

}  // namespace

bool InitTrace(GCRuntime& gc) {
  SB_LOG(INFO) << "CJGT_THREAD_ID_TO_NAME: " << SbThreadGetId() << " "
               << GetThreadName();
  return true;
}

void FinishTrace() {}

bool TraceEnabled() {
  return true;
}

void TraceNurseryAlloc(Cell* thing, size_t size) {
  // Don't do anything.  We will trace |thing| if it ends up getting
  // tenured.
}

void TraceTenuredAlloc(Cell* thing, AllocKind kind) {
  StartTrackingThing(thing);
}

void TraceCreateObject(JSObject* object) {}

void TraceMinorGCStart() {}

void TracePromoteToTenured(Cell* src, Cell* dst) {
  StartTrackingThing(dst);
}

void TraceMinorGCEnd() {
  MaybeDump();
}

void TraceMajorGCStart() {}

void TraceTenuredFinalize(Cell* thing) {
  StopTrackingThing(thing);
}

void TraceMajorGCEnd() {
  MaybeDump();
}

void TraceTypeNewScript(js::ObjectGroup* group) {}

}  // namespace gc
}  // namespace js

#elif defined(JS_GC_TRACE)

#include "gc/GCTrace.h"

#include <stdio.h>
#include <string.h>

#include "gc/GCTraceFormat.h"

#include "js/HashTable.h"

using namespace js;
using namespace js::gc;

JS_STATIC_ASSERT(AllocKinds == unsigned(AllocKind::LIMIT));
JS_STATIC_ASSERT(LastObjectAllocKind == unsigned(AllocKind::OBJECT_LAST));

static FILE* gcTraceFile = nullptr;

static HashSet<const Class*, DefaultHasher<const Class*>, SystemAllocPolicy> tracedClasses;
static HashSet<const ObjectGroup*, DefaultHasher<const ObjectGroup*>, SystemAllocPolicy> tracedGroups;

static inline void
WriteWord(uint64_t data)
{
    if (gcTraceFile)
        fwrite(&data, sizeof(data), 1, gcTraceFile);
}

static inline void
TraceEvent(GCTraceEvent event, uint64_t payload = 0, uint8_t extra = 0)
{
    MOZ_ASSERT(event < GCTraceEventCount);
    MOZ_ASSERT((payload >> TracePayloadBits) == 0);
    WriteWord((uint64_t(event) << TraceEventShift) |
               (uint64_t(extra) << TraceExtraShift) | payload);
}

static inline void
TraceAddress(const void* p)
{
    TraceEvent(TraceDataAddress, uint64_t(p));
}

static inline void
TraceInt(uint32_t data)
{
    TraceEvent(TraceDataInt, data);
}

static void
TraceString(const char* string)
{
    JS_STATIC_ASSERT(sizeof(char) == 1);

    size_t length = strlen(string);
    const unsigned charsPerWord = sizeof(uint64_t);
    unsigned wordCount = (length + charsPerWord - 1) / charsPerWord;

    TraceEvent(TraceDataString, length);
    for (unsigned i = 0; i < wordCount; ++i) {
        union
        {
            uint64_t word;
            char chars[charsPerWord];
        } data;
        strncpy(data.chars, string + (i * charsPerWord), charsPerWord);
        WriteWord(data.word);
    }
}

bool
js::gc::InitTrace(GCRuntime& gc)
{
    /* This currently does not support multiple runtimes. */
    MOZ_ALWAYS_TRUE(!gcTraceFile);

    char* filename = js_sb_getenv("JS_GC_TRACE");
    if (!filename)
        return true;

    if (!tracedClasses.init() || !tracedTypes.init()) {
        FinishTrace();
        return false;
    }

    gcTraceFile = fopen(filename, "w");
    if (!gcTraceFile) {
        FinishTrace();
        return false;
    }

    TraceEvent(TraceEventInit, 0, TraceFormatVersion);

    /* Trace information about thing sizes. */
    for (auto kind : AllAllocKinds())
        TraceEvent(TraceEventThingSize, Arena::thingSize(kind));

    return true;
}

void
js::gc::FinishTrace()
{
    if (gcTraceFile) {
        fclose(gcTraceFile);
        gcTraceFile = nullptr;
    }
    tracedClasses.finish();
    tracedTypes.finish();
}

bool
js::gc::TraceEnabled()
{
    return gcTraceFile != nullptr;
}

void
js::gc::TraceNurseryAlloc(Cell* thing, size_t size)
{
    if (thing) {
        /* We don't have AllocKind here, but we can work it out from size. */
        unsigned slots = (size - sizeof(JSObject)) / sizeof(JS::Value);
        AllocKind kind = GetBackgroundAllocKind(GetGCObjectKind(slots));
        TraceEvent(TraceEventNurseryAlloc, uint64_t(thing), kind);
    }
}

void
js::gc::TraceTenuredAlloc(Cell* thing, AllocKind kind)
{
    if (thing)
        TraceEvent(TraceEventTenuredAlloc, uint64_t(thing), kind);
}

static void
MaybeTraceClass(const Class* clasp)
{
    if (tracedClasses.has(clasp))
        return;

    TraceEvent(TraceEventClassInfo, uint64_t(clasp));
    TraceString(clasp->name);
    TraceInt(clasp->flags);
    TraceInt(clasp->finalize != nullptr);

    MOZ_ALWAYS_TRUE(tracedClasses.put(clasp));
}

static void
MaybeTraceGroup(ObjectGroup* group)
{
    if (tracedGroups.has(group))
        return;

    MaybeTraceClass(group->clasp());
    TraceEvent(TraceEventGroupInfo, uint64_t(group));
    TraceAddress(group->clasp());
    TraceInt(group->flags());

    MOZ_ALWAYS_TRUE(tracedGroups.put(group));
}

void
js::gc::TraceTypeNewScript(ObjectGroup* group)
{
    const size_t bufLength = 128;
    static char buffer[bufLength];
    MOZ_ASSERT(group->hasNewScript());
    JSAtom* funName = group->newScript()->fun->displayAtom();
    if (!funName)
        return;

    size_t length = funName->length();
    MOZ_ALWAYS_TRUE(length < bufLength);
    CopyChars(reinterpret_cast<Latin1Char*>(buffer), *funName);
    buffer[length] = 0;

    TraceEvent(TraceEventTypeNewScript, uint64_t(group));
    TraceString(buffer);
}

void
js::gc::TraceCreateObject(JSObject* object)
{
    if (!gcTraceFile)
        return;

    ObjectGroup* group = object->group();
    MaybeTraceGroup(group);
    TraceEvent(TraceEventCreateObject, uint64_t(object));
    TraceAddress(group);
}

void
js::gc::TraceMinorGCStart()
{
    TraceEvent(TraceEventMinorGCStart);
}

void
js::gc::TracePromoteToTenured(Cell* src, Cell* dst)
{
    TraceEvent(TraceEventPromoteToTenured, uint64_t(src));
    TraceAddress(dst);
}

void
js::gc::TraceMinorGCEnd()
{
    TraceEvent(TraceEventMinorGCEnd);
}

void
js::gc::TraceMajorGCStart()
{
    TraceEvent(TraceEventMajorGCStart);
}

void
js::gc::TraceTenuredFinalize(Cell* thing)
{
    if (!gcTraceFile)
        return;
    if (thing->tenuredGetAllocKind() == AllocKind::OBJECT_GROUP)
        tracedGroups.remove(static_cast<const ObjectGroup*>(thing));
    TraceEvent(TraceEventTenuredFinalize, uint64_t(thing));
}

void
js::gc::TraceMajorGCEnd()
{
    TraceEvent(TraceEventMajorGCEnd);
}

#endif
