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
"""Defines abstract build tools."""

import abc


class Tool(object):
  """A base class for all build tools.

  Build tools are used to generate Ninja files.
  """

  __metaclass__ = abc.ABCMeta

  @abc.abstractmethod
  def IsPlatformAgnostic(self):
    """Whether an output of a tool depends on a platform.

    Platform-agnostic tools are only processed once, even if they are provided
    by both target and host toolchains.
    """
    pass

  @abc.abstractmethod
  def GetRuleName(self):
    """Returns a name of a Ninja rule.

    Used as a base name for several Ninja variables, such as path, flags, etc.

    Returns:
      None if a tool doesn't have a corresponding rule.
    """
    pass

  # TODO: Inline path in a command and get rid of this method.
  @abc.abstractmethod
  def GetPath(self):
    """Returns a full path to a tool.

    Returns:
      None if a tool doesn't have a corresponding rule.
    """
    pass

  # TODO: Inline extra flags in a command and get rid of this method.
  @abc.abstractmethod
  def GetExtraFlags(self):
    """Returns tool flags specific to a platform.

    These flags are common for all tool invocations.

    Returns:
      None if a tool doesn't have a corresponding rule.
    """
    pass

  @abc.abstractmethod
  def GetMaxConcurrentProcesses(self):
    """Restricts an amount of concurrently running tool instances.

    See 'depth' at https://ninja-build.org/manual.html#ref_pool for details.

    Returns:
      None if a tool doesn't have a corresponding rule or doesn't restrict an
      amount of concurrently running instances.
    """
    pass

  @abc.abstractmethod
  def GetCommand(self, path, extra_flags, flags, shell):
    """Returns a command that invokes a tool.

    See 'command' at https://ninja-build.org/manual.html#ref_rule for details.

    Args:
      path: A variable that contains a path to a tool.
      extra_flags: A variable that contains tool flags specific to a platform.
      flags: A variable that contains tool flags specific to a target.
      shell: The shell that will be used to interpret the command.

    Returns:
      None if a tool doesn't have a corresponding rule.
    """
    pass

  @abc.abstractmethod
  def GetDescription(self):
    """Returns a human-readable description of an action performed by a tool.

    See 'description' at https://ninja-build.org/manual.html#ref_rule for
    details.

    Returns:
      None if a tool doesn't have a corresponding rule.
    """
    pass

  @abc.abstractmethod
  def GetHeaderDependenciesFilePath(self):
    """Returns a path to a Makefile that contains implicit dependencies.

    See 'depfile' at https://ninja-build.org/manual.html#ref_rule for details.

    Returns:
      None if a tool doesn't generate implicit dependencies.
    """
    pass

  @abc.abstractmethod
  def GetHeaderDependenciesFormat(self):
    """Specifies special dependency processing by Ninja.

    See 'deps' at https://ninja-build.org/manual.html#ref_rule for details.

    Returns:
      None if a tool doesn't generate implicit dependencies.
    """
    pass

  @abc.abstractmethod
  def GetRspFilePath(self):
    """Returns a full path to a response file.

    See 'rspfile' at https://ninja-build.org/manual.html#ref_rule for details.

    Returns:
      None if a response file is not used.
    """
    pass

  @abc.abstractmethod
  def GetRspFileContent(self):
    """Returns a content of a response file.

    See 'rspfile_content' at https://ninja-build.org/manual.html#ref_rule for
    details.

    Returns:
      None if a response file is not used.
    """
    pass


class CCompiler(Tool):
  """Compiles C sources."""

  def IsPlatformAgnostic(self):
    return False

  def GetRuleName(self):
    return 'compile_c'

  @abc.abstractmethod
  def GetFlags(self, defines, include_dirs, cflags):
    """Returns tool flags specific to a target.

    This method translates platform-agnostic concepts into a command line
    arguments understood by a tool.

    Args:
      defines: A list of preprocessor defines in "NAME=VALUE" format.
      include_dirs: A list of header search directories.
      cflags: A list of GCC-style command-line flags. See
        https://gcc.gnu.org/onlinedocs/gcc/Invoking-GCC.html#Invoking-GCC for
        details.

    Returns:
      A list of unquoted strings, one for each flag. It is a responsibility of a
      caller to quote flags that contain special characters (as determined by a
      shell) before passing to a tool.
    """
    pass


