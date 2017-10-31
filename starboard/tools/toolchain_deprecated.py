# Copyright (c) 2017 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Starboard's abstract Toolchain which is implemented for each platform."""

import abc


class ABCMetaSingleton(abc.ABCMeta):
  instances = {}

  def __call__(cls, *args, **kwargs):
    if cls not in cls.instances:
      cls.instances[cls] = super(ABCMetaSingleton, cls).__call__(
          *args, **kwargs)
    return cls.instances[cls]


class Toolchain(object):
  """This is an abstract interface of the Toolchain."""
  __metaclass__ = ABCMetaSingleton

  # TODO: move variables / commands
  #   AR / _HOST
  #   ARFLAGS / _HOST
  #   ARTHINFLAGS / _HOST
  #   CC / CC_HOST
  #   CXX / CXX_HOST
  #   LD / LD_HOST
  #   RC / RC_HOST
  # And / or implement NinjaWriter.WriteSources & GenerateOutputForConfig body
  # here.

  @abc.abstractmethod
  def Define(self, d):
    pass

  @abc.abstractmethod
  def EncodeRspFileList(self, args):
    pass

  @abc.abstractmethod
  def ExpandEnvVars(self, path, env):
    pass

  @abc.abstractmethod
  def ExpandRuleVariables(self, path, root, dirname, source, ext, name):
    pass

  @abc.abstractmethod
  def GenerateEnvironmentFiles(self, toplevel_build_dir, generator_flags,
                               open_out):
    pass

  @abc.abstractmethod
  def GetCompilerSettings(self):
    pass

  @abc.abstractmethod
  def GetPrecompiledHeader(self, **kwargs):
    pass

  @abc.abstractmethod
  def InitCompilerSettings(self, spec, **kwargs):
    pass

  @abc.abstractmethod
  def SetAdditionalGypVariables(self, default_variables, **kwargs):
    pass

  @abc.abstractmethod
  def VerifyMissingSources(self, sources, **kwargs):
    pass

  @abc.abstractmethod
  def QuoteForRspFile(self, arg):
    pass


class PrecompiledHeader:
  """Abstract precompiled header settings class."""
  __metaclass__ = abc.ABCMeta

  @abc.abstractmethod
  def __init__(self):
    pass

  @abc.abstractmethod
  def GetFlagsModifications(self, input_flags, output, implicit, command,
                            cflags_c, cflags_cc, expand_special):
    pass

  @abc.abstractmethod
  def GetPchBuildCommands(self):
    pass

  @abc.abstractmethod
  def GetInclude(self):
    pass

  @abc.abstractmethod
  def GetObjDependencies(self, sources, output):
    pass


class CompilerSettings:
  """Abstract Compiler Settings class."""
  __metaclass__ = abc.ABCMeta

  @abc.abstractmethod
  def __init__(self):
    pass

  @abc.abstractmethod
  def GetArch(self, config):
    pass

  @abc.abstractmethod
  def GetCflags(self, config):
    pass

  @abc.abstractmethod
  def GetCflagsC(self, config):
    pass

  @abc.abstractmethod
  def GetCflagsCC(self, config):
    pass

  @abc.abstractmethod
  def GetCflagsObjectiveC(self, config):
    pass

  @abc.abstractmethod
  def GetCflagsObjectiveCC(self, config):
    pass

  @abc.abstractmethod
  def GetDefines(self, config):
    pass

  @abc.abstractmethod
  def GetLdFlags(self, config, **kwargs):
    pass

  @abc.abstractmethod
  def GetLibFlags(self, config, gyp_path_to_ninja):
    pass

  @abc.abstractmethod
  def GetRcFlags(self, config, gyp_path_to_ninja):
    pass

  @abc.abstractmethod
  def ProcessIncludeDirs(self, include_dirs, config_name):
    pass

  @abc.abstractmethod
  def ProcessLibraries(self, libraries, config_name):
    pass
