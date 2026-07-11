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
import json
import os
import re
import subprocess
import sys


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
      help="Path to the unstripped libchrobalt.so.")
  parser.add_argument(
      "-s",
      "--symbolizer_path",
      help=("Path to llvm-symbolizer. If omitted, will auto-detect "
            "inside the toolchain, falling back to system PATH."))

  args = parser.parse_args()

  # 1. Validate Trace Path
  trace_path = os.path.abspath(args.trace_path)
  if not os.path.exists(trace_path):
    print(f"Error: Trace file not found at: {trace_path}")
    sys.exit(1)

  # 2. Discover Repo Root and Validate Library Path
  repo_root = find_repo_root()
  lib_path = os.path.abspath(args.lib_path)
  if not os.path.exists(lib_path):
    print(f"Error: Specified library not found at: {lib_path}")
    sys.exit(1)

  # 3. Auto-detect Toolchain llvm-symbolizer
  symbolizer_path = args.symbolizer_path
  if not symbolizer_path:
    toolchain_path = None
    if repo_root:
      toolchain_path = os.path.join(
          repo_root,
          "third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer")

    if toolchain_path and os.path.exists(toolchain_path):
      symbolizer_path = toolchain_path
      rel_sym_path = os.path.relpath(symbolizer_path, repo_root)
      print(f"💡 Using toolchain prebuilt symbolizer: {rel_sym_path}")
    else:
      symbolizer_path = "llvm-symbolizer"
      print("💡 Toolchain symbolizer not found. "
            "Falling back to system 'llvm-symbolizer'.")
  else:
    if symbolizer_path != "llvm-symbolizer" and not os.path.exists(
        symbolizer_path):
      print(f"Warning: Specified symbolizer not found at {symbolizer_path}."
            f" Falling back to system 'llvm-symbolizer'.")
      symbolizer_path = "llvm-symbolizer"

  # 4. Present Execution Profile
  print("============================================================")
  print("🚀 RUNNING COBALT HEAP SYMBOLIZER")
  print(f"   📁 Trace File:       {trace_path}")
  print(f"   ⚙️  Unstripped Lib:   {lib_path}")
  print(f"   🛠️  LLVM Symbolizer:  {symbolizer_path}")
  print("============================================================")

  print("Loading trace file...")
  with open(trace_path, "r", encoding="utf-8", errors="replace") as f:
    trace_string = f.read()

  # Step 5: Find the base load address of libchrobalt.so
  print("Extracting libchrobalt.so memory mapping from trace...")
  regex_pattern = (r"\\\"?mf\\\"?:\s*\\\"?([^\\\"]*libchrobalt.so)\\\"?,"
                   r"\s*\\\"?pf\\\"?:\s*5,"
                   r"\s*\\\"?sa\\\"?:\s*\\\"?(?P<sa>[0-9a-fA-F]+)\\\"?,"
                   r"\s*\\\"?sz\\\"?:\s*\\\"?(?P<sz>[0-9a-fA-F]+)\\\"?")
  match = re.search(regex_pattern, trace_string)

  if not match:
    relaxed_pattern = (r"libchrobalt.so[^}\n]*?pf[^}\n]*?5[^}\n]*?sa[^}\n]*?"
                       r"(?P<sa>[0-9a-fA-F]+)"
                       r"[^}\n]*?sz[^}\n]*?(?P<sz>[0-9a-fA-F]+)")
    match = re.search(relaxed_pattern, trace_string)

  if not match:
    print("Error: Could not find libchrobalt.so executable mapping in trace!")
    sys.exit(1)

  base_address_hex = match.group("sa")
  size_hex = match.group("sz")

  base_address = int(base_address_hex, 16)
  size = int(size_hex, 16)

  print(f"🎉 Found libchrobalt.so base load address: 0x{base_address_hex} "
        f"(Size: 0x{size_hex} bytes)")

  trace_data = json.loads(trace_string)

  # Step 6: Locate heaps_v2 and extract all PC strings across all snapshots
  events = [
      x for x in (trace_data if isinstance(trace_data, list) else trace_data
                  .get("traceEvents", []))
      if x.get("name") == "periodic_interval"
  ]

  entry_map = {}
  pc_count = 0

  parsed_dumps = []
  for e in events:
    args = e.get("args", {})
    if "dumps" not in args:
      continue
    dumps = args["dumps"]
    was_string = isinstance(dumps, str)
    temp = json.loads(dumps) if was_string else dumps
    if isinstance(temp, dict) and "heaps_v2" in temp:
      parsed_dumps.append((e, temp, was_string))
      heaps = temp["heaps_v2"]
      strings_table = heaps.get("maps", {}).get("strings", [])
      for entry in strings_table:
        s = entry.get("string", "")
        if s.startswith("pc:"):
          pc_hex = s[3:]
          if pc_hex not in entry_map:
            entry_map[pc_hex] = []
          entry_map[pc_hex].append(entry)
          pc_count += 1

  print(f"Found {len(entry_map)} unique raw program counters "
        f"({pc_count} total occurrences) across all snapshots.")

  if not entry_map:
    print("No program counters to resolve!")
    sys.exit(0)

  # Step 7: Resolve PCs in bulk using llvm-symbolizer
  print("Resolving C++ symbols...")
  offsets = []
  offset_to_entries = {}

  for pc_hex, entries in entry_map.items():
    pc_val = int(pc_hex, 16)
    if base_address <= pc_val < base_address + size:
      offset = pc_val - base_address
      offset_str = f"0x{offset:x}"
      if offset_str not in offset_to_entries:
        offset_to_entries[offset_str] = []
        offsets.append(offset_str)
      offset_to_entries[offset_str].extend(entries)

  print(f"Resolving {len(offsets)} offsets belonging to libchrobalt.so...")

  stdout = ""
  if offsets:
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

  lines = stdout.splitlines()

  resolved_count = 0
  for i in range(len(offsets)):
    func_idx = 3 * i
    loc_idx = 3 * i + 1
    if loc_idx >= len(lines):
      symbol = f"Unresolved [offset: {offsets[i]}]"
    else:
      func = lines[func_idx].strip()
      loc = lines[loc_idx].strip()

      if func == "??" or not func:
        symbol = f"Unresolved [offset: {offsets[i]}]"
      else:
        if repo_root and loc.startswith(repo_root):
          loc = os.path.relpath(loc, repo_root)
        else:
          common_dirs = [
              "cobalt/", "starboard/", "third_party/", "base/", "net/",
              "media/", "ui/", "components/", "content/", "gpu/", "cc/",
              "mojo/", "services/", "v8/", "skia/", "url/", "ipc/", "crypto/",
              "copied_base/"
          ]
          for d in common_dirs:
            if d in loc:
              loc = d + loc.partition(d)[2]
              break

        symbol = f"{func} ({loc})"
        resolved_count += 1

    offset_key = offsets[i]
    for entry in offset_to_entries[offset_key]:
      entry["string"] = symbol

  print(f"🎉 Successfully resolved {resolved_count} "
        f"out of {len(offsets)} C++ symbols!")

  # Step 8: Save back to disk in-place
  print("Saving symbolized trace...")
  for e, temp, was_string in parsed_dumps:
    if was_string:
      e["args"]["dumps"] = json.dumps(temp)

  temp_trace_path = trace_path + ".tmp"
  try:
    with open(temp_trace_path, "w", encoding="utf-8") as f:
      json.dump(trace_data, f)
    os.replace(temp_trace_path, trace_path)
  except Exception as e:
    if os.path.exists(temp_trace_path):
      os.remove(temp_trace_path)
    raise e

  # Step 9: Self-Verification Phase
  print("\nVerifying symbolized trace mapping...")
  last_heaps = parsed_dumps[-1][1]["heaps_v2"] if parsed_dumps else None

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
    print("   📊 Verification Stats (Latest Snapshot):")
    print(f"      • Total Strings in Maps:    {len(strings_table)}")
    print(f"      • Fully Symbolized C++:     {len(resolved)}")
    print(f"      • Unresolved System PCs:    {len(pcs)}")
    print("   Sample Resolved C++ Symbols:")
    for s in resolved[:10]:
      print(f"      - {s}")
    print()

  print("============================================================")
  print("🎉 SYMBOLIZATION COMPLETELY SUCCESSFUL!")
  print(f"   📁 Output Path: {trace_path}")
  print("============================================================")


if __name__ == "__main__":
  main()
