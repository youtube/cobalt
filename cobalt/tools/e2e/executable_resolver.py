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
"""Abstract interface for locating Cobalt executables and launch commands."""

import abc
from typing import List, Optional, Union


class ExecutableResolver(abc.ABC):
  """Abstract interface for locating Cobalt binaries and launch targets."""

  @abc.abstractmethod
  def resolve(
      self,
      config: str = 'qa',
      custom_executable: Optional[str] = None,
      out_dir: Optional[str] = None,
  ) -> Union[str, List[str]]:
    """Resolves and returns the executable command or target."""
