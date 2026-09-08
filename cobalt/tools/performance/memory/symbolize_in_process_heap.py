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
"""Cobalt DWARF Callstack Symbolizer.

This script parses and symbolizes hexadecimal program counters inside
in-process heap traces using local unstripped binaries and llvm-symbolizer.
"""

import argparse
import bisect
import collections
import json
import os
import subprocess
import sys

_SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
if _SCRIPT_DIR not in sys.path:
  sys.path.insert(0, _SCRIPT_DIR)

try:
  import convert_heaps_v2_to_html as html_converter
except ImportError:
  html_converter = None


def find_repo_root():
  """Walks upwards to find the Cobalt repository root."""
  curr = os.path.realpath(os.getcwd())
  while curr:
    if os.path.exists(os.path.join(curr, "cobalt")) and os.path.exists(
        os.path.join(curr, "third_party")):
      return curr
    parent = os.path.dirname(curr)
    if parent == curr:
      break
    curr = parent
  return None


def resolve_symbolizer_path(user_symbolizer_path, repo_root):
  """Auto-detects toolchain llvm-symbolizer or falls back to system PATH.

  Both Android TV and RDK builds use Clang from third_party/llvm-build/.
  Using the matching toolchain symbolizer guarantees DWARF 5 compatibility.
  """
  if user_symbolizer_path:
    if (user_symbolizer_path != "llvm-symbolizer" and
        not os.path.exists(user_symbolizer_path)):
      print(
          f"Warning: Specified symbolizer not found at {user_symbolizer_path}."
          " Falling back to system 'llvm-symbolizer'.")
      return "llvm-symbolizer"
    return user_symbolizer_path

  if repo_root:
    toolchain_sym = os.path.join(
        repo_root, "third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer")
    if os.path.exists(toolchain_sym):
      rel_path = os.path.relpath(toolchain_sym, repo_root)
      print(f"💡 Using toolchain prebuilt symbolizer: {rel_path}")
      return toolchain_sym

  print("💡 Toolchain symbolizer not found. "
        "Falling back to system 'llvm-symbolizer'.")
  return "llvm-symbolizer"


def resolve_unstripped_library_path(lib_path):
  """Auto-detects unstripped binary if a stripped one was provided."""
  lib_path = os.path.abspath(lib_path)
  if not os.path.exists(lib_path):
    print(f"Error: Specified library not found at: {lib_path}")
    sys.exit(1)

  # Check standard Chromium / Cobalt unstripped output directory
  unstripped_candidate = os.path.join(
      os.path.dirname(lib_path), "lib.unstripped", os.path.basename(lib_path))
  if os.path.exists(unstripped_candidate):
    print(f"💡 Found unstripped binary: {unstripped_candidate}")
    return unstripped_candidate

  return lib_path


def get_elf_text_virtaddr(lib_path):
  """Extracts the virtual address of the executable .text LOAD segment.

  When shared libraries (e.g. libcobalt.so) are compiled, read-only headers and
  metadata precede executable code, giving the .text segment a non-zero virtual
  address (e.g. 0x0113bb80).
  """
  try:
    readelf_proc = subprocess.run(["readelf", "-l", lib_path],
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE,
                                  text=True,
                                  check=True)
    for line in readelf_proc.stdout.splitlines():
      if "LOAD" in line and "E" in line:
        parts = line.split()
        if len(parts) >= 3:
          try:
            return int(parts[2], 16)
          except ValueError:
            pass
  except (subprocess.SubprocessError, OSError) as ex:
    print(f"Warning: Could not read ELF program headers via readelf: {ex}")
  return 0


def find_named_mapping_base_address(heaps_dumps, lib_path):
  """Finds library base load address via named file paths in process_mmaps.

  TARGET PLATFORM: Android TV and Desktop Linux.

  On platforms where the OS dynamic linker (Android Bionic linker64 / Linux
  ld.so) loads the shared library, the Linux kernel populates the mapped file
  ('mf') and description file ('df') fields in /proc/self/maps. When multiple
  segments exist (code, rodata, data), the base address is the lowest start
  address (sa) across all matching regions.

  Returns:
    tuple of (base_address, size) if found, otherwise (None, None).
  """
  lib_name = os.path.basename(lib_path)
  for dump in heaps_dumps:
    vm_regions = dump.get("process_mmaps", {}).get("vm_regions", [])
    matching_regions = []
    for r in vm_regions:
      mf = r.get("mf") or ""
      df = r.get("df") or ""
      if lib_name in mf or lib_name in df:
        sa = int(r.get("sa") or "0", 16)
        sz = int(r.get("sz") or "0", 16)
        matching_regions.append((sa, sz))

    if matching_regions:
      min_sa = min(sa for sa, _ in matching_regions)
      max_ea = max(sa + sz for sa, sz in matching_regions)
      total_size = max_ea - min_sa
      return min_sa, total_size

  return None, None