class CxxCompiler(Tool):
  """Compiles C++ sources."""

  def IsPlatformAgnostic(self):
    return False

  def GetRuleName(self):
    return 'compile_cxx'

  @abc.abstractmethod
  def GetFlags(self, defines, include_dirs, cflags):
    """Returns tool flags specific to a target.

    This method translates platform-agnostic concepts into a command line
    arguments understood by a tool.

    Args:
      defines: A list of preprocessor defines in "NAME=VALUE" format.
      include_dirs: A list of header search directories.
      cflags: A list of GCC-style command-line flags. See
        https://gcc.gnu.org/onlinedocs/gcc/Invoking-GCC.html#Invoking-GCC for
        details.

    Returns:
      A list of unquoted strings, one for each flag. It is a responsibility of a
      caller to quote flags that contain special characters (as determined by a
      shell) before passing to a tool.
    """
    pass


class ObjectiveCxxCompiler(Tool):
  """Compiles Objective-C++ sources."""

  def IsPlatformAgnostic(self):
    return False

  def GetRuleName(self):
    return 'compile_objective_cxx'

  @abc.abstractmethod
  def GetFlags(self, defines, include_dirs, cflags):
    """Returns tool flags specific to a target.

    This method translates platform-agnostic concepts into a command line
    arguments understood by a tool.

    Args:
      defines: A list of preprocessor defines in "NAME=VALUE" format.
      include_dirs: A list of header search directories.
      cflags: A list of GCC-style command-line flags. See
        https://gcc.gnu.org/onlinedocs/gcc/Invoking-GCC.html#Invoking-GCC for
        details.

    Returns:
      A list of unquoted strings, one for each flag. It is a responsibility of a
      caller to quote flags that contain special characters (as determined by a
      shell) before passing to a tool.
    """
    pass


class AssemblerWithCPreprocessor(Tool):
  """Compiles assembler sources that contain C preprocessor directives."""

  def IsPlatformAgnostic(self):
    return False

  def GetRuleName(self):
    return 'assemble'

  @abc.abstractmethod
  def GetFlags(self, defines, include_dirs, cflags):
    """Returns tool flags specific to a target.

    This method translates platform-agnostic concepts into a command line
    arguments understood by a tool.

    Args:
      defines: A list of preprocessor defines in "NAME=VALUE" format.
      include_dirs: A list of header search directories.
      cflags: A list of GCC-style command-line flags. See
        https://gcc.gnu.org/onlinedocs/gcc/Invoking-GCC.html#Invoking-GCC for
        details.

    Returns:
      A list of unquoted strings, one for each flag. It is a responsibility of a
      caller to quote flags that contain special characters (as determined by a
      shell) before passing to a tool.
    """
    pass


class StaticLinker(Tool):
  """Creates self-contained archives."""

  def IsPlatformAgnostic(self):
    return False

  def GetRuleName(self):
    return 'link_static'

  def GetHeaderDependenciesFilePath(self):
    # Only applicable to C family compilers.
    return None

  def GetHeaderDependenciesFormat(self):
    # Only applicable to C family compilers.
    return None


class StaticThinLinker(Tool):
  """Creates thin archives using GNU ar."""

  def IsPlatformAgnostic(self):
    return False

  def GetRuleName(self):
    return 'link_static_thin'

  def GetHeaderDependenciesFilePath(self):
    # Only applicable to C family compilers.
    return None

  def GetHeaderDependenciesFormat(self):
    # Only applicable to C family compilers.
    return None


