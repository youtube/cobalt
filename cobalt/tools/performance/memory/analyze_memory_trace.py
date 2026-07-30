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
"""Analyzes memory-infra dumps from Perfetto or Chromium traces.

Parses Perfetto protobuf traces (.perfetto-trace, .pftrace) and Chromium JSON
traces (.json, .json.gz), and generates comprehensive reports for:
  - V8 JavaScript Engine (heaps, code space, old/new space, spaces breakdown)
  - Blink & DOM (Blink GC heap, normal/custom pages, DOM node/doc counts)
  - Skia Graphics (glyph cache, font subcaches, GPU text/resources)
  - CC (Compositor tile memory, layer resources, active vs free)
  - GPU (shared image mailboxes, textures, format & dimensions)
  - PartitionAlloc & Malloc (buffers, array buffers, fragmentation)
  - Other memory-infra categories (parkable strings, shared memory, storage)

Usage:
  # Analyze a Perfetto trace file:
  python3 cobalt/tools/performance/memory/analyze_memory_trace.py \
      trace.perfetto-trace

  # Verbose / detailed breakdowns of individual mailboxes and caches:
  python3 cobalt/tools/performance/memory/analyze_memory_trace.py \
      trace.perfetto-trace -v

  # Compare initial vs final dumps (delta / memory growth):
  python3 cobalt/tools/performance/memory/analyze_memory_trace.py \
      trace.perfetto-trace --compare

  # Export report as Markdown:
  python3 cobalt/tools/performance/memory/analyze_memory_trace.py \
      trace.perfetto-trace -o report.md

  # Capture live trace from localhost:9222 and analyze immediately:
  python3 cobalt/tools/performance/memory/analyze_memory_trace.py --live
"""

# pylint: disable=inconsistent-quotes,broad-exception-caught

import argparse
import asyncio
import gzip
import json
import math
import os
import sys
import tempfile
from typing import Any, Dict, List, Optional, Tuple


def read_varint(data: bytes, pos: int) -> Tuple[Optional[int], int]:
  """Reads a protobuf varint from data starting at pos."""
  val = 0
  shift = 0
  length = len(data)
  while pos < length:
    b = data[pos]
    pos += 1
    val |= (b & 0x7F) << shift
    if not b & 0x80:
      return val, pos
    shift += 7
  return None, pos


def to_int(val: Any) -> int:
  """Converts various value representations (int, float, hex, str) to int."""
  if val is None:
    return 0
  if isinstance(val, (int, float)):
    if math.isnan(val) or math.isinf(val):
      return 0
    return int(val)
  if isinstance(val, str):
    val_s = val.strip()
    if not val_s:
      return 0
    if val_s.startswith(("0x", "0X")):
      try:
        return int(val_s, 16)
      except ValueError:
        return 0
    # Try decimal int first
    try:
      return int(val_s)
    except ValueError:
      pass
    # Try scientific notation float (e.g. '1e6', '1.5e3') or decimal float
    if "." in val_s or ("e" in val_s.lower() and
                        any(c.isdigit() for c in val_s)):
      try:
        f = float(val_s)
        if math.isnan(f) or math.isinf(f):
          return 0
        return int(f)
      except (ValueError, OverflowError):
        pass
    # Try hex (e.g. Chromium un-prefixed hex byte counts like '7e9000')
    try:
      return int(val_s, 16)
    except ValueError:
      pass
    # Try float fallback
    try:
      f = float(val_s)
      if math.isnan(f) or math.isinf(f):
        return 0
      return int(f)
    except (ValueError, OverflowError):
      return 0
  return 0


class MemoryDump:
  """Represents a single global memory dump captured within a trace.

  This class aggregates allocator node allocations and computes subsystem
  totals to analyze memory-infra dumps.

  Lifetime and Ownership:
    Instances are created during trace parsing (e.g., in load_trace) and are
    owned by the caller/parser. They are expected to live as long as the
    analysis or report generation is active.

  Threading Model:
    This class is not thread-safe and is Thread-affine (intended to be used
    from a single thread).
  """

  def __init__(
      self,
      dump_id: str,
      timestamp: Optional[int] = None,
      pid: Optional[int] = None,
  ):
    self.dump_id = dump_id
    self.timestamp = timestamp
    self.pid = pid
    # Map of allocator node name -> {"size": int, "entries": {key: val}}
    self.nodes: Dict[str, Dict[str, Any]] = {}
    self._leaf_nodes: Optional[Dict[str, Dict[str, Any]]] = None

  def add_node(
      self,
      name: str,
      size_bytes: Optional[int],
      entries: Optional[Dict[str, Any]] = None,
  ):
    self.nodes[name] = {
        "size": size_bytes,
        "entries": entries or {},
    }
    self._leaf_nodes = None

  @staticmethod
  def node_size(info: Dict[str, Any]) -> int:
    """Extracts size in bytes from node info, checking size and entry fields."""
    if not info:
      return 0
    return (to_int(info.get("size")) or
            to_int(info.get("entries", {}).get("size")) or
            to_int(info.get("entries", {}).get("allocated_objects_size")))

  @property
  def leaf_nodes(self) -> Dict[str, Dict[str, Any]]:
    """Returns only leaf nodes to prevent double-counting parent sums."""
    if self._leaf_nodes is None:
      all_names = set(self.nodes.keys())
      parent_names = set()
      for name in all_names:
        idx = name.rfind("/")
        while idx != -1:
          parent_names.add(name[:idx])
          idx = name.rfind("/", 0, idx)
      self._leaf_nodes = {
          k: v for k, v in self.nodes.items() if k not in parent_names
      }
    return self._leaf_nodes

  def get_subsystem_totals(self) -> Dict[str, int]:
    """Computes total bytes per root subsystem using leaf node allocations."""
    totals: Dict[str, int] = {}
    for name, info in self.leaf_nodes.items():
      subsys = name.split("/")[0]
      sz = self.node_size(info)
      if sz > 0:
        totals[subsys] = totals.get(subsys, 0) + sz
    return totals

  def get_total_instrumented_bytes(self) -> int:
    return sum(self.get_subsystem_totals().values())


