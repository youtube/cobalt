#!/usr/bin/env python3
#
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
#
"""tvOS packager using llvm-ar to handle thin archives.

This script is a replacement for simple_packager.py that handles LLVM thin archives
correctly by using llvm-ar with the 'L' modifier to merge them.
"""

import os
import re
import shutil
import subprocess
import sys

if len(sys.argv) < 4:
    print("Usage: tvos_packager.py <src_dir> <out_dir> <package_dir> [<build_info_path>]")
    sys.exit(1)

src_dir = sys.argv[1]
out_dir = sys.argv[2]
package_dir = sys.argv[3]
# build_info_path is accepted for compatibility but not used yet
build_info_path = sys.argv[4] if len(sys.argv) > 4 else None

ninja_path = os.path.join(out_dir, 'obj/cobalt/cobalt_executable.ninja')
llvm_ar = os.path.join(src_dir, 'third_party/llvm-build/Release+Asserts/bin/llvm-ar')

if not os.path.exists(ninja_path):
    print(f"Error: Ninja file not found at {ninja_path}")
    sys.exit(1)

if not os.path.exists(llvm_ar):
    print(f"Error: llvm-ar not found at {llvm_ar}")
    sys.exit(1)

print(f"Parsing {ninja_path}...")

def parse_ninja_file(ninja_path):
    with open(ninja_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Handle line continuations with $
    content = re.sub(r'\$\r?\n\s*', '', content)

    lines = content.split('\n')
    
    variables = {}
    link_inputs = []
    
    for line in lines:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        # Parse variable definitions: var = value
        var_match = re.match(r'^([a-zA-Z0-9_]+)\s*=\s*(.*)$', line)
        if var_match:
            name = var_match.group(1)
            val = var_match.group(2)
            variables[name] = val
            continue
        
        # Parse build statement: build obj/cobalt/cobalt: link input1 input2 ...
        build_match = re.match(r'^build\s+obj/cobalt/cobalt:\s*link\s+(.*)$', line)
        if build_match:
            inputs_str = build_match.group(1)
            parts = inputs_str.split()
            for part in parts:
                if part == '|' or part == '||':
                    break
                link_inputs.append(part)
            continue
            
    return variables, link_inputs

variables, link_inputs = parse_ninja_file(ninja_path)

# Collect inputs
all_inputs = []

# Add rlibs
rlibs = variables.get('rlibs', '').split()
all_inputs.extend(rlibs)

# Add libs if they are actual files (avoid -l options)
libs = variables.get('libs', '').split()
for lib in libs:
    if lib.endswith('.a') or lib.endswith('.o') or lib.endswith('.rlib'):
        all_inputs.append(lib)

# Add primary link inputs
all_inputs.extend(link_inputs)

# Deduplicate inputs while preserving order
seen = set()
unique_inputs = []
for item in all_inputs:
    if item not in seen:
        seen.add(item)
        unique_inputs.append(item)

print(f"Total unique inputs to bundle: {len(unique_inputs)}")

# Write response file in out_dir
rsp_path = os.path.join(out_dir, 'cobalt_a_inputs.rsp')
with open(rsp_path, 'w', encoding='utf-8') as f:
    f.write('\n'.join(unique_inputs) + '\n')

print(f"Response file written to: {rsp_path}")

# Run llvm-ar
output_a = os.path.join(out_dir, 'cobalt.a')
if os.path.exists(output_a):
    print("Removing existing cobalt.a in out_dir...")
    os.remove(output_a)

print("Running llvm-ar to generate cobalt.a...")
ar_cmd = [
    llvm_ar,
    'qcsL',
    'cobalt.a',
    '@cobalt_a_inputs.rsp'
]

result = subprocess.run(
    ar_cmd,
    cwd=out_dir,
    capture_output=True,
    text=True,
    check=False
)

if result.returncode != 0:
    print(f"Error: llvm-ar failed with code {result.returncode}")
    print("Stdout:", result.stdout)
    print("Stderr:", result.stderr)
    if os.path.exists(rsp_path):
        os.remove(rsp_path)
    sys.exit(1)

print("Archive created successfully in out_dir.")

# Remove temporary response file
if os.path.exists(rsp_path):
    os.remove(rsp_path)

# Copy to package_dir/lib/cobalt.a
lib_dir = os.path.join(package_dir, 'lib')
os.makedirs(lib_dir, exist_ok=True)
dest_a = os.path.join(lib_dir, 'cobalt.a')

print(f"Copying cobalt.a to {dest_a}...")
shutil.copy2(output_a, dest_a)

# Clean up the one in out_dir to match simple_packager.py behavior
print("Removing temporary cobalt.a from out_dir...")
os.remove(output_a)

print("Success!")