class ExecutableLinker(Tool):
  """Links executables."""

  def IsPlatformAgnostic(self):
    return False

  def GetRuleName(self):
    return 'link_executable'

  def GetHeaderDependenciesFilePath(self):
    # Only applicable to C family compilers.
    return None

  def GetHeaderDependenciesFormat(self):
    # Only applicable to C family compilers.
    return None

  @abc.abstractmethod
  def GetFlags(self, ldflags):
    """Returns tool flags specific to a target.

    This method translates platform-agnostic concepts into a command line
    arguments understood by a tool.

    Args:
      ldflags: A list of GCC-style command-line flags. See
        https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html#Link-Options for
        details.

    Returns:
      A list of unquoted strings, one for each flag. It is a responsibility of a
      caller to quote flags that contain special characters (as determined by a
      shell) before passing to a tool.
    """
    pass


class Stamp(Tool):
  """Updates the access and modification times of a file to the current time."""

  def IsPlatformAgnostic(self):
    return True

  def GetRuleName(self):
    return 'stamp'

  def GetHeaderDependenciesFilePath(self):
    # Only applicable to C family compilers.
    return None

  def GetHeaderDependenciesFormat(self):
    # Only applicable to C family compilers.
    return None


class Copy(Tool):
  """Copies individual files."""

  def IsPlatformAgnostic(self):
    return True

  def GetRuleName(self):
    return 'copy'

  def GetHeaderDependenciesFilePath(self):
    # Only applicable to C family compilers.
    return None

  def GetHeaderDependenciesFormat(self):
    # Only applicable to C family compilers.
    return None


class Shell(Tool):
  """Constructs command lines."""

  def IsPlatformAgnostic(self):
    return True

  def GetRuleName(self):
    # Shell should not be used by Ninja, only by other tools.
    return None

  def GetPath(self):
    # Shell should not be used by Ninja, only by other tools.
    return None

  def GetExtraFlags(self):
    # Shell should not be used by Ninja, only by other tools.
    return None

  def GetMaxConcurrentProcesses(self):
    # Shell should not be used by Ninja, only by other tools.
    return None

  def GetCommand(self, path, extra_flags, flags, shell):
    # Shell should not be used by Ninja, only by other tools.
    return None

  def GetDescription(self):
    # Shell should not be used by Ninja, only by other tools.
    return None

  def GetHeaderDependenciesFilePath(self):
    # Shell should not be used by Ninja, only by other tools.
    return None

  def GetHeaderDependenciesFormat(self):
    # Shell should not be used by Ninja, only by other tools.
    return None

  def GetRspFilePath(self):
    # Shell should not be used by Ninja, only by other tools.
    return None

  def GetRspFileContent(self):
    # Shell should not be used by Ninja, only by other tools.
    return None

  @abc.abstractmethod
  def MaybeQuoteArgument(self, argument):
    """Quotes a string so that shell could interpret it as a single argument.

    Args:
      argument: A string that can contain arbitrary characters.

    Returns:
      A quoted and appropriately escaped string. Returns the original string
      if no processing is necessary.
    """
    pass

  @abc.abstractmethod
  def Join(self, command):
    """Joins a sequence of unquoted command line terms into a string.

    Args:
      command: An unquoted, unescaped sequence of command line terms.

    Returns:
      A quoted and appropriately escaped string for the same command line.
    """
    pass

  @abc.abstractmethod
  def And(self, *commands):
    """Joins a sequence of command lines with the AND shell operator.

    Args:
      *commands: A sequence of either quoted strings or unquoted sequences of
                 command line terms that make up the command lines to be ANDed.

    Returns:
      A quoted and appropriately escaped string for the ANDed command line.
    """
    pass

  @abc.abstractmethod
  def Or(self, *commands):
    """Joins a sequence of command lines with the OR shell operator.

    Args:
      *commands: A sequence of either quoted strings or unquoted sequences of
                 command line terms that make up the command lines to be ORed.

    Returns:
      A quoted and appropriately escaped string for the ORed command line.
    """
    pass