def find_evergreen_anonymous_mapping_base_address(heaps_dumps,
                                                  pc_to_string_entries,
                                                  lib_path):
  """Calculates library base load address for in-memory anonymous mappings.

  TARGET PLATFORM: RDK (Cobalt Evergreen).

  On RDK, Cobalt Evergreen does NOT use the OS dynamic linker. Instead,
  Evergreen's custom in-memory ELF loader (elf_loader) allocates anonymous
  memory (mmap) and decompresses libcobalt.so directly into RAM. Because the
  kernel is unaware of any backing file, /proc/self/maps has an empty
  filename ('mf': '').

  This function:
  1. Reads the ELF .text segment's virtual address via readelf (e.g.
     0x0113bb80).
  2. Scans anonymous vm_regions and correlates them with the raw hardware
     Program Counters (PCs) extracted from the heap profile to locate the
     hosting executable code segment using binary search (bisect).
  3. Reverse-engineers the true ELF base load address:
     base_address = segment_start_address - elf_text_virtaddr

  Returns:
    tuple of (base_address, size) if found, otherwise (None, None).
  """
  if not pc_to_string_entries:
    return None, None

  elf_text_virtaddr = get_elf_text_virtaddr(lib_path)
  all_pc_ints = sorted(int(p, 16) for p in pc_to_string_entries)

  best_region = None
  best_count = 0

  for dump in heaps_dumps:
    vm_regions = dump.get("process_mmaps", {}).get("vm_regions", [])
    for r in vm_regions:
      sa = int(r.get("sa") or "0", 16)
      sz = int(r.get("sz") or "0", 16)
      ea = sa + sz
      left_idx = bisect.bisect_left(all_pc_ints, sa)
      right_idx = bisect.bisect_left(all_pc_ints, ea)
      matching_count = right_idx - left_idx
      if matching_count > best_count:
        best_count = matching_count
        best_region = (sa, sz)

  if best_region and best_count > 0:
    segment_sa, segment_sz = best_region
    base_address = segment_sa - elf_text_virtaddr
    size = segment_sz + elf_text_virtaddr
    print(f"🎉 Correlated {best_count} PCs to Evergreen dynamic mapping "
          f"at 0x{segment_sa:x} (ELF VirtAddr: 0x{elf_text_virtaddr:x}, "
          f"Base: 0x{base_address:x})")
    return base_address, size

  return None, None


def extract_lib_base_address(heaps_dumps, pc_to_string_entries, lib_path):
  """Finds the base load address using platform-specific strategies.

  1. Strategy 1 (Android TV / Linux): Named file in process_mmaps.
  2. Strategy 2 (RDK Evergreen): Anonymous memory PC correlation.
  """
  # 1. Try standard Android TV / Linux named mapping
  base_address, size = find_named_mapping_base_address(heaps_dumps, lib_path)

  # 2. Fall back to RDK Evergreen anonymous memory correlation
  if base_address is None:
    base_address, size = find_evergreen_anonymous_mapping_base_address(
        heaps_dumps, pc_to_string_entries, lib_path)

  if base_address is None:
    print(f"Error: Could not find {os.path.basename(lib_path)} "
          "executable mapping in trace!")
    sys.exit(1)

  return base_address, size