def _parse_trace_packet(p: bytes, dumps: List[MemoryDump]):
  """Decodes memory-infra dumps from a single TracePacket payload."""
  ppos = 0
  plen = len(p)
  timestamp = None
  while ppos < plen:
    tag, ppos = read_varint(p, ppos)
    if tag is None:
      break
    fnum = tag >> 3
    wtype = tag & 7
    if fnum == 8 and wtype == 0:  # timestamp
      timestamp, ppos = read_varint(p, ppos)
    elif fnum == 73 and wtype == 2:  # memory_tracker_snapshot
      slen, ppos = read_varint(p, ppos)
      if slen is None or ppos + slen > plen:
        break
      snap_data = p[ppos:ppos + slen]
      ppos += slen

      spos = 0
      slen_total = len(snap_data)
      global_dump_id = f"dump_{len(dumps) + 1}"

      while spos < slen_total:
        stag, spos = read_varint(snap_data, spos)
        if stag is None:
          break
        sfnum = stag >> 3
        swtype = stag & 7
        if sfnum == 1 and swtype == 0:
          gid, spos = read_varint(snap_data, spos)
          if gid is not None:
            global_dump_id = hex(gid)
        elif sfnum == 3 and swtype == 2:  # process_memory_dumps
          proclen, spos = read_varint(snap_data, spos)
          if proclen is None or spos + proclen > slen_total:
            break
          proc_data = snap_data[spos:spos + proclen]
          spos += proclen

          pr_pos = 0
          pr_len = len(proc_data)
          pid = None
          dump = MemoryDump(
              dump_id=global_dump_id, timestamp=timestamp, pid=pid)

          while pr_pos < pr_len:
            ptag, pr_pos = read_varint(proc_data, pr_pos)
            if ptag is None:
              break
            pfnum = ptag >> 3
            pwtype = ptag & 7
            if pfnum == 1 and pwtype == 0:  # pid
              pid, pr_pos = read_varint(proc_data, pr_pos)
              dump.pid = pid
            elif pfnum == 2 and pwtype == 2:  # allocator_dumps
              nlen, pr_pos = read_varint(proc_data, pr_pos)
              if nlen is None or pr_pos + nlen > pr_len:
                break
              ndata = proc_data[pr_pos:pr_pos + nlen]
              pr_pos += nlen

              npos = 0
              nlen_tot = len(ndata)
              node_name = None
              size_bytes = None
              entries: Dict[str, Any] = {}

              while npos < nlen_tot:
                ntag, npos = read_varint(ndata, npos)
                if ntag is None:
                  break
                nfnum = ntag >> 3
                nwtype = ntag & 7
                if nfnum == 2 and nwtype == 2:  # absolute_name
                  namelen, npos = read_varint(ndata, npos)
                  if namelen is not None:
                    node_name = ndata[npos:npos + namelen].decode(
                        "utf-8", "ignore")
                    npos += namelen
                elif nfnum == 4 and nwtype == 0:  # size_bytes
                  size_bytes, npos = read_varint(ndata, npos)
                elif nfnum == 5 and nwtype == 2:  # entries
                  elen, npos = read_varint(ndata, npos)
                  if elen is not None:
                    edata = ndata[npos:npos + elen]
                    npos += elen
                    epos = 0
                    elen_tot = len(edata)
                    ename = None
                    eval_u = None
                    eval_s = None
                    while epos < elen_tot:
                      etag, epos = read_varint(edata, epos)
                      if etag is None:
                        break
                      efnum = etag >> 3
                      ewtype = etag & 7
                      if efnum == 1 and ewtype == 2:
                        enamelen, epos = read_varint(edata, epos)
                        if enamelen is not None:
                          ename = edata[epos:epos + enamelen].decode(
                              "utf-8", "ignore")
                          epos += enamelen
                      elif efnum == 3 and ewtype == 0:
                        eval_u, epos = read_varint(edata, epos)
                      elif efnum == 4 and ewtype == 2:
                        eslen, epos = read_varint(edata, epos)
                        if eslen is not None:
                          eval_s = edata[epos:epos + eslen].decode(
                              "utf-8", "ignore")
                          epos += eslen
                      elif ewtype == 0:
                        _, epos = read_varint(edata, epos)
                      elif ewtype == 1:
                        epos += 8
                      elif ewtype == 5:
                        epos += 4
                      elif ewtype == 2:
                        l, epos = read_varint(edata, epos)
                        if l is not None:
                          epos += l
                      else:
                        break
                    if ename:
                      entries[ename] = eval_u if eval_u is not None else eval_s
                elif nwtype == 0:
                  _, npos = read_varint(ndata, npos)
                elif nwtype == 1:
                  npos += 8
                elif nwtype == 5:
                  npos += 4
                elif nwtype == 2:
                  l, npos = read_varint(ndata, npos)
                  if l is not None:
                    npos += l
                else:
                  break

              if node_name:
                dump.add_node(node_name, size_bytes, entries)
            elif pwtype == 0:
              _, pr_pos = read_varint(proc_data, pr_pos)
            elif pwtype == 1:
              pr_pos += 8
            elif pwtype == 5:
              pr_pos += 4
            elif pwtype == 2:
              l, pr_pos = read_varint(proc_data, pr_pos)
              if l is not None:
                pr_pos += l
            else:
              break

          if dump.nodes:
            dumps.append(dump)
        elif swtype == 0:
          _, spos = read_varint(snap_data, spos)
        elif swtype == 1:
          spos += 8
        elif swtype == 5:
          spos += 4
        elif swtype == 2:
          l, spos = read_varint(snap_data, spos)
          if l is not None:
            spos += l
        else:
          break
    elif wtype == 2:
      pl, ppos = read_varint(p, ppos)
      if pl is not None:
        ppos += pl
    elif wtype == 0:
      _, ppos = read_varint(p, ppos)
    elif wtype == 1:
      ppos += 8
    elif wtype == 5:
      ppos += 4
    else:
      break


