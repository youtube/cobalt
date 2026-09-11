# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Resolves Linux Modular and Evergreen Cobalt executables."""

import os
from typing import List, Optional, Union

try:
  from cobalt.tools.e2e.executable_resolver import ExecutableResolver
except ImportError:
  from executable_resolver import ExecutableResolver  # type: ignore[no-redef]


class LinuxExecutableResolver(ExecutableResolver):
  """Resolves Linux Modular and Evergreen Cobalt executables."""

  def __init__(self, platform: Optional[str] = None):
    self.platform = platform

  def resolve(
      self,
      config: str = 'qa',
      custom_executable: Optional[str] = None,
      out_dir: Optional[str] = None,
  ) -> Union[str, List[str]]:
    """Resolves the executable command based on configuration."""
    if custom_executable:
      return custom_executable

    search_roots = []
    if out_dir:
      search_roots.append(out_dir)
    if os.environ.get('OUT_DIR'):
      search_roots.append(os.environ['OUT_DIR'])
    if os.environ.get('GITHUB_WORKSPACE'):
      search_roots.append(os.path.join(os.environ['GITHUB_WORKSPACE'], 'out'))
    search_roots.extend(['./out', '../out'])

    for root in search_roots:
      modular_qa = os.path.join(root, 'linux-x64x11-modular_qa',
                                'cobalt_loader')
      modular_devel = os.path.join(root, 'linux-x64x11-modular_devel',
                                   'cobalt_loader')
      evergreen_qa = os.path.join(root, 'evergreen-x64_qa', 'loader_app')
      evergreen_devel = os.path.join(root, 'evergreen-x64_devel', 'loader_app')

      def format_evergreen(loader, lib_dir):
        lib = os.path.join(lib_dir, 'libcobalt.so')
        lib_arg = lib if os.path.isfile(lib) else 'libcobalt.so'
        content_arg = lib_dir if os.path.isdir(lib_dir) else '.'
        return (f'{loader} --evergreen_content={content_arg}'
                f' --evergreen_library={lib_arg}')

      # Also check if root itself is the specific out directory
      if os.path.isfile(os.path.join(root, 'cobalt_loader')):
        return os.path.join(root, 'cobalt_loader')
      if os.path.isfile(os.path.join(root, 'loader_app')):
        return format_evergreen(os.path.join(root, 'loader_app'), root)

      if self.platform in ('modular', 'linux-x64x11-modular'):
        target = modular_qa if config == 'qa' else modular_devel
        if os.path.isfile(target):
          return target
        if os.path.isfile(modular_devel):
          return modular_devel

      if self.platform in ('evergreen', 'evergreen-x64'):
        target = evergreen_qa if config == 'qa' else evergreen_devel
        if os.path.isfile(target):
          return format_evergreen(target, os.path.dirname(target))
        if os.path.isfile(evergreen_devel):
          return format_evergreen(evergreen_devel,
                                  os.path.dirname(evergreen_devel))

      # Auto-detection
      if os.path.isfile(modular_qa):
        return modular_qa
      if os.path.isfile(evergreen_qa):
        return format_evergreen(evergreen_qa, os.path.dirname(evergreen_qa))
      if os.path.isfile(modular_devel):
        return modular_devel
      if os.path.isfile(evergreen_devel):
        return format_evergreen(evergreen_devel,
                                os.path.dirname(evergreen_devel))

    raise FileNotFoundError(
        'Could not find any Cobalt executable in search paths. Please specify'
        ' --executable, --out-dir, or build modular/evergreen first.')
