# Copyright 2017 Google Inc. All Rights Reserved.
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
"""Allows to use Bash as a shell."""

import re

from starboard.tools.toolchain import abstract


def _MaybeJoin(shell, command):
  if isinstance(command, basestring):
    return command
  return shell.Join(command)


class Shell(abstract.Shell):
  """Constructs command lines using Bash syntax."""

  def MaybeQuoteArgument(self, argument):
    # Rather than attempting to enumerate the bad shell characters, just
    # whitelist common OK ones and quote anything else.
    if re.match(r'^[a-zA-Z0-9_=.,\\/+-]+$', argument):
      return argument  # No quoting necessary.
    return "'" + argument.replace("'", "'\"'\"'") + "'"

  def Join(self, command):
    assert not isinstance(command, basestring)
    return ' '.join(self.MaybeQuoteArgument(argument) for argument in command)

  def And(self, *commands):
    return ' && '.join(_MaybeJoin(self, command) for command in commands)

  def Or(self, *commands):
    return ' || '.join(_MaybeJoin(self, command) for command in commands)