def parse_perfetto_proto(data: bytes) -> List[MemoryDump]:
  """Extracts memory dumps from a Perfetto protobuf trace."""
  pos = 0
  length = len(data)
  dumps: List[MemoryDump] = []

  while pos < length:
    tag, pos = read_varint(data, pos)
    if tag is None:
      break
    field_num = tag >> 3
    wire_type = tag & 7
    if wire_type == 2:
      plen, pos = read_varint(data, pos)
      if plen is None or pos + plen > length:
        break
      val = data[pos:pos + plen]
      pos += plen
      if field_num == 1:  # TracePacket
        _parse_trace_packet(val, dumps)
    elif wire_type == 0:
      _, pos = read_varint(data, pos)
    elif wire_type == 1:
      pos += 8
    elif wire_type == 5:
      pos += 4
    else:
      break

  return dumps


def parse_json_trace(json_data: Any) -> List[MemoryDump]:
  """Extracts memory dumps from a legacy Chromium JSON trace."""
  events = []
  if isinstance(json_data, dict):
    events = json_data.get("traceEvents", [])
  elif isinstance(json_data, list):
    events = json_data

  dumps: List[MemoryDump] = []
  for e in events:
    args = e.get("args", {})
    if "dumps" in args and "allocators" in args["dumps"]:
      allocators = args["dumps"]["allocators"]
      if not allocators:
        continue
      dump_id = hex(args["dumps"].get("id", len(dumps) + 1))
      dump = MemoryDump(
          dump_id=dump_id,
          timestamp=e.get("ts"),
          pid=e.get("pid"),
      )
      for name, alloc in allocators.items():
        attrs = alloc.get("attrs", {})
        size_bytes = None
        entries = {}
        for attr_k, attr_v in attrs.items():
          val = attr_v.get("value") if isinstance(attr_v, dict) else attr_v
          val_int = to_int(val)
          if attr_k in ("size", "allocated_objects_size") and val_int > 0:
            size_bytes = val_int
          entries[attr_k] = val_int if val_int > 0 else val
        dump.add_node(name, size_bytes, entries)
      dumps.append(dump)

  return dumps


def load_trace(file_path: str) -> List[MemoryDump]:
  """Loads and parses memory dumps from either Perfetto or JSON trace."""
  if not os.path.exists(file_path):
    raise FileNotFoundError(f"Trace file not found: {file_path}")

  # Check if gzipped
  with open(file_path, "rb") as f:
    header = f.read(2)

  is_gzip = header.startswith(b"\x1f\x8b")
  if is_gzip:
    with gzip.open(file_path, "rb") as f:
      content = f.read()
  else:
    with open(file_path, "rb") as f:
      content = f.read()

  # Check if JSON
  first_char = content.lstrip()[:1]
  if first_char in (b"{", b"["):
    try:
      parsed_json = json.loads(content.decode("utf-8"))
      dumps = parse_json_trace(parsed_json)
      if dumps:
        return dumps
    except Exception:
      pass

  # Try Perfetto protobuf parser
  dumps = parse_perfetto_proto(content)
  if dumps:
    return dumps

  # If empty, try JSON decoding as fallback
  try:
    parsed_json = json.loads(content.decode("utf-8"))
    return parse_json_trace(parsed_json)
  except Exception:
    pass

  return []


def format_bytes(b: int) -> str:
  """Formats byte values into human-readable strings."""
  if b is None or b < 0:
    return "0 B"
  kb = b / 1024.0
  mb = kb / 1024.0
  if mb >= 10.0:
    return f"{mb:7.2f} MB"
  elif mb >= 1.0:
    return f"{mb:7.3f} MB"
  elif kb >= 1.0:
    return f"{kb:7.1f} KB"
  else:
    return f"{b:5d} B"


# ==============================================================================
# Subsystem Analysis & Formatting
# ==============================================================================