def extract_memory_snapshots_and_pcs(trace_data):
  """Extracts memory snapshot dumps and groups raw PC string dictionary entries.

  Returns:
    tuple of (heaps_dumps, pc_to_string_entries)
    - heaps_dumps: List of dumps dictionaries containing 'heaps_v2'.
    - pc_to_string_entries: Map of hex_pc -> list of entry dict references.
  """
  raw_events = trace_data if isinstance(trace_data, list) else trace_data.get(
      "traceEvents", [])

  heaps_dumps = []
  for event in raw_events:
    if not isinstance(event, dict):
      continue
    args = event.get("args")
    if not isinstance(args, dict):
      continue
    dumps = args.get("dumps")
    if isinstance(dumps, str):
      try:
        dumps = json.loads(dumps)
        args["dumps"] = dumps
        event["_dumps_was_string"] = True
      except json.JSONDecodeError:
        continue
    if isinstance(dumps, dict) and "heaps_v2" in dumps:
      heaps_dumps.append(dumps)

  pc_to_string_entries = collections.defaultdict(list)
  total_pc_occurrences = 0

  for dump in heaps_dumps:
    strings_table = dump["heaps_v2"].get("maps", {}).get("strings", [])
    for entry in strings_table:
      s = entry.get("string", "")
      if s.startswith("pc:"):
        hex_pc = s[3:]
        pc_to_string_entries[hex_pc].append(entry)
        total_pc_occurrences += 1

  print(f"Found {len(pc_to_string_entries)} unique raw program counters "
        f"({total_pc_occurrences} total occurrences) across "
        f"{len(heaps_dumps)} snapshots.")

  return heaps_dumps, pc_to_string_entries


def compute_relative_offsets(pc_to_string_entries, base_address, size):
  """Converts absolute runtime PCs to relative ELF offsets for symbolization.

  Returns:
    tuple of (offsets, offset_to_entries)
    - offsets: List of hex offset strings (e.g. ['0x113c204', ...]).
    - offset_to_entries: Map of offset_str -> list of entry dict references.
  """
  offsets = []
  offset_to_entries = collections.defaultdict(list)

  for hex_pc, entries in pc_to_string_entries.items():
    pc_val = int(hex_pc, 16)
    if base_address <= pc_val < base_address + size:
      offset = pc_val - base_address
      offset_str = f"0x{offset:x}"
      if offset_str not in offset_to_entries:
        offsets.append(offset_str)
      offset_to_entries[offset_str].extend(entries)

  return offsets, offset_to_entries


def invoke_llvm_symbolizer(offsets, lib_path, symbolizer_path):
  """Batches all offsets to llvm-symbolizer over stdin in a single process."""
  if not offsets:
    return []

  cmd = [symbolizer_path, "--demangle", "--no-inlines", f"--obj={lib_path}"]
  try:
    with subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True) as process:
      stdout, stderr = process.communicate(input="\n".join(offsets))
    if process.returncode != 0:
      print(f"Error running llvm-symbolizer: {stderr}")
      sys.exit(1)
  except FileNotFoundError:
    print(f"Error: Symbolizer executable not found at '{symbolizer_path}'. "
          "Please ensure llvm-symbolizer is installed and in your PATH, "
          "or specify a valid path using --symbolizer_path.")
    sys.exit(1)

  return stdout.splitlines()


def sanitize_source_location(loc, repo_root):
  """Cleans up absolute build machine source paths into relative paths."""
  if repo_root and loc.startswith(repo_root):
    return os.path.relpath(loc, repo_root)

  common_dirs = [
      "cobalt/", "starboard/", "third_party/", "base/", "net/", "media/", "ui/",
      "components/", "content/", "gpu/", "cc/", "mojo/", "services/", "v8/",
      "skia/", "url/", "ipc/", "crypto/", "copied_base/"
  ]
  for d in common_dirs:
    if d in loc:
      return d + loc.partition(d)[2]

  return loc


def parse_and_apply_symbols(offsets, symbolizer_lines, offset_to_entries,
                            repo_root):
  """Parses llvm-symbolizer 3-line output blocks and mutates JSON in-place."""
  resolved_count = 0

  for i, offset_key in enumerate(offsets):
    func_idx = 3 * i
    loc_idx = 3 * i + 1

    if loc_idx >= len(symbolizer_lines):
      symbol = f"Unresolved [offset: {offset_key}]"
    else:
      func = symbolizer_lines[func_idx].strip()
      loc = symbolizer_lines[loc_idx].strip()

      if func == "??" or not func:
        symbol = f"Unresolved [offset: {offset_key}]"
      else:
        loc = sanitize_source_location(loc, repo_root)
        symbol = f"{func} ({loc})"
        resolved_count += 1

    # Mutate the dictionary entries in-place
    for entry in offset_to_entries[offset_key]:
      entry["string"] = symbol

  print(f"🎉 Successfully resolved {resolved_count} "
        f"out of {len(offsets)} C++ symbols!")


