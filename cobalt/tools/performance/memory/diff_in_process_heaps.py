#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Cobalt In-Process Heap Differential Bloat Analyzer.

This script compares two in-process heap profiling JSON traces (e.g., from
Cobalt 26 and Cobalt 27) containing 'heaps_v2' memory dumps. It automatically
walks the callstack prefix tree (trie), resolves symbol strings, categorizes
allocations across subsystems, filters noise, and outputs actionable tables
and markdown reports showing exactly where memory footprint grew.
"""

import argparse
import json
import logging
import os
import re
import sys
from typing import Any, Dict, List, Optional, Tuple

# Configure logging
logging.basicConfig(level=logging.INFO, format="%(message)s")
logger = logging.getLogger("diff_in_process_heaps")


class AllocationRecord:
  """Represents a normalized allocation group from a heap snapshot.

  This class aggregates and normalizes allocation data from heaps_v2
  snapshots to facilitate cross-trace comparison and differential analysis.

  Lifetime and Ownership:
    Instances are short-lived, created during trace parsing, and owned by
    the parsing and reporting functions.

  Threading Model:
    Not thread-safe; intended for single-threaded context (thread-affine).
  """

  def __init__(
      self,
      allocator: str,
      subsystem: str,
      leaf_symbol: str,
      callstack: List[str],
      *,
      size_bytes: int = 0,
      count: int = 1,
      container_symbol: str = "Unknown",
  ):
    self.allocator = allocator
    self.subsystem = subsystem
    self.leaf_symbol = leaf_symbol
    self.callstack = callstack
    self.size_bytes = size_bytes
    self.count = count
    self.container_symbol = container_symbol

  @property
  def normalized_stack_key(self) -> str:
    """Returns a normalized callstack string suitable for cross-trace matching,

    immune to worker thread line shifts.
    """
    cleaned = []
    for frame in self.callstack[:7]:
      if len(cleaned) >= 2 and is_boilerplate_or_scheduling_frame(frame):
        break
      clean_frame = frame.split(" (")[0]
      s = re.sub(r"0x[0-9a-fA-F]+", "0x*", clean_frame)
      cleaned.append(s)
    return " -> ".join(cleaned)


def format_bytes(size_bytes: float) -> str:
  """Formats byte counts into human-readable strings (KB, MB, GB)."""
  abs_bytes = abs(size_bytes)
  sign = "-" if size_bytes < 0 else ("+" if size_bytes > 0 else "")
  if abs_bytes >= 1024 * 1024 * 1024:
    return f"{sign}{abs_bytes / (1024 * 1024 * 1024):.2f} GB"
  elif abs_bytes >= 1024 * 1024:
    return f"{sign}{abs_bytes / (1024 * 1024):.2f} MB"
  elif abs_bytes >= 1024:
    return f"{sign}{abs_bytes / 1024:.2f} KB"
  else:
    return f"{sign}{int(abs_bytes)} B"


def classify_subsystem(allocator: str, stack: List[str]) -> str:
  """Classifies an allocation into a Cobalt architectural subsystem."""
  alloc_lower = allocator.lower() if allocator else ""
  stack_str = " -> ".join(stack).lower()

  if ("perfetto" in stack_str or "perfetto::" in stack_str or
      "heap_profiling" in stack_str or "memory_instrumentation" in stack_str or
      "trace_event" in stack_str or "protozero::" in stack_str):
    return "Tracing & Heap Profiler Overhead"
  if ("v8" in alloc_lower or "v8::" in stack_str or "/v8/" in stack_str or
      "v8_" in stack_str):
    return "V8 / JS Engine"
  if ("skia" in alloc_lower or "skia" in stack_str or "skgc" in stack_str or
      "::sk_" in stack_str or " sk_" in stack_str or
      "sk_malloc" in alloc_lower):
    return "Skia / Graphics"
  if "partition_alloc" in alloc_lower or "partitionalloc" in stack_str:
    if ("dom/" in stack_str or "dom::" in stack_str or "::dom" in stack_str or
        "css/" in stack_str or "css::" in stack_str or "html/" in stack_str or
        "html::" in stack_str or "layout/" in stack_str or
        "layout::" in stack_str):
      return "DOM / Layout (PartitionAlloc)"
    if ("media/" in stack_str or "media::" in stack_str or
        "audio" in stack_str or "video" in stack_str):
      return "Media (PartitionAlloc)"
    return "PartitionAlloc (Core/Other)"
  if ("dom/" in stack_str or "dom::" in stack_str or "::dom" in stack_str or
      "css/" in stack_str or "css::" in stack_str or "html/" in stack_str or
      "html::" in stack_str or "layout/" in stack_str or
      "layout::" in stack_str or "blink" in stack_str):
    return "DOM / Layout / Blink"
  if ("media/" in stack_str or "media::" in stack_str or "audio" in stack_str or
      "video" in stack_str or "codec" in stack_str or "decoder" in stack_str):
    return "Media / Audio / Video"
  if ("compositor" in stack_str or "renderer" in stack_str or
      "gl/" in stack_str or "gl::" in stack_str or "gpu" in stack_str):
    return "Compositor / GPU"
  if "mojo" in stack_str or "ipc::" in stack_str or "/ipc/" in stack_str:
    return "Mojo & IPC Channels"
  if ("net/" in stack_str or "net::" in stack_str or "url/" in stack_str or
      "url::" in stack_str or "network" in stack_str or "curl" in stack_str):
    return "Network / URL"
  if "cobalt::" in stack_str or "/cobalt/" in stack_str:
    return "Cobalt Browser Shell"
  if ("starboard/" in stack_str or "starboard::" in stack_str or
      "sb_" in stack_str or "rdk" in stack_str or "android" in stack_str):
    return "Starboard / OS Layer"
  if ("base/" in stack_str or "base::" in stack_str or
      "threading" in stack_str or "task_runner" in stack_str):
    return "Base / Utility"
  if "malloc" in alloc_lower:
    return "Malloc (Unclassified C++)"

  alloc_name = allocator or "Unknown"
  return f"Other ({alloc_name})"


def get_callstack_symbols(node_id: int,
                          node_to_info: Dict[int, Dict[str, Any]]) -> List[str]:
  """Walks the callstack prefix tree (trie) upwards from leaf node to root."""
  stack = []
  curr = node_id
  visited = set()
  while curr is not None and curr in node_to_info and curr not in visited:
    visited.add(curr)
    info = node_to_info[curr]
    stack.append(info["symbol"])
    curr = info.get("parent_id")
  return stack


_ALLOC_VERB_REGEX = re.compile(
    r"(alloc|malloc|calloc|realloc|dealloc|ensurecapacity|ensurespace|"
    r"tryexpand|expandtofit|refilllab|rehash|createuninitialized|\bfree\b|"
    r"\bgrow\b)",
    re.IGNORECASE,
)


def is_memory_management_frame(frame: str) -> bool:
  """Determines if a frame is an internal allocation or profiler helper."""
  exact_prefixes = [
      "base::PoissonAllocationSampler::",
      "base::allocator::dispatcher::",
      "allocator_shim::",
      "partition_alloc::",
      "base::internal::PartitionAlloc",
      "WTF::Partitions::",
      "WTF::FastMalloc",
      "WTF::BufferMalloc",
      "WTF::ArrayBufferContents::",
      "WTF::StringImpl::",
      "WTF::String::",
      "WTF::StringBuilder::",
      "WTF::StringBuffer",
      "WTF::TextCodecUTF8::Decode",
      "blink::ArrayBufferContents::",
      "blink::(anonymous namespace)::ArrayBufferAllocator::",
      "blink::RWBuffer::",
      "blink::ParkableImageImpl::",
      "blink::ParkableImageManager::CreateParkableImage",
      "blink::ParkableImage::",
      "blink::DeferredImageDecoder::Create",
      "blink::DeferredImageDecoder::SetDataInternal",
      "blink::BitmapImage::SetData",
      "blink::TextResourceDecoder::Decode",
      "blink::TextResource::DecodedText",
      "sk_malloc_",
      "sk_realloc_",
      "sk_free",
      "SkMemory::Alloc",
      "SkData::PrivateNewWithCopy",
      "SkArenaAlloc::ensureSpace",
      "starboard::ReuseAllocatorBase::",
      "starboard::EmbeddedMetadataReuseAllocatorBase::",
      "starboard::common::",
      "starboard::BidirectionalFitReuseAllocator",
      "media::BidirectionalFitDecoderBufferAllocatorStrategy",
      "media::DecoderBufferAllocator::",
      "media::StarboardMemoryAllocator::",
      "_uhash_",
      "uhash_put",
      "__wrap_",
      "operator new(",
      "operator new[](",
      "operator delete(",
      "malloc",
      "free",
      "realloc",
      "calloc",
      "posix_memalign",
      "base::UncheckedMalloc",
      "base::MakeRefCounted",
      "base::internal::WeakPtrFactoryBase",
      "std::__1::",
      "std::__Cr::",
      "std::Cr::",
      "std::make_unique",
      "protozero::ScatteredHeapBuffer::",
      "mojo::internal::ArraySerializer",
      "mojo::internal::Serializer",
      "mojo::StructTraits",
      "perfetto::",
      "heap_profiling::",
      "memory_instrumentation::",
      "trace_event::",
  ]
  if any(p in frame for p in exact_prefixes):
    return True

  file_indicators = [
      "/allocator/",
      "sampling_heap_profiler",
      "third_party/libc++/",
      "v8/src/heap/",
      "/sqlite3.c",
      "/uhash.cpp",
      "starboard/common/reuse_allocator",
      "starboard/common/embedded_metadata_reuse_allocator",
      "starboard_memory_allocator.h",
      "bidirectional_fit_",
      "/perfetto/",
      "/tracing/",
      "/memory_instrumentation/",
      "/heap_profiling/",
  ]
  if any(f in frame for f in file_indicators):
    return True

  symbol_part = frame.split(" (")[0]
  if _ALLOC_VERB_REGEX.search(symbol_part):
    return True

  return False


def is_boilerplate_or_scheduling_frame(frame: str) -> bool:
  """Determines if a frame is generic task scheduling or thread loop."""
  boilerplate_patterns = [
      "base::TaskAnnotator::RunTaskImpl",
      "base::internal::TaskTracker::",
      "base::internal::WorkerThread::",
      "base::(anonymous namespace)::ThreadFunc",
      "base::Thread::ThreadMain",
      "base::Thread::Run",
      "base::RunLoop::Run",
      "base::MessagePumpDefault::Run",
      "base::MessagePumpEpoll::Run",
      "base::MessagePumpGlib::Run",
      "base::sequence_manager::internal::",
      "base::OnceCallback<",
      "base::RepeatingCallback<",
      "base::internal::Invoker<",
      "base::internal::DecayedFunctorTraits<",
      "base::internal::FunctorTraits<",
      "mojo::Connector::",
      "mojo::InterfaceEndpointClient::",
      "mojo::MessageDispatcher::",
      "mojo::internal::MultiplexRouter::",
      "mojo::SimpleWatcher::",
      "network::mojom::URLLoaderClientStubDispatch::Accept",
      "ThreadPoolForegroundWorker",
      "ThreadPoolSingleThreadForegroundBlocking",
      "ThreadPoolServiceThread",
      "Chrome_InProcRendererThread",
      "MemoryInfra",
  ]
  return any(pat in frame for pat in boilerplate_patterns)


def get_immediate_container(stack: List[str]) -> str:
  """Returns immediate C++ container / data structure / allocator bridge."""
  if not stack:
    return "Unknown Container"
  raw_hooks = [
      "PoissonAllocationSampler", "allocator::dispatcher", "allocator_shim::",
      "partition_alloc::PartitionRoot::Alloc",
      "PartitionAllocatorAllocationHook", "__wrap_", "operator new(",
      "operator new[](", "operator delete(", "shim_alloc_functions",
      "MemoryAllocatorDump", "ProcessMemoryDump"
  ]
  for frame in stack:
    if not any(hook in frame for hook in raw_hooks):
      return frame.split(" (")[0]
  return stack[0].split(" (")[0]


def filter_profiler_hooks(stack: List[str]) -> List[str]:
  """Strips internal profiler, container, and raw allocation frames."""
  filtered = [frame for frame in stack if not is_memory_management_frame(frame)]
  return filtered if filtered else stack


def get_actionable_leaf(stack: List[str]) -> str:
  """Finds true caller by skipping allocators and scheduling boilerplate."""
  if not stack:
    return "unknown_leaf"

  for frame in stack:
    if not is_memory_management_frame(
        frame) and not is_boilerplate_or_scheduling_frame(frame):
      if frame.startswith("pc:") or frame.startswith(
          "unresolved_sid_") or frame == "unknown_leaf":
        continue
      return frame

  for frame in stack:
    if not is_memory_management_frame(frame):
      if "mojo::" in frame or "mojom::" in frame:
        return "[Mojo / IPC Dispatch Boilerplate]"
      if "TaskQueueImpl" in frame or "sequence_manager" in frame:
        return "[TaskQueue / MessagePump Boilerplate]"
      if ("TaskAnnotator" in frame or "TaskTracker" in frame or
          "WorkerThread" in frame or frame.startswith("pc:") or
          frame.startswith("unresolved_sid_")):
        return "[Untracked / Unresolved Background Task Pool]"
      return frame

  return stack[0]


def extract_allocations(heaps_v2: Dict[str, Any]) -> List[AllocationRecord]:
  """Extracts and normalizes all allocation records from a heaps_v2 snapshot."""
  maps = heaps_v2.get("maps", heaps_v2)
  strings_list = maps.get("strings", heaps_v2.get("strings", []))
  nodes_list = maps.get("nodes", heaps_v2.get("nodes", []))
  types_list = maps.get("types", heaps_v2.get("types", []))

  id_to_string = {}
  for entry in strings_list:
    if isinstance(entry, dict) and "id" in entry and "string" in entry:
      id_to_string[entry["id"]] = entry["string"]

  id_to_type = {}
  for t in types_list:
    if isinstance(t, dict) and "id" in t and "name_sid" in t:
      t_id = t["id"]
      id_to_type[t_id] = id_to_string.get(t["name_sid"], f"type_{t_id}")

  node_to_info = {}
  for n in nodes_list:
    if not isinstance(n, dict) or "id" not in n:
      continue
    node_id = n["id"]
    name_id = n.get(
        "name_sid",
        n.get("name_id", n.get("sid", n.get("name", n.get("string_id")))))
    parent_id = n.get(
        "parent_id",
        n.get("parent", n.get("parent_sid", n.get("parent_node_id"))),
    )

    if isinstance(name_id, int):
      symbol = id_to_string.get(name_id, f"unresolved_sid_{name_id}")
    elif isinstance(name_id, str):
      symbol = name_id
    else:
      symbol = f"node_{node_id}"

    node_to_info[node_id] = {
        "symbol": symbol,
        "parent_id": parent_id,
        "size": n.get("size", n.get("self_size", n.get("bytes", 0))),
        "count": n.get("count", n.get("objects", 0)),
    }

  records = []
  allocators = heaps_v2.get("allocators", {})

  def _parse_size_and_count(item: Dict[str, Any]) -> Tuple[int, int]:
    size = item.get("size", item.get("bytes", 0))
    count = item.get("count",
                     item.get("objects", item.get("allocations_count", 1)))

    if not size and "attrs" in item and isinstance(item["attrs"], dict):
      size_obj = item["attrs"].get("size", {})
      if isinstance(size_obj, dict):
        size = size_obj.get("value", 0)
      elif isinstance(size_obj, (int, float)):
        size = size_obj

    if size is None:
      size = 0
    if count is None:
      count = 1

    if isinstance(size, str):
      try:
        size = int(size, 16) if size.startswith("0x") else int(size)
      except ValueError:
        size = 0
    if isinstance(count, str):
      try:
        count = int(count)
      except ValueError:
        count = 1
    return int(size), int(count)

  # Check top-level or maps-level 'allocations' list first
  top_allocations = heaps_v2.get(
      "allocations",
      maps.get("allocations", heaps_v2.get("counts", maps.get("counts", []))))
  if isinstance(top_allocations, list) and len(top_allocations) > 0:
    for item in top_allocations:
      if not isinstance(item, dict):
        continue
      node_id = item.get(
          "node_id",
          item.get("node", item.get("id", item.get("callstack_node_id"))))
      type_id = item.get("type_id", item.get("type"))
      alloc_name = item.get("allocator", "heap_allocations")
      if type_id and type_id in id_to_type and alloc_name == "heap_allocations":
        alloc_name = id_to_type[type_id]

      size, count = _parse_size_and_count(item)
      if size != 0:
        stack = (
            get_callstack_symbols(node_id, node_to_info)
            if node_id and node_id in node_to_info else [f"[{alloc_name}]"])
        clean_stack = filter_profiler_hooks(stack)
        leaf = get_actionable_leaf(clean_stack)
        container = get_immediate_container(stack)
        subsystem = classify_subsystem(alloc_name, clean_stack)
        records.append(
            AllocationRecord(
                alloc_name,
                subsystem,
                leaf,
                clean_stack,
                size_bytes=size,
                count=count,
                container_symbol=container,
            ))

  if isinstance(allocators, dict):
    for alloc_name, alloc_data in allocators.items():
      if not isinstance(alloc_data, dict):
        continue

      # 1. Check for parallel arrays (column-oriented: nodes, sizes, counts)
      col_nodes = alloc_data.get("nodes")
      col_sizes = alloc_data.get("sizes", alloc_data.get("bytes"))
      col_counts = alloc_data.get("counts", alloc_data.get("objects"))
      col_types = alloc_data.get("types")

      if (isinstance(col_nodes, list) and len(col_nodes) > 0 and
          not isinstance(col_nodes[0], dict)):
        # Column-oriented parallel arrays!
        for idx, node_id in enumerate(col_nodes):
          size = (
              col_sizes[idx]
              if isinstance(col_sizes, list) and idx < len(col_sizes) else 0)
          if size is None:
            size = 0
          count = (
              col_counts[idx]
              if isinstance(col_counts, list) and idx < len(col_counts) else 1)
          if count is None:
            count = 1
          type_id = (
              col_types[idx]
              if isinstance(col_types, list) and idx < len(col_types) else None)

          eff_alloc_name = alloc_name
          if type_id is not None and type_id in id_to_type:
            type_name = id_to_type[type_id]
            if type_name and not type_name.startswith("type_"):
              eff_alloc_name = f"{alloc_name}:{type_name}"

          if size != 0:
            stack = (
                get_callstack_symbols(node_id, node_to_info) if node_id and
                node_id in node_to_info else [f"[{eff_alloc_name}]"])
            clean_stack = filter_profiler_hooks(stack)
            leaf = get_actionable_leaf(clean_stack)
            container = get_immediate_container(stack)
            subsystem = classify_subsystem(eff_alloc_name, clean_stack)
            records.append(
                AllocationRecord(
                    eff_alloc_name,
                    subsystem,
                    leaf,
                    clean_stack,
                    size_bytes=int(size),
                    count=int(count),
                    container_symbol=container,
                ))
        continue

      # 2. Check for row-oriented dictionary items (list of dicts)
      node_items = None
      for list_key in ["nodes", "allocations", "counts", "types", "items"]:
        if list_key in alloc_data and isinstance(alloc_data[list_key], list):
          node_items = alloc_data[list_key]
          break

      if node_items is not None and len(node_items) > 0 and isinstance(
          node_items[0], dict):
        for item in node_items:
          if not isinstance(item, dict):
            continue
          node_id = item.get(
              "node_id",
              item.get("node", item.get("id", item.get("callstack_node_id"))))
          size, count = _parse_size_and_count(item)

          if size != 0:
            stack = (
                get_callstack_symbols(node_id, node_to_info)
                if node_id and node_id in node_to_info else [f"[{alloc_name}]"])
            clean_stack = filter_profiler_hooks(stack)
            leaf = get_actionable_leaf(clean_stack)
            container = get_immediate_container(stack)
            subsystem = classify_subsystem(alloc_name, clean_stack)
            records.append(
                AllocationRecord(
                    alloc_name,
                    subsystem,
                    leaf,
                    clean_stack,
                    size_bytes=size,
                    count=count,
                    container_symbol=container,
                ))
      else:
        size, count = _parse_size_and_count(alloc_data)
        if size != 0:
          stack = [f"[{alloc_name}] (Aggregate)"]
          subsystem = classify_subsystem(alloc_name, stack)
          records.append(
              AllocationRecord(
                  alloc_name,
                  subsystem,
                  stack[0],
                  stack,
                  size_bytes=size,
                  count=count,
                  container_symbol=stack[0],
              ))

  # Fallback: check node_to_info sizes directly if allocators empty
  if not records:
    for node_id, info in node_to_info.items():
      size = info.get("size", 0)
      if size is None:
        size = 0
      count = info.get("count", 1)
      if count is None:
        count = 1
      if isinstance(size, str):
        try:
          size = int(size, 16) if size.startswith("0x") else int(size)
        except ValueError:
          size = 0
      if isinstance(count, str):
        try:
          count = int(count)
        except ValueError:
          count = 1

      if size != 0:
        stack = get_callstack_symbols(node_id, node_to_info)
        clean_stack = filter_profiler_hooks(stack)
        leaf = get_actionable_leaf(clean_stack)
        container = get_immediate_container(stack)
        subsystem = classify_subsystem("heap_nodes", clean_stack)
        records.append(
            AllocationRecord(
                "heap_nodes",
                subsystem,
                leaf,
                clean_stack,
                size_bytes=size,
                count=count,
                container_symbol=container,
            ))

  return records


def parse_snapshot(
    trace_path: str,
    snapshot_index: int = -1) -> Optional[Tuple[Dict[str, Any], str]]:
  """Extracts a target heaps_v2 snapshot dictionary from a trace file.

  Args:
    trace_path: Path to the JSON trace file.
    snapshot_index: Index of the snapshot to extract (-1 for latest/steady
      state).

  Returns:
    Tuple of (heaps_v2_dict, snapshot_description) or None on failure.
  """
  if not os.path.isfile(trace_path):
    logger.error("❌ Error: Trace file not found: %s", trace_path)
    return None

  try:
    with open(trace_path, "r", encoding="utf-8", errors="replace") as f:
      trace_data = json.load(f)
  except (json.JSONDecodeError, OSError, ValueError, UnicodeDecodeError) as e:
    logger.error("❌ Error reading JSON trace from %s: %s", trace_path, e)
    return None

  # Check if trace_data is already a direct dumps or heaps_v2 object
  if isinstance(trace_data, dict):
    base_nm = os.path.basename(trace_path)
    if "heaps_v2" in trace_data and isinstance(trace_data["heaps_v2"], dict):
      return trace_data["heaps_v2"], f"Direct dumps dictionary in {base_nm}"
    if "maps" in trace_data and ("strings" in trace_data["maps"] or
                                 "nodes" in trace_data["maps"]):
      return trace_data, f"Direct heaps_v2 object in {base_nm}"
    if "allocators" in trace_data:
      return trace_data, f"Direct heaps_v2 object in {base_nm}"

  # Otherwise scan traceEvents for memory dumps
  events = []
  if isinstance(trace_data, list):
    events = trace_data
  elif isinstance(trace_data, dict):
    events = trace_data.get("traceEvents", [])

  snapshots = []
  for e in events:
    if not isinstance(e, dict):
      continue
    args = e.get("args", {})
    if not isinstance(args, dict) or "dumps" not in args:
      continue

    dumps = args["dumps"]
    if isinstance(dumps, str):
      try:
        dumps = json.loads(dumps)
      except json.JSONDecodeError:
        continue

    if isinstance(dumps, dict) and "heaps_v2" in dumps:
      ts = e.get("ts", 0)
      name = e.get("name", "memory_dump")
      snapshots.append((dumps["heaps_v2"], f"{name} at ts={ts} µs"))

  if not snapshots:
    logger.error("❌ Error: No valid 'heaps_v2' memory snapshots found in %s",
                 trace_path)
    return None

  # If snapshot_index is -1, pick the snapshot with peak allocation size
  # instead of blindly picking the last (which might be an empty shutdown dump).
  if snapshot_index == -1 and len(snapshots) > 1:
    best_idx = -1
    best_size = -1
    for i, (heaps_obj, desc) in enumerate(snapshots):
      recs = extract_allocations(heaps_obj)
      tot = sum(r.size_bytes for r in recs)
      if tot > best_size:
        best_size = tot
        best_idx = i
    selected_heaps, desc = snapshots[best_idx]
    logger.info(
        "✅ Selected peak steady-state snapshot %d/%d from %s (%s, %s in %d "
        "records)",
        best_idx + 1,
        len(snapshots),
        os.path.basename(trace_path),
        desc,
        format_bytes(best_size),
        len(extract_allocations(selected_heaps)),
    )
    return selected_heaps, desc
  else:
    try:
      selected_heaps, desc = snapshots[snapshot_index]
      recs = extract_allocations(selected_heaps)
      logger.info(
          "✅ Selected snapshot %d/%d from %s (%s, %s in %d records)",
          snapshot_index if snapshot_index >= 0 else len(snapshots) +
          snapshot_index + 1,
          len(snapshots),
          os.path.basename(trace_path),
          desc,
          format_bytes(sum(r.size_bytes for r in recs)),
          len(recs),
      )
      return selected_heaps, desc
    except IndexError:
      logger.error(
          "❌ Error: Snapshot index %d out of range (total %d snapshots in %s)",
          snapshot_index,
          len(snapshots),
          trace_path,
      )
      return None


def aggregate_by_key(records: List[AllocationRecord],
                     key_func) -> Dict[str, Tuple[int, int]]:
  """Aggregates size_bytes and count by a custom grouping key function."""
  aggregated = {}
  for r in records:
    key = key_func(r)
    size, count = aggregated.get(key, (0, 0))
    aggregated[key] = (size + r.size_bytes, count + r.count)
  return aggregated


def compare_aggregates(
    base_agg: Dict[str, Tuple[int, int]],
    target_agg: Dict[str, Tuple[int, int]],
    threshold_bytes: int = 0,
) -> List[Dict[str, Any]]:
  """Compares two aggregated dictionaries and computes diffs."""
  all_keys = set(base_agg.keys()) | set(target_agg.keys())
  results = []

  for key in all_keys:
    base_size, base_count = base_agg.get(key, (0, 0))
    target_size, target_count = target_agg.get(key, (0, 0))
    diff_size = target_size - base_size
    diff_count = target_count - base_count

    if abs(diff_size) < threshold_bytes:
      continue

    pct_change = ((diff_size / base_size * 100.0) if base_size > 0 else
                  (100.0 if target_size > 0 else 0.0))
    results.append({
        "key": key,
        "base_size": base_size,
        "target_size": target_size,
        "diff_size": diff_size,
        "base_count": base_count,
        "target_count": target_count,
        "diff_count": diff_count,
        "pct_change": pct_change,
    })

  # Sort by absolute difference descending, then by signed diff
  results.sort(
      key=lambda x: (abs(x["diff_size"]), x["diff_size"]), reverse=True)
  return results


def generate_single_profile_report(
    target_path: str,
    target_desc: str,
    target_records: List[AllocationRecord],
) -> str:
  """Generates a single-profile diagnostic report without a base trace."""
  target_total = sum(r.size_bytes for r in target_records)

  lines = []
  lines.append("# Cobalt Native Heap Memory Diagnosis Report (Single Profile)")
  lines.append("")
  lines.append("> [!NOTE]")
  lines.append(
      "> This is a **single-profile heap diagnostic report** analyzing the"
      f" memory footprint of `{target_path}` ({target_desc}).")
  lines.append("")
  lines.append("## 1. Executive Summary & Total Accounted Footprint")
  lines.append("")
  lines.append(f"- **Trace File:** `{target_path}` ({target_desc})")
  lines.append(f"- **Total Accounted Heap:** `{format_bytes(target_total)}`")
  lines.append(f"- **Total Allocation Records:** `{len(target_records)}`")
  lines.append("")

  # 2. Subsystem Breakdown
  lines.append("## 2. Subsystem Memory Breakdown")
  lines.append(
      "This table attributes every in-process PartitionAlloc, V8, and malloc"
      " allocation to its corresponding browser subsystem.")
  lines.append("")
  lines.append("| Subsystem | Allocated Size | Percentage |")
  lines.append("| :--- | :--- | :--- |")
  sub_agg = aggregate_by_key(target_records, lambda r: r.subsystem)
  for sub, (size, _) in sorted(
      sub_agg.items(), key=lambda x: x[1][0], reverse=True):
    pct = (size / target_total * 100.0) if target_total > 0 else 0.0
    lines.append(f"| **{sub}** | {format_bytes(size)} | {pct:.2f}% |")
  lines.append("")

  # 3. Top Hotspots
  lines.append("## 3. Top 30 Allocation Hotspots")
  lines.append(
      "These are the individual C++ callstacks responsible for the largest "
      "memory allocations.")
  lines.append("")
  header_t3 = (
      "| Rank | Subsystem | Allocator & Container Bridge | Actionable Domain "
      "Caller | C++ Callstack (Compact) | Size |")
  lines.append(header_t3)
  lines.append("| :--- | :--- | :--- | :--- | :--- | :--- |")

  def _get_t3_key(r: AllocationRecord) -> str:
    leaf = r.leaf_symbol.split(" (")[0]
    return (f"{r.subsystem} __INFO_SEP__ {r.allocator} __INFO_SEP__ "
            f"{r.container_symbol} __INFO_SEP__ {leaf} __INFO_SEP__ "
            f"{r.normalized_stack_key}")

  stack_agg = aggregate_by_key(
      [
          r for r in target_records
          if r.subsystem != "Tracing & Heap Profiler Overhead"
      ],
      _get_t3_key,
  )
  rank = 1
  for key, (size, _) in sorted(
      stack_agg.items(), key=lambda x: x[1][0], reverse=True)[:30]:
    parts = key.split(" __INFO_SEP__ ", 4)
    subsys = parts[0]
    allocator = parts[1] if len(parts) > 1 else "Unknown"
    container = (parts[2] if len(parts) > 2 else "Unknown").replace("|", "\\|")
    leaf_esc = (parts[3] if len(parts) > 3 else "Unknown").replace("|", "\\|")
    callstack_str = parts[4] if len(parts) > 4 else key
    callstack_formatted = callstack_str.replace(" -> ", "<br>↳ ")
    size_str = format_bytes(size)
    row_t3 = (f"| {rank} | {subsys} | `{allocator}` <br>↳ `{container}` | "
              f"`{leaf_esc}` | `{callstack_formatted}` | **{size_str}** |")
    lines.append(row_t3)
    rank += 1
  lines.append("")
  return "\n".join(lines)


def generate_report(
    base_path: str,
    base_desc: str,
    base_records: List[AllocationRecord],
    target_path: str,
    *,
    target_desc: str,
    target_records: List[AllocationRecord],
    threshold_bytes: int,
) -> str:
  """Generates the comprehensive differential memory report in Markdown."""
  base_total = sum(r.size_bytes for r in base_records)
  target_total = sum(r.size_bytes for r in target_records)
  diff_total = target_total - base_total
  pct_total = (diff_total / base_total * 100.0) if base_total > 0 else 0.0

  lines = []
  lines.append("# Cobalt In-Process Heap Differential Bloat Analysis")
  lines.append("")
  lines.append("> [!NOTE]")
  lines.append(
      "> This is a **differential heap comparison report** analyzing the memory"
      f" delta between `{base_path}` (Base) and `{target_path}` (Target).")
  lines.append("")
  lines.append("## 1. Executive Summary & Total Accounted Footprint")
  lines.append("")
  lines.append(
      f"- **Base Version (e.g. Cobalt 26):** `{base_path}` ({base_desc})")
  lines.append(
      f"- **Target Version (e.g. Cobalt 27):** `{target_path}` ({target_desc})")
  lines.append(
      f"- **Noise Filter Threshold:** `{format_bytes(threshold_bytes)}`")
  lines.append("")
  lines.append(
      "| Metric | Base Version | Target Version | Diff (+/-) | % Change |")
  lines.append("| :--- | :--- | :--- | :--- | :--- |")
  base_tot_str = format_bytes(base_total)
  target_tot_str = format_bytes(target_total)
  diff_tot_str = format_bytes(diff_total)
  lines.append(
      f"| **Total Accounted Heap** | {base_tot_str} | {target_tot_str} | "
      f"**{diff_tot_str}** | {pct_total:+.2f}% |")
  lines.append("")

  # 2. Allocator Breakdown
  lines.append("## 2. Allocation Breakdown by Allocator Type")
  lines.append("")
  lines.append(
      "| Allocator | Base Size | Target Size | Diff (+/-) | % Change |")
  lines.append("| :--- | :--- | :--- | :--- | :--- |")
  alloc_diffs = compare_aggregates(
      aggregate_by_key(base_records, lambda r: r.allocator),
      aggregate_by_key(target_records, lambda r: r.allocator),
  )
  for item in alloc_diffs:
    key = item["key"]
    base_sz = format_bytes(item["base_size"])
    target_sz = format_bytes(item["target_size"])
    diff_sz = format_bytes(item["diff_size"])
    pct = item["pct_change"]
    lines.append(f"| **{key}** | {base_sz} | {target_sz} | **{diff_sz}** | "
                 f"{pct:+.2f}% |")
  lines.append("")

  # 3. Subsystem Breakdown
  lines.append("## 3. Allocation Breakdown by Architectural Subsystem")
  lines.append("")
  lines.append(
      "| Subsystem | Base Size | Target Size | Diff (+/-) | % Change |")
  lines.append("| :--- | :--- | :--- | :--- | :--- |")
  sub_diffs = compare_aggregates(
      aggregate_by_key(base_records, lambda r: r.subsystem),
      aggregate_by_key(target_records, lambda r: r.subsystem),
  )
  for item in sub_diffs:
    key = item["key"]
    base_sz = format_bytes(item["base_size"])
    target_sz = format_bytes(item["target_size"])
    diff_sz = format_bytes(item["diff_size"])
    pct = item["pct_change"]
    lines.append(f"| **{key}** | {base_sz} | {target_sz} | **{diff_sz}** | "
                 f"{pct:+.2f}% |")
  # 4. Top Differential Hotspots & Callstacks (Unified Table)
  lines.append("## 4. Top Differential Allocation Hotspots & Callstacks")
  lines.append("")
  thresh_str = format_bytes(threshold_bytes)
  lines.append(
      "Shows top C++ callstacks and leaf functions whose memory footprint "
      f"changed by more than `{thresh_str}`, sorted by absolute growth.")
  lines.append("")
  header_t4 = (
      "| Rank | Subsystem | Allocator & Container Bridge | Actionable Domain "
      "Caller | C++ Callstack (Compact) | Base Size | Target Size | Diff "
      "(+/-) |")
  lines.append(header_t4)
  lines.append("| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |")

  def _get_t4_key(r: AllocationRecord) -> str:
    leaf = r.leaf_symbol.split(" (")[0]
    return (f"{r.subsystem} __INFO_SEP__ {r.allocator} __INFO_SEP__ "
            f"{r.container_symbol} __INFO_SEP__ {leaf} __INFO_SEP__ "
            f"{r.normalized_stack_key}")

  # Group by (subsystem, allocator, container, clean_leaf, stack_key)
  stack_base_agg = {}
  for r in base_records:
    if r.subsystem == "Tracing & Heap Profiler Overhead":
      continue
    k = _get_t4_key(r)
    size, count = stack_base_agg.get(k, (0, 0))
    stack_base_agg[k] = (size + r.size_bytes, count + r.count)

  stack_target_agg = {}
  for r in target_records:
    if r.subsystem == "Tracing & Heap Profiler Overhead":
      continue
    k = _get_t4_key(r)
    size, count = stack_target_agg.get(k, (0, 0))
    stack_target_agg[k] = (size + r.size_bytes, count + r.count)

  stack_diffs = compare_aggregates(stack_base_agg, stack_target_agg,
                                   threshold_bytes)

  count_shown = 0
  for item in stack_diffs[:30]:
    if item["diff_size"] == 0:
      continue
    count_shown += 1
    parts = item["key"].split(" __INFO_SEP__ ", 4)
    subsys = parts[0]
    allocator = parts[1] if len(parts) > 1 else "Unknown"
    container = (parts[2] if len(parts) > 2 else "Unknown").replace("|", "\\|")
    leaf_esc = (parts[3] if len(parts) > 3 else "Unknown").replace("|", "\\|")
    callstack_str = parts[4] if len(parts) > 4 else item["key"]
    callstack_compact = callstack_str.replace(" -> ", "<br>↳ ")
    base_sz = format_bytes(item["base_size"])
    target_sz = format_bytes(item["target_size"])
    diff_sz = format_bytes(item["diff_size"])
    row_t4 = (
        f"| {count_shown} | {subsys} | `{allocator}` <br>↳ `{container}` | "
        f"`{leaf_esc}` | `{callstack_compact}` | {base_sz} | {target_sz} | "
        f"**{diff_sz}** |")
    lines.append(row_t4)

  lines.append("")
  if count_shown == 0:
    lines.append(
        f"*(No hotspot deltas exceeded {format_bytes(threshold_bytes)})*\n")

  return "\n".join(lines)


def print_console_summary(markdown_report: str, output_path: str) -> None:
  """Prints a concise executive summary to the console terminal."""
  print("\n" + "=" * 80)
  lines = markdown_report.splitlines()
  is_single = "Single Profile" in lines[0] if lines else False

  if is_single:
    print("\033[1;36m📊 COBALT NATIVE HEAP DIAGNOSIS SUMMARY (Single "
          "Profile)\033[0m")
  else:
    print("\033[1;36m📊 COBALT HEAP DIFFERENTIAL BLOAT SUMMARY\033[0m")
  print("-" * 80)

  # Print Executive Summary bullet points (Section 1)
  in_sec1 = False
  for line in lines:
    if line.startswith("## 1. Executive Summary"):
      in_sec1 = True
      continue
    if line.startswith("## 2."):
      in_sec1 = False
      break
    if in_sec1 and (line.startswith("- ") or
                    line.startswith("| **Total Accounted")):
      print(f"  {line}")

  print("\n\033[1;33m🏢 Top 5 Subsystem Footprints / Deltas:\033[0m")

  # Find Subsystem table rows and print top 5
  in_sub_table = False
  sub_count = 0
  for line in lines:
    if ("Subsystem Memory Breakdown" in line or
        "Breakdown by Architectural Subsystem" in line):
      in_sub_table = True
      continue
    if line.startswith("## 3.") or line.startswith("## 4."):
      in_sub_table = False
      break
    if in_sub_table and line.startswith(
        "| **") and not line.startswith("| **Subsystem**"):
      sub_count += 1
      if sub_count <= 5:
        # Strip markdown table borders for clean terminal display
        parts = [p.strip() for p in line.split("|")[1:-1]]
        if len(parts) >= 3:
          sub_name = parts[0].replace("**", "")
          size_val = parts[1] if is_single else parts[3]
          pct_val = parts[2] if is_single else parts[4]
          print(f"  {sub_count}. {sub_name:<32} {size_val:>12} ({pct_val})")

  print("-" * 80)
  msg_saved = (
      "\033[1;32m🎉 Full diagnostic report (all 30+ hotspots & callstack "
      "trees) saved to:\033[0m")
  print(msg_saved)
  print(f"   {output_path}")
  print("=" * 80 + "\n")


def main():
  parser = argparse.ArgumentParser(
      description=(
          "Cobalt In-Process Heap Differential Bloat Analyzer.\n"
          "Compares two heap profiling traces and categorizes footprint deltas."
      ))
  # Support positional and --base / --target flags for flexibility
  parser.add_argument(
      "traces",
      nargs="*",
      help="Paths to base and target trace files",
  )
  parser.add_argument(
      "-b",
      "--base",
      dest="base_path",
      help="Path to base trace file (e.g. /tmp/c26_trace.json)",
  )
  parser.add_argument(
      "-t",
      "--target",
      dest="target_path",
      help="Path to target trace file (e.g. /tmp/c27_trace.json)",
  )
  parser.add_argument(
      "-o",
      "--output",
      dest="output_path",
      help="Path to save markdown report (e.g. /tmp/c26_vs_c27_report.md)",
  )
  parser.add_argument(
      "--threshold-kb",
      type=float,
      default=10.0,
      help="Threshold in KB below which deltas are filtered (default: 10 KB)",
  )
  parser.add_argument(
      "--base-snapshot-index",
      type=int,
      default=-1,
      help="Snapshot index from base trace (-1 for steady state)",
  )
  parser.add_argument(
      "--target-snapshot-index",
      type=int,
      default=-1,
      help="Snapshot index from target trace (-1 for steady state)",
  )

  args = parser.parse_args()

  # Resolve base and target paths from positional or option flags
  base_path = args.base_path
  target_path = args.target_path

  if args.traces:
    if len(args.traces) == 1:
      if not target_path:
        target_path = args.traces[0]
      elif not base_path:
        base_path = args.traces[0]
    elif len(args.traces) >= 2:
      if not base_path:
        base_path = args.traces[0]
      if not target_path:
        target_path = args.traces[1]

  if not target_path:
    parser.error(
        "Specify at least a target trace to analyze (or base and target).\n"
        "Example: python3 diff_in_process_heaps.py /tmp/c26.json /tmp/c27.json")

  target_path = os.path.abspath(target_path)
  threshold_bytes = int(args.threshold_kb * 1024)

  logger.info("🔍 Loading Target Trace: %s", target_path)
  target_res = parse_snapshot(target_path, args.target_snapshot_index)
  if not target_res:
    sys.exit(1)
  target_heaps, target_desc = target_res
  target_records = extract_allocations(target_heaps)

  if base_path:
    base_path = os.path.abspath(base_path)
    logger.info("🔍 Loading Base Trace:   %s", base_path)
    base_res = parse_snapshot(base_path, args.base_snapshot_index)
    if not base_res:
      sys.exit(1)
    base_heaps, base_desc = base_res
    base_records = extract_allocations(base_heaps)

    logger.info(
        "📊 Extracted %d allocation records from Base, %d from Target.",
        len(base_records),
        len(target_records),
    )
    logger.info("📝 Generating differential report...")
    report = generate_report(
        base_path,
        base_desc,
        base_records,
        target_path,
        target_desc=target_desc,
        target_records=target_records,
        threshold_bytes=threshold_bytes,
    )
  else:
    logger.info(
        "📊 Extracted %d allocation records from Target (Single Profile Mode).",
        len(target_records),
    )
    logger.info("📝 Generating single-profile diagnostic report...")
    report = generate_single_profile_report(target_path, target_desc,
                                            target_records)

  if args.output_path:
    out_path = os.path.abspath(args.output_path)
  else:
    base_name = os.path.splitext(os.path.basename(target_path))[0]
    out_path = os.path.abspath(
        os.path.join(
            os.path.dirname(target_path), f"{base_name}_memory_report.md"))

  try:
    with open(out_path, "w", encoding="utf-8") as f:
      f.write(report)
  except (OSError, IOError, UnicodeEncodeError) as e:
    logger.error("❌ Error writing output report to %s: %s", out_path, e)

  print_console_summary(report, out_path)
  return 0


if __name__ == "__main__":
  sys.exit(main())