def analyze_v8(dump: MemoryDump) -> Dict[str, Any]:
  """Analyzes V8 JavaScript Engine allocations."""
  nodes = dump.nodes
  v8_nodes = {k: v for k, v in nodes.items() if k.startswith("v8/")}

  spaces: Dict[str, int] = {}
  committed_total = 0
  allocated_total = 0

  for k, v in v8_nodes.items():
    sz = to_int(v["size"]) or to_int(v["entries"].get("allocated_objects_size"))
    if "heap/" in k:
      space_name = k.split("heap/")[-1].split("/")[0]
      spaces[space_name] = spaces.get(space_name, 0) + sz
    elif "malloc" in k:
      spaces["malloc"] = sz
    elif "global_handles" in k:
      spaces["global_handles"] = sz

    alloc_sz = to_int(v["entries"].get("allocated_objects_size"))
    if alloc_sz > 0:
      allocated_total += alloc_sz
    comm_sz = to_int(v["entries"].get("committed_size") or
                     v["entries"].get("virtual_size"))
    if comm_sz > 0:
      committed_total += comm_sz

  leaf_total = sum(
      to_int(v["size"]) or to_int(v["entries"].get("allocated_objects_size"))
      for k, v in dump.leaf_nodes.items()
      if k.startswith("v8/"))

  return {
      "total_bytes": leaf_total,
      "allocated_objects_bytes": allocated_total,
      "committed_bytes": committed_total,
      "spaces": spaces,
  }


def analyze_blink(dump: MemoryDump) -> Dict[str, Any]:
  """Analyzes Blink DOM, GC Heap, and Web Caches."""
  nodes = dump.nodes
  gc_nodes = {k: v for k, v in nodes.items() if k.startswith("blink_gc/")}
  dom_nodes = {k: v for k, v in nodes.items() if k.startswith("blink_objects/")}

  spaces: Dict[str, int] = {}
  for k, v in gc_nodes.items():
    if "heap/" in k:
      sub = k.split("heap/")[-1].split("/")[0]
      sz = to_int(v["size"]) or to_int(
          v["entries"].get("allocated_objects_size"))
      if sz and sub not in spaces:
        spaces[sub] = sz

  dom_counts: Dict[str, Any] = {}
  for k, v in dom_nodes.items():
    obj_name = k.split("blink_objects/")[-1]
    count = v["entries"].get("object_count") or v["size"]
    if count is not None:
      dom_counts[obj_name] = count

  web_cache_bytes = sum(
      to_int(v["size"])
      for k, v in dump.leaf_nodes.items()
      if k.startswith("web_cache/"))
  gc_total = sum(
      to_int(v["size"]) or to_int(v["entries"].get("allocated_objects_size"))
      for k, v in dump.leaf_nodes.items()
      if k.startswith("blink_gc/"))

  return {
      "total_gc_bytes": gc_total,
      "web_cache_bytes": web_cache_bytes,
      "gc_spaces": spaces,
      "dom_objects": dom_counts,
  }


def analyze_skia(dump: MemoryDump) -> Dict[str, Any]:
  """Analyzes Skia 2D Graphics and Font caches."""
  skia_nodes = {k: v for k, v in dump.nodes.items() if k.startswith("skia/")}

  glyph_cache_bytes = 0
  glyph_count = 0
  fonts: Dict[str, int] = {}
  gpu_res_bytes = 0

  for k, v in skia_nodes.items():
    sz = dump.node_size(v)
    if "sk_glyph_cache" in k:
      if k == "skia/sk_glyph_cache":
        glyph_cache_bytes = sz
        glyph_count = to_int(v["entries"].get("glyph_count", 0))
      else:
        parts = k.split("/")
        if len(parts) >= 3:
          font_name = parts[2]
          fonts[font_name] = fonts.get(font_name, 0) + sz
    elif "gpu_resources" in k:
      gpu_res_bytes += sz

  leaf_total = sum(
      dump.node_size(v)
      for k, v in dump.leaf_nodes.items()
      if k.startswith("skia/"))

  return {
      "total_bytes": leaf_total,
      "glyph_cache_bytes": glyph_cache_bytes,
      "glyph_count": glyph_count,
      "gpu_resources_bytes": gpu_res_bytes,
      "top_fonts": fonts,
  }


def analyze_cc(dump: MemoryDump) -> Dict[str, Any]:
  """Analyzes Chrome Compositor (CC) tile memory and resources."""
  tile_bytes = 0
  resource_bytes = 0
  tile_count = 0

  for k, v in dump.leaf_nodes.items():
    if not k.startswith("cc/"):
      continue
    sz = dump.node_size(v)
    if "tile_memory" in k:
      tile_bytes += sz
      tile_count += 1
    elif "resource_memory" in k:
      resource_bytes += sz

  leaf_total = sum(
      dump.node_size(v)
      for k, v in dump.leaf_nodes.items()
      if k.startswith("cc/"))

  return {
      "total_bytes": leaf_total,
      "tile_memory_bytes": tile_bytes,
      "tile_count": tile_count,
      "resource_memory_bytes": resource_bytes,
  }


def analyze_gpu(dump: MemoryDump) -> Dict[str, Any]:
  """Analyzes GPU shared image mailboxes and textures."""
  shared_images = []
  for k, v in dump.nodes.items():
    if "gpu/shared_images" in k and "mailbox" in k:
      sz = dump.node_size(v)
      mb_name = k.split("mailbox_")[-1]
      entries = v.get("entries", {})
      shared_images.append({
          "mailbox": mb_name,
          "size": sz,
          "dimensions": entries.get("dimensions"),
          "format": entries.get("format"),
          "purgeable": entries.get("purgeable", 0),
      })

  shared_images.sort(key=lambda x: x["size"], reverse=True)
  leaf_total = sum(
      dump.node_size(v)
      for k, v in dump.leaf_nodes.items()
      if k.startswith("gpu/"))

  return {
      "total_bytes": leaf_total,
      "shared_images_count": len(shared_images),
      "shared_images_bytes": sum(img["size"] for img in shared_images),
      "top_mailboxes": shared_images[:10],
  }


