# Copyright 2018 Google Inc. All Rights Reserved.
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
"""Allows to use cmd as a shell."""

import re

from starboard.tools.toolchain import abstract


def _MaybeJoin(shell, command):
  if isinstance(command, basestring):
    return command
  return shell.Join(command)


class Shell(abstract.Shell):
  """Constructs command lines using Cmd syntax."""

  def __init__(self, quote=True):
    # Toggle whether or not to quote command line arguments.
    self.quote = quote

  def MaybeQuoteArgument(self, arg):
    # Rather than attempting to enumerate the bad shell characters, just
    # whitelist common OK ones and quote anything else.
    if re.match(r'^[a-zA-Z0-9_=.\\/-]+$', arg):
      return arg  # No quoting necessary.
    return self.QuoteForRspFile(arg)

  def QuoteForRspFile(self, arg):
    """Quote a command line argument.

    Quote the argument so that it appears as one argument when
    processed via cmd.exe and parsed by CommandLineToArgvW (as is typical for
    Windows programs).

    Args:
      arg: The argument to quote.

    Returns:
      The quoted argument.
    """

    # See http://goo.gl/cuFbX and http://goo.gl/dhPnp including the comment
    # threads. This is actually the quoting rules for CommandLineToArgvW, not
    # for the shell, because the shell doesn't do anything in Windows. This
    # works more or less because most programs (including the compiler, etc.)
    # use that function to handle command line arguments.

    # For a literal quote, CommandLineToArgvW requires 2n+1 backslashes
    # preceding it, and results in n backslashes + the quote. So we substitute
    # in 2* what we match, +1 more, plus the quote.
    windows_quoter_regex = re.compile(r'(\\*)"')
    arg = windows_quoter_regex.sub(lambda mo: 2 * mo.group(1) + '\\"', arg)

    # %'s also need to be doubled otherwise they're interpreted as batch
    # positional arguments. Also make sure to escape the % so that they're
    # passed literally through escaping so they can be singled to just the
    # original %. Otherwise, trying to pass the literal representation that
    # looks like an environment variable to the shell (e.g. %PATH%) would fail.
    arg = arg.replace('%', '%%')

    # These commands are used in rsp files, so no escaping for the shell (via ^)
    # is necessary.

    # Finally, wrap the whole thing in quotes so that the above quote rule
    # applies and whitespace isn't a word break.
    return '"' + arg + '"'

  def Join(self, command):
    assert not isinstance(command, basestring)
    if self.quote:
      return ' '.join(self.MaybeQuoteArgument(argument) for argument in command)
    else:
      return ' '.join(command)

  def And(self, *commands):
    return ' && '.join(_MaybeJoin(self, command) for command in commands)

  def Or(self, *commands):
    return ' || '.join(_MaybeJoin(self, command) for command in commands)