def save_symbolized_trace(trace_data, trace_path):
  """Atomically writes the updated JSON trace back to disk."""
  raw_events = trace_data if isinstance(trace_data, list) else trace_data.get(
      "traceEvents", [])
  for event in raw_events:
    if isinstance(event, dict) and event.pop("_dumps_was_string", False):
      args = event.get("args")
      if isinstance(args, dict) and "dumps" in args:
        args["dumps"] = json.dumps(args["dumps"])

  print("Saving symbolized trace...")
  temp_trace_path = trace_path + ".tmp"
  try:
    with open(temp_trace_path, "w", encoding="utf-8") as f:
      json.dump(trace_data, f)
    os.replace(temp_trace_path, trace_path)
  except Exception as e:
    if os.path.exists(temp_trace_path):
      os.remove(temp_trace_path)
    raise e


def generate_and_print_verification_summary(heaps_dumps,
                                            base_address=None,
                                            lib_path=None,
                                            summary_json_path=None):
  """Prints statistical verification and optionally exports JSON summary.

  Returns:
    The overall symbol resolution rate (float between 0.0 and 1.0).
  """
  print("\nVerifying symbolized trace mapping...")
  last_heaps = heaps_dumps[-1]["heaps_v2"] if heaps_dumps else None

  total_strings = 0
  fully_symbolized = 0
  unresolved_pcs = 0
  resolution_rate = 1.0

  if last_heaps:
    strings_table = last_heaps.get("maps", {}).get("strings", [])
    pcs = [
        s["string"]
        for s in strings_table
        if "string" in s and s["string"].startswith("pc:")
    ]
    resolved = [
        s["string"]
        for s in strings_table
        if "string" in s and not s["string"].startswith("pc:")
    ]
    total_strings = len(strings_table)
    fully_symbolized = len(resolved)
    unresolved_pcs = len(pcs)
    resolution_rate = (
        float(fully_symbolized) / total_strings if total_strings > 0 else 1.0)

    print("   📊 Verification Stats (Latest Snapshot):")
    print(f"      • Total Strings in Maps:    {total_strings}")
    print(f"      • Fully Symbolized C++:     {fully_symbolized}")
    print(f"      • Unresolved System PCs:    {unresolved_pcs}")
    print(f"      • Symbol Resolution Rate:   {resolution_rate:.2%}")
    print("   Sample Resolved C++ Symbols:")
    for s in resolved[:10]:
      print(f"      - {s}")
    print()

  if summary_json_path:
    summary_data = {
        "total_snapshots": len(heaps_dumps),
        "total_strings_in_maps": total_strings,
        "symbolized_pcs": fully_symbolized,
        "unresolved_system_pcs": unresolved_pcs,
        "symbol_resolution_rate": round(resolution_rate, 4),
        "base_load_address":
            (f"0x{base_address:x}" if base_address is not None else None),
        "mapped_library": os.path.basename(lib_path) if lib_path else None,
    }
    summary_abs_path = os.path.abspath(summary_json_path)
    with open(summary_abs_path, "w", encoding="utf-8") as f:
      json.dump(summary_data, f, indent=2)
    print(f"📊 Machine-readable summary exported: {summary_abs_path}")

  return resolution_rate


def export_flamegraph_html(trace_path, export_html_arg):
  """Converts the symbolized trace to an interactive HTML flamegraph."""
  if html_converter:
    if isinstance(export_html_arg, str):
      html_out = os.path.abspath(export_html_arg)
    else:
      html_out = os.path.splitext(trace_path)[0] + ".html"
    html_converter.convert_trace_to_html(trace_path, html_out)
  else:
    print("Warning: Could not import convert_heaps_v2_to_html.")