def analyze_partition_alloc(dump: MemoryDump) -> Dict[str, Any]:
  """Analyzes PartitionAlloc and system malloc."""
  partitions: Dict[str, int] = {}
  allocated_objects = 0

  for k, v in dump.nodes.items():
    if k.startswith("partition_alloc/partitions/"):
      parts = k.split("partitions/")[-1].split("/")
      if len(parts) == 1:
        part_name = parts[0]
        sz = dump.node_size(v)
        partitions[part_name] = sz
    elif k == "partition_alloc/allocated_objects":
      allocated_objects = dump.node_size(v)

  pa_total = sum(
      dump.node_size(v)
      for k, v in dump.leaf_nodes.items()
      if k.startswith("partition_alloc/"))
  malloc_total = sum(
      dump.node_size(v)
      for k, v in dump.leaf_nodes.items()
      if k.startswith("malloc"))

  return {
      "partition_alloc_total_bytes": pa_total,
      "malloc_total_bytes": malloc_total,
      "allocated_objects_bytes": allocated_objects,
      "partitions": partitions,
  }


# ==============================================================================
# Report Generators
# ==============================================================================


def generate_report(
    dump: MemoryDump,
    verbose: bool = False,
    compare_dump: Optional[MemoryDump] = None,
) -> str:
  """Generates a complete terminal text report for a memory dump."""
  lines: List[str] = []
  w = 78

  lines.append("=" * w)
  title = f" COBALT CHROMIUM MEMORY-INFRA ANALYSIS (Dump {dump.dump_id}) "
  lines.append(f"{title:^{w}}")
  lines.append("=" * w)

  if dump.pid:
    lines.append(f"PID: {dump.pid}")
  if dump.timestamp:
    lines.append(f"Timestamp: {dump.timestamp} µs")

  totals = dump.get_subsystem_totals()
  total_mem = sum(totals.values())

  lines.append("-" * w)
  lines.append(
      f"{'Subsystem':<26} {'Size (MB)':>12} {'Size (KB)':>14} {'% Total':>10}")
  lines.append("-" * w)

  compare_totals = compare_dump.get_subsystem_totals() if compare_dump else {}

  for subsys, b in sorted(totals.items(), key=lambda x: x[1], reverse=True):
    mb = b / (1024 * 1024)
    kb = b / 1024.0
    pct = (b / total_mem * 100) if total_mem else 0

    diff_str = ""
    if compare_dump:
      prev_b = compare_totals.get(subsys, 0)
      delta = b - prev_b
      delta_mb = delta / (1024 * 1024)
      if abs(delta) >= 1024:
        sign = "+" if delta > 0 else ""
        diff_str = f" ({sign}{delta_mb:.2f} MB)"

    lines.append(
        f"{subsys:<26} {mb:>11.2f}M {kb:>13.1f}K {pct:>9.1f}%{diff_str}")

  lines.append("-" * w)
  total_mb = total_mem / (1024 * 1024)
  total_kb = total_mem / 1024.0
  lines.append(
      f"{'TOTAL INSTRUMENTED':<26} {total_mb:>11.2f}M {total_kb:>13.1f}K"
      "    100.0%")
  lines.append("=" * w)

  # 1. V8 Breakdown
  v8_data = analyze_v8(dump)
  lines.append("\n" + "-" * w)
  lines.append(
      f"1. V8 JAVASCRIPT ENGINE: {format_bytes(v8_data['total_bytes'])}")
  lines.append("-" * w)
  for space, sz in sorted(
      v8_data["spaces"].items(), key=lambda x: x[1], reverse=True):
    lines.append(f"  • {space:<32}: {format_bytes(sz)}")

  # 2. Blink Breakdown
  blink_data = analyze_blink(dump)
  lines.append("\n" + "-" * w)
  lines.append(
      f"2. BLINK & DOM: GC Heap {format_bytes(blink_data['total_gc_bytes'])},"
      f" WebCache {format_bytes(blink_data['web_cache_bytes'])}")
  lines.append("-" * w)
  if blink_data["gc_spaces"]:
    lines.append("  GC Page Spaces:")
    for sp, sz in sorted(
        blink_data["gc_spaces"].items(), key=lambda x: x[1], reverse=True):
      lines.append(f"    - {sp:<30}: {format_bytes(sz)}")
  if blink_data["dom_objects"]:
    lines.append("  DOM Object Counts:")
    for obj, count in sorted(blink_data["dom_objects"].items()):
      lines.append(f"    - {obj:<30}: {count}")

  # 3. Skia Breakdown
  skia_data = analyze_skia(dump)
  lines.append("\n" + "-" * w)
  lines.append(
      f"3. SKIA GRAPHICS: {format_bytes(skia_data['total_bytes'])} (Glyphs:"
      f" {format_bytes(skia_data['glyph_cache_bytes'])},"
      f" {skia_data['glyph_count']} glyphs)")
  lines.append("-" * w)
  if skia_data["gpu_resources_bytes"]:
    lines.append(f"  • GPU Resources                 :"
                 f" {format_bytes(skia_data['gpu_resources_bytes'])}")
  if skia_data["top_fonts"]:
    lines.append("  Top Cached Fonts:")
    for font, sz in sorted(
        skia_data["top_fonts"].items(), key=lambda x: x[1], reverse=True)[:6]:
      lines.append(f"    - {font:<30}: {format_bytes(sz)}")

  # 4. CC Breakdown
  cc_data = analyze_cc(dump)
  lines.append("\n" + "-" * w)
  lines.append(
      f"4. CHROME COMPOSITOR (CC): {format_bytes(cc_data['total_bytes'])}")
  lines.append("-" * w)
  lines.append(
      f"  • Rasterized Tile Memory         :"
      f" {format_bytes(cc_data['tile_memory_bytes'])} ({cc_data['tile_count']}"
      " active tiles)")
  lines.append(f"  • Composited Layer Resources     :"
               f" {format_bytes(cc_data['resource_memory_bytes'])}")

  # 5. GPU Breakdown
  gpu_data = analyze_gpu(dump)
  lines.append("\n" + "-" * w)
  lines.append(
      f"5. GPU & SHARED IMAGES: {format_bytes(gpu_data['total_bytes'])}"
      f" ({gpu_data['shared_images_count']} texture mailboxes)")
  lines.append("-" * w)
  lines.append(f"  • Shared Images Total            :"
               f" {format_bytes(gpu_data['shared_images_bytes'])}")
  if verbose or len(gpu_data["top_mailboxes"]) > 0:
    lines.append("  Largest Texture Mailboxes (Decoded Images/Canvases):")
    for img in gpu_data["top_mailboxes"][:8]:
      dim = img["dimensions"] or "unknown"
      fmt = img["format"] or ""
      extra = f"[{dim} {fmt}]".strip()
      lines.append(
          f"    - {img['mailbox'][:32]:<34}: {format_bytes(img['size']):>10} "
          f" {extra}")

  # 6. PartitionAlloc & Malloc
  pa_data = analyze_partition_alloc(dump)
  lines.append("\n" + "-" * w)
  lines.append(
      f"6. PARTITION_ALLOC & MALLOC: PA"
      f" {format_bytes(pa_data['partition_alloc_total_bytes'])}, Malloc"
      f" {format_bytes(pa_data['malloc_total_bytes'])}")
  lines.append("-" * w)
  if pa_data["partitions"]:
    lines.append("  Partitions Breakdown:")
    for p_name, sz in sorted(
        pa_data["partitions"].items(), key=lambda x: x[1], reverse=True):
      lines.append(f"    - {p_name:<30}: {format_bytes(sz)}")

  # 7. Other Categories
  other_cats = {
      k: v for k, v in totals.items() if k not in (
          "v8",
          "blink_gc",
          "blink_objects",
          "skia",
          "cc",
          "gpu",
          "partition_alloc",
          "malloc",
      )
  }
  if other_cats:
    lines.append("\n" + "-" * w)
    lines.append("7. OTHER MEMORY-INFRA CATEGORIES")
    lines.append("-" * w)
    for k, v in sorted(other_cats.items(), key=lambda x: x[1], reverse=True):
      lines.append(f"  • {k:<32}: {format_bytes(v)}")

  lines.append("\n" + "=" * w)
  return "\n".join(lines)


