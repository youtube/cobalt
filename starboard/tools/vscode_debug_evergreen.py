# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""
GDB script to automatically load symbols for Evergreen libraries.

This script sets a breakpoint on elf_loader::ProgramTable::PublishEvergreenInfo
to detect when an ELF file is loaded and then uses the add-symbol-file command
to load the symbols at the correct memory offset.

# pylint: disable=line-too-long
To use it, add the following to your .vscode/launch.json:
{
    "name": "Cobalt Evergreen (gdb)",
    "type": "cppdbg",
    "request": "launch",
    "program": "${workspaceFolder}/out/evergreen-x64_devel/elf_loader_sandbox",
    "args": ["--evergreen_content=.", "--evergreen_library=libcobalt.so"],
    // other fields...
    "setupCommands": [
        {
            "description": "Load Evergreen Symbol Hook",
            "text": "source ${workspaceFolder}/starboard/tools/vscode_debug_evergreen.py"
        }
    ]
}

# pylint: enable=line-too-long
"""

import gdb


class LoadEvergreenLibrary(gdb.Breakpoint):
  """
  A GDB Breakpoint that hooks into the Evergreen loader to load symbols.
  """

  def __init__(self):
    # Set a breakpoint on the function where the library is fully loaded
    # and we have the base address and file path available.
    super().__init__(
        "elf_loader::ProgramTable::PublishEvergreenInfo", internal=True)
    self.silent = True

  def stop(self):
    try:
      # Access the 'file_path' argument
      file_path_val = gdb.parse_and_eval("file_path")
      file_path = file_path_val.string()

      # Access the 'base_memory_address_' member of the 'this' object
      # We cast to uintptr_t to ensure we get an integer address
      base_addr_val = gdb.parse_and_eval("this->base_memory_address_")
      base_addr = int(base_addr_val)

      print("[Evergreen Loader Hook] Detected "
            f"load of {file_path} at {hex(base_addr)}")

      # Construct and execute the add-symbol-file command
      # The -o option offsets all sections by the base address, which matches
      # how the ELF loader maps the segments (assuming 0-based ELF segments).
      cmd = f"add-symbol-file {file_path} -o {hex(base_addr)}"

      # We use from_tty=False to avoid "add symbol table from file? (y or n)"
      # prompts but add-symbol-file might still prompt.
      # To suppress confirmation, we can use "noconfirm" if available or
      # set confirm off temporarily.
      gdb.execute("set confirm off")
      gdb.execute(cmd)
      gdb.execute("set confirm on")

      print("[Evergreen Loader Hook] Symbols loaded successfully.")

    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f"[Evergreen Loader Hook] Error loading symbols: {e}")

    # Return False to continue execution immediately
    return False


# Instantiate the breakpoint
LoadEvergreenLibrary()
print("[Evergreen Loader Hook] Initialized. "
      "Waiting for Evergreen libraries to load...")