def main():
  parser = argparse.ArgumentParser(
      description=(
          "Cobalt DWARF Callstack Symbolizer.\n"
          "Resolves raw program counters inside in-process heap traces "
          "using local unstripped binaries."))
  parser.add_argument(
      "trace_path",
      help="Path to the JSON trace file to symbolize (e.g. /tmp/c27_raw.json)")
  parser.add_argument(
      "-l",
      "--lib_path",
      required=True,
      help="Path to the unstripped libcobalt.so / libchrobalt.so.")
  parser.add_argument(
      "-o",
      "--output_path",
      help=("Optional path to write the symbolized trace. If omitted, "
            "modifies the input trace in-place."))
  parser.add_argument(
      "-s",
      "--symbolizer_path",
      help=("Path to llvm-symbolizer. If omitted, will auto-detect "
            "inside the toolchain, falling back to system PATH."))
  parser.add_argument(
      "--export_html",
      nargs="?",
      const=True,
      help=("Export self-contained interactive HTML Flamegraph report for "
            "direct browser visualization. Optional output path."))
  parser.add_argument(
      "--export_summary_json",
      help="Optional path to export a machine-readable JSON summary report.")
  parser.add_argument(
      "--fail_under_resolution_rate",
      type=float,
      help=("Minimum required symbol resolution rate (e.g. 0.90 for 90%%). "
            "Exits with non-zero code if resolution rate is below threshold."))

  args = parser.parse_args()

  # 1. Validate Trace Path & Output Path
  trace_path = os.path.abspath(args.trace_path)
  if not os.path.exists(trace_path):
    print(f"Error: Trace file not found at: {trace_path}")
    sys.exit(1)

  output_path = (
      os.path.abspath(args.output_path) if args.output_path else trace_path)

  # 2. Discover Repo Root & Toolchain Binaries
  repo_root = find_repo_root()
  lib_path = resolve_unstripped_library_path(args.lib_path)
  symbolizer_path = resolve_symbolizer_path(args.symbolizer_path, repo_root)

  print("============================================================")
  print("🚀 RUNNING COBALT HEAP SYMBOLIZER")
  print(f"   📁 Trace File:       {trace_path}")
  print(f"   ⚙️  Unstripped Lib:   {lib_path}")
  print(f"   🛠️  LLVM Symbolizer:  {symbolizer_path}")
  if output_path != trace_path:
    print(f"   💾 Output Target:    {output_path}")
  print("============================================================")

  # 3. Load Trace JSON
  print("Loading trace file...")
  with open(trace_path, "r", encoding="utf-8", errors="replace") as f:
    trace_data = json.load(f)

  # 4. Extract Memory Dumps and Raw Program Counters
  heaps_dumps, pc_to_string_entries = extract_memory_snapshots_and_pcs(
      trace_data)

  if not pc_to_string_entries:
    print("💡 No unresolved raw program counters found in trace "
          "(callstacks are already resolved or thread-attributed).")
    if output_path != trace_path:
      save_symbolized_trace(trace_data, output_path)
    if args.export_summary_json:
      generate_and_print_verification_summary(
          heaps_dumps,
          base_address=None,
          lib_path=lib_path,
          summary_json_path=args.export_summary_json)
    if args.export_html:
      export_flamegraph_html(output_path, args.export_html)
    sys.exit(0)

  # 5. Determine Base Load Address (Android TV vs. RDK Evergreen)
  print(f"Extracting {os.path.basename(lib_path)} memory mapping from trace...")
  base_address, size = extract_lib_base_address(heaps_dumps,
                                                pc_to_string_entries, lib_path)
  print(f"🎉 Found {os.path.basename(lib_path)} base load address: "
        f"0x{base_address:x} (Size: 0x{size:x} bytes)")

  # 6. Map PCs to Relative ELF Offsets
  print("Resolving C++ symbols...")
  offsets, offset_to_entries = compute_relative_offsets(pc_to_string_entries,
                                                        base_address, size)
  print(f"Resolving {len(offsets)} offsets belonging to "
        f"{os.path.basename(lib_path)}...")

  # 7. Bulk Symbolize with llvm-symbolizer
  symbolizer_lines = invoke_llvm_symbolizer(offsets, lib_path, symbolizer_path)
  parse_and_apply_symbols(offsets, symbolizer_lines, offset_to_entries,
                          repo_root)

  # 8. Save Updated Trace
  save_symbolized_trace(trace_data, output_path)

  # 9. Self-Verification & Export Reports
  resolution_rate = generate_and_print_verification_summary(
      heaps_dumps,
      base_address=base_address,
      lib_path=lib_path,
      summary_json_path=args.export_summary_json)

  if args.export_html:
    export_flamegraph_html(output_path, args.export_html)

  print("============================================================")
  print("🎉 SYMBOLIZATION COMPLETELY SUCCESSFUL!")
  print(f"   📁 Output Path: {output_path}")
  print("============================================================")

  if args.fail_under_resolution_rate is not None:
    if resolution_rate < args.fail_under_resolution_rate:
      print(f"❌ Error: Symbol resolution rate {resolution_rate:.2%} is below "
            f"required threshold {args.fail_under_resolution_rate:.2%}!")
      sys.exit(1)


if __name__ == "__main__":
  main()