def generate_markdown(dump: MemoryDump,
                      compare_dump: Optional[MemoryDump] = None) -> str:
  """Generates a GitHub-flavored Markdown report."""
  lines: List[str] = []
  lines.append("# Cobalt Memory-Infra Analysis Report")
  lines.append(f"**Dump ID:** `{dump.dump_id}`  ")
  if dump.pid:
    lines.append(f"**Process PID:** `{dump.pid}`  ")
  if dump.timestamp:
    lines.append(f"**Timestamp:** `{dump.timestamp} µs`  ")
  lines.append("")

  totals = dump.get_subsystem_totals()
  total_mem = sum(totals.values())
  compare_totals = compare_dump.get_subsystem_totals() if compare_dump else {}

  lines.append("## Subsystem Memory Overview\n")
  if compare_dump:
    lines.append("| Subsystem | Size | % Total | Delta (Growth) |")
    lines.append("| :--- | :--- | :--- | :--- |")
  else:
    lines.append("| Subsystem | Size | % Total |")
    lines.append("| :--- | :--- | :--- |")

  for subsys, b in sorted(totals.items(), key=lambda x: x[1], reverse=True):
    pct = (b / total_mem * 100) if total_mem else 0
    size_str = format_bytes(b).strip()
    if compare_dump:
      prev_b = compare_totals.get(subsys, 0)
      delta = b - prev_b
      sign = "+" if delta > 0 else ""
      delta_str = f"{sign}{format_bytes(delta).strip()}"
      lines.append(f"| **{subsys}** | {size_str} | {pct:.1f}% | {delta_str} |")
    else:
      lines.append(f"| **{subsys}** | {size_str} | {pct:.1f}% |")

  lines.append(
      f"| **Total Instrumented** | **{format_bytes(total_mem).strip()}** |"
      " **100.0%** | |\n")

  # V8 Section
  v8_data = analyze_v8(dump)
  v8_bytes_str = format_bytes(v8_data["total_bytes"]).strip()
  lines.append(f"### 1. V8 JavaScript Engine ({v8_bytes_str})\n")
  for sp, sz in sorted(
      v8_data["spaces"].items(), key=lambda x: x[1], reverse=True):
    lines.append(f"- **{sp}**: {format_bytes(sz).strip()}")
  lines.append("")

  # Blink Section
  blink_data = analyze_blink(dump)
  gc_str = format_bytes(blink_data["total_gc_bytes"]).strip()
  wc_str = format_bytes(blink_data["web_cache_bytes"]).strip()
  lines.append(f"### 2. Blink & DOM (GC Heap: {gc_str}, WebCache: {wc_str})\n")
  for sp, sz in sorted(
      blink_data["gc_spaces"].items(), key=lambda x: x[1], reverse=True):
    lines.append(f"- **{sp}**: {format_bytes(sz).strip()}")
  lines.append("")

  # Skia Section
  skia_data = analyze_skia(dump)
  skia_bytes_str = format_bytes(skia_data["total_bytes"]).strip()
  glyph_bytes_str = format_bytes(skia_data["glyph_cache_bytes"]).strip()
  glyph_count = skia_data["glyph_count"]
  lines.append(f"### 3. Skia Graphics ({skia_bytes_str})\n")
  lines.append(f"- **Glyph Cache**: {glyph_bytes_str} ({glyph_count} glyphs)")
  if skia_data["gpu_resources_bytes"]:
    gpu_res_str = format_bytes(skia_data["gpu_resources_bytes"]).strip()
    lines.append(f"- **GPU Resources**: {gpu_res_str}")
  lines.append("")

  # CC Section
  cc_data = analyze_cc(dump)
  cc_bytes_str = format_bytes(cc_data["total_bytes"]).strip()
  tile_bytes_str = format_bytes(cc_data["tile_memory_bytes"]).strip()
  tile_count = cc_data["tile_count"]
  res_bytes_str = format_bytes(cc_data["resource_memory_bytes"]).strip()
  lines.append(f"### 4. Chrome Compositor (CC) ({cc_bytes_str})\n")
  lines.append(f"- **Rasterized Tiles**: {tile_bytes_str} ({tile_count} tiles)")
  lines.append(f"- **Layer Resources**: {res_bytes_str}")
  lines.append("")

  # GPU Section
  gpu_data = analyze_gpu(dump)
  gpu_bytes_str = format_bytes(gpu_data["total_bytes"]).strip()
  si_bytes_str = format_bytes(gpu_data["shared_images_bytes"]).strip()
  si_count = gpu_data["shared_images_count"]
  lines.append(f"### 5. GPU & Shared Images ({gpu_bytes_str})\n")
  lines.append(
      f"- **Shared Images Total**: {si_bytes_str} ({si_count} mailboxes)")
  if gpu_data["top_mailboxes"]:
    lines.append("\n*Largest Texture Mailboxes:*")
    for img in gpu_data["top_mailboxes"][:5]:
      dim = img["dimensions"] or "unknown"
      fmt = img["format"] or ""
      extra = f"[{dim} {fmt}]".strip()
      sz_str = format_bytes(img["size"]).strip()
      lines.append(f"- `{img['mailbox'][:32]}`: {sz_str} {extra}".rstrip())
  lines.append("")

  # PartitionAlloc Section
  pa_data = analyze_partition_alloc(dump)
  pa_bytes_str = format_bytes(pa_data["partition_alloc_total_bytes"]).strip()
  malloc_bytes_str = format_bytes(pa_data["malloc_total_bytes"]).strip()
  lines.append("### 6. PartitionAlloc & Malloc "
               f"(PA: {pa_bytes_str}, Malloc: {malloc_bytes_str})\n")
  if pa_data["partitions"]:
    lines.append("*Partitions Breakdown:*")
    for p_name, sz in sorted(
        pa_data["partitions"].items(), key=lambda x: x[1], reverse=True):
      lines.append(f"- **{p_name}**: {format_bytes(sz).strip()}")
    lines.append("")

  # Other Categories Section
  other_cats = {
      k: v for k, v in totals.items() if k not in (
          "v8",
          "blink_gc",
          "blink_objects",
          "skia",
          "cc",
          "gpu",
          "partition_alloc",
          "malloc",
      )
  }
  if other_cats:
    lines.append("### 7. Other Memory-Infra Categories\n")
    for k, v in sorted(other_cats.items(), key=lambda x: x[1], reverse=True):
      lines.append(f"- **{k}**: {format_bytes(v).strip()}")
    lines.append("")

  return "\n".join(lines)


# ==============================================================================
# CLI and Live Capture
# ==============================================================================


async def live_capture(
    host: str = "localhost",
    port: int = 9222,
    duration: float = 3.0,
    preset: str = "rdk",
) -> List[MemoryDump]:
  """Captures a temporary trace via CDP and returns its parsed memory dumps."""
  # pylint: disable=import-outside-toplevel
  tools_dir = os.path.dirname(os.path.abspath(__file__))
  if tools_dir not in sys.path:
    sys.path.insert(0, tools_dir)
  try:
    from cdp_meminfra_tracing import capture_meminfra_trace
  except ImportError:
    try:
      from cobalt.tools.performance.memory.cdp_meminfra_tracing import capture_meminfra_trace
    except ImportError:
      from cobalt.tools.cdp_meminfra_tracing import capture_meminfra_trace

  with tempfile.NamedTemporaryFile(
      suffix=".perfetto-trace", delete=False) as tf:
    temp_path = tf.name

  try:
    print(
        f"[CDP] Capturing {duration}s live memory trace from {host}:{port}...")
    await capture_meminfra_trace(
        host=host,
        port=port,
        duration=duration,
        output_path=temp_path,
        preset=preset,
        level_of_detail="detailed",
    )
    return load_trace(temp_path)
  finally:
    if os.path.exists(temp_path):
      os.unlink(temp_path)


async def live_capture_and_analyze(
    host: str = "localhost",
    port: int = 9222,
    duration: float = 3.0,
    preset: str = "rdk",
) -> str:
  """Captures a temporary trace via CDP and returns its analysis."""
  dumps = await live_capture(
      host=host, port=port, duration=duration, preset=preset)
  if not dumps:
    return "Error: No memory dumps found in captured trace."
  return generate_report(dumps[-1])


def find_latest_trace() -> Optional[str]:
  """Searches common directories for the newest trace file."""
  candidates = []
  search_dirs = [".", "/tmp", "rdk_analysis"]
  for d in search_dirs:
    if not os.path.exists(d):
      continue
    for f in os.listdir(d):
      if f.endswith(
          (".perfetto-trace", ".pftrace", ".json.gz")) and "trace" in f.lower():
        p = os.path.join(d, f)
        candidates.append((os.path.getmtime(p), p))
  if candidates:
    candidates.sort(reverse=True)
    return candidates[0][1]
  return None


def main():
  parser = argparse.ArgumentParser(
      description=(
          "Analyze Cobalt memory-infra dumps across V8, Blink, Skia, CC, GPU"
          " and other categories."))
  parser.add_argument(
      "trace_file",
      type=str,
      nargs="?",
      default=None,
      help=(
          "Path to trace file (.perfetto-trace, .pftrace, .json, .json.gz). If"
          " omitted, uses latest trace found."),
  )
  parser.add_argument(
      "-v",
      "--verbose",
      action="store_true",
      help=(
          "Include granular leaf breakdowns (individual textures, fonts, etc.)"
      ),
  )
  parser.add_argument(
      "--dump",
      type=int,
      default=-1,
      help="Dump index to analyze (1-based, default: -1 for latest/best dump)",
  )
  parser.add_argument(
      "--compare",
      action="store_true",
      help="Compare initial vs final dump to show growth delta",
  )
  parser.add_argument(
      "-o",
      "--output",
      type=str,
      default="",
      help="Output report to a Markdown file",
  )
  parser.add_argument(
      "--markdown",
      action="store_true",
      help="Print report in Markdown format to stdout",
  )
  parser.add_argument(
      "--json",
      action="store_true",
      dest="output_json",
      help="Output analysis in JSON format",
  )
  parser.add_argument(
      "--live",
      action="store_true",
      help="Capture a live trace from localhost:9222 and analyze immediately",
  )
  parser.add_argument(
      "--host",
      type=str,
      default="localhost",
      help="DevTools host for --live (default: localhost)",
  )
  parser.add_argument(
      "--port",
      type=int,
      default=9222,
      help="DevTools port for --live (default: 9222)",
  )

  args = parser.parse_args()

  if args.live:
    try:
      dumps = asyncio.run(live_capture(host=args.host, port=args.port))
    except Exception as e:
      print(f"[CDP] Live capture failed: {e}", file=sys.stderr)
      sys.exit(1)
    if not dumps:
      print(
          "Error: No memory-infra dumps found in live capture from "
          f"{args.host}:{args.port}.",
          file=sys.stderr,
      )
      sys.exit(1)
    print(f"[Analyze] Loaded {len(dumps)} memory dump(s) from live capture")
  else:
    target_file = args.trace_file
    if not target_file:
      target_file = find_latest_trace()
      if not target_file:
        print(
            "Error: No trace file provided and no existing traces found.",
            file=sys.stderr,
        )
        print("Usage: analyze_memory_trace.py <trace_file>", file=sys.stderr)
        sys.exit(1)
      print(f"[Analyze] Using latest discovered trace: {target_file}")

    try:
      dumps = load_trace(target_file)
    except Exception as e:
      print(f"Error reading trace file: {e}", file=sys.stderr)
      sys.exit(1)

    if not dumps:
      print(
          f"Error: No memory-infra dumps found in {target_file}.",
          file=sys.stderr,
      )
      print(
          "Make sure the trace was recorded with"
          " 'disabled-by-default-memory-infra' enabled.",
          file=sys.stderr,
      )
      sys.exit(1)

    print(f"[Analyze] Loaded {len(dumps)} memory dump(s) from {target_file}")

  # Select dump: default to the dump with the most detailed allocator nodes
  if args.dump > 0 and args.dump <= len(dumps):
    selected_dump = dumps[args.dump - 1]
  else:
    selected_dump = max(dumps, key=lambda d: len(d.nodes))

  compare_dump = None
  if args.compare and len(dumps) > 1:
    # Compare against the initial dump
    compare_dump = min(dumps, key=lambda d: d.timestamp or 0)
    if compare_dump == selected_dump:
      # If selected is the first, compare against the last
      compare_dump = dumps[-1]

  if args.output_json:
    v8 = analyze_v8(selected_dump)
    blink = analyze_blink(selected_dump)
    skia = analyze_skia(selected_dump)
    cc = analyze_cc(selected_dump)
    gpu = analyze_gpu(selected_dump)
    pa = analyze_partition_alloc(selected_dump)
    totals = selected_dump.get_subsystem_totals()
    out = {
        "dump_id": selected_dump.dump_id,
        "timestamp": selected_dump.timestamp,
        "pid": selected_dump.pid,
        "totals": totals,
        "v8": v8,
        "blink": blink,
        "skia": skia,
        "cc": cc,
        "gpu": gpu,
        "partition_alloc": pa,
    }
    print(json.dumps(out, indent=2))
    return

  if args.markdown:
    md = generate_markdown(selected_dump, compare_dump)
    print(md)
  elif args.output:
    md = generate_markdown(selected_dump, compare_dump)
    with open(args.output, "w", encoding="utf-8") as f:
      f.write(md)
    print(f"[Analyze] Report written to {args.output}")
  else:
    report = generate_report(
        selected_dump, verbose=args.verbose, compare_dump=compare_dump)
    print(report)


if __name__ == "__main__":
  main()
