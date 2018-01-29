# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import hashlib
import multiprocessing
import os.path
import pipes
import platform
import re
import signal
import subprocess
import sys

from cobalt.build import config

import gyp
import gyp.common

# TODO: These should be replaced with calls to the abstract tool chain, when it
# is implemented on all supported platforms.
from gyp.msvs_emulation import EncodeRspFileList
from gyp.msvs_emulation import GenerateEnvironmentFiles
from gyp.msvs_emulation import MsvsSettings
import gyp.MSVSUtil as MSVSUtil
import gyp.xcode_emulation

from gyp.common import GetEnvironFallback
import gyp.ninja_syntax as ninja_syntax

if sys.platform == 'cygwin':
  import cygpath

from starboard.tools.toolchain import abstract

generator_default_variables = {
    'EXECUTABLE_PREFIX': '',
    'EXECUTABLE_SUFFIX': '',
    'STATIC_LIB_PREFIX': 'lib',
    'STATIC_LIB_SUFFIX': '.a',
    'SHARED_LIB_PREFIX': 'lib',

    # Gyp expects the following variables to be expandable by the build
    # system to the appropriate locations.  Ninja prefers paths to be
    # known at gyp time.  To resolve this, introduce special
    # variables starting with $! and $| (which begin with a $ so gyp knows it
    # should be treated specially, but is otherwise an invalid
    # ninja/shell variable) that are passed to gyp here but expanded
    # before writing out into the target .ninja files; see
    # ExpandSpecial.
    # $! is used for variables that represent a path and that can only appear at
    # the start of a string, while $| is used for variables that can appear
    # anywhere in a string.
    'INTERMEDIATE_DIR': '$!INTERMEDIATE_DIR',
    'SHARED_INTERMEDIATE_DIR': '$!PRODUCT_DIR/gen',
    'PRODUCT_DIR': '$!PRODUCT_DIR',
    'CONFIGURATION_NAME': '$|CONFIGURATION_NAME',

    # Special variables that may be used by gyp 'rule' targets.
    # We generate definitions for these variables on the fly when processing a
    # rule.
    'RULE_INPUT_ROOT': '${root}',
    'RULE_INPUT_DIRNAME': '${dirname}',
    'RULE_INPUT_PATH': '${source}',
    'RULE_INPUT_EXT': '${ext}',
    'RULE_INPUT_NAME': '${name}',
}

# Placates pylint.
generator_additional_non_configuration_keys = []
generator_additional_path_sections = []
generator_extra_sources_for_rules = []

generator_supports_multiple_toolsets = True

is_linux = platform.system() == 'Linux'
is_windows = platform.system() == 'Windows'

microsoft_flavors = ['win', 'win-win32', 'win-win32-lib', 'xb1', 'xb1-future',
    'xb1-youtubetv']
sony_flavors = ['ps3', 'ps4', 'ps4-vr']
windows_host_flavors = microsoft_flavors + sony_flavors


def GetToolchainOrNone(flavor):
  return config.GetPlatformConfig(flavor).GetToolchain()


def GetTargetToolchain(flavor):
  return config.GetPlatformConfig(flavor).GetTargetToolchain()


def GetHostToolchain(flavor):
  return config.GetPlatformConfig(flavor).GetHostToolchain()


def FindFirstInstanceOf(type, instances):
  try:
    return (instance for instance in instances
            if isinstance(instance, type)).next()
  except StopIteration:
    return None


def GetShell(flavor):
  shell = FindFirstInstanceOf(abstract.Shell, GetHostToolchain(flavor))
  assert shell, 'The host toolchain must provide a shell.'
  return shell


def GetNinjaRuleName(tool, toolset):
  if tool.IsPlatformAgnostic() or toolset == 'target':
    return tool.GetRuleName()
  return '{0}_{1}'.format(tool.GetRuleName(), toolset)


def GetConfigFlags(config, toolset, keyword):
  flags = config.get(keyword, [])
  if toolset == 'host':
    flags = config.get('{0}_host'.format(keyword), flags)
  return flags


def StripPrefix(arg, prefix):
  if arg.startswith(prefix):
    return arg[len(prefix):]
  return arg


def QuoteShellArgument(arg, flavor):
  """Quote a string such that it will be interpreted as a single argument
  by the shell."""
  # Rather than attempting to enumerate the bad shell characters, just
  # whitelist common OK ones and quote anything else.
  if re.match(r'^[a-zA-Z0-9_=.\\/-]+$', arg):
    return arg  # No quoting necessary.
  if GetToolchainOrNone(flavor):
    return GetToolchainOrNone(flavor).QuoteForRspFile(arg)
  elif flavor in sony_flavors:
    # Escape double quotes.
    return '"' + arg.replace('\"', '\\\"') + '"'
  return "'" + arg.replace("'", "'" + '"\'"' + "'") + "'"


def Define(d, flavor):
  """Takes a preprocessor define and returns a -D parameter that's ninja- and
  shell-escaped."""

  return QuoteShellArgument(ninja_syntax.escape('-D' + d), flavor)


def InvertRelativePath(path):
  """Given a relative path like foo/bar, return the inverse relative path:
  the path from the relative path back to the origin dir.

  E.g. os.path.normpath(os.path.join(path, InvertRelativePath(path)))
  should always produce the empty string."""

  if not path:
    return path
  # Only need to handle relative paths into subdirectories for now.
  assert '..' not in path, path
  depth = len(path.split(os.path.sep))
  return os.path.sep.join(['..'] * depth)


class Target:
  """Target represents the paths used within a single gyp target.

  Conceptually, building a single target A is a series of steps:

  1) actions/rules/copies  generates source/resources/etc.
  2) compiles              generates .o files
  3) link                  generates a binary (library/executable)
  4) bundle                merges the above in a mac bundle

  (Any of these steps can be optional.)

  From a build ordering perspective, a dependent target B could just
  depend on the last output of this series of steps.

  But some dependent commands sometimes need to reach inside the box.
  For example, when linking B it needs to get the path to the static
  library generated by A.

  This object stores those paths.  To keep things simple, member
  variables only store concrete paths to single files, while methods
  compute derived values like "the last output of the target".
  """

  def __init__(self, type):
    # Gyp type ("static_library", etc.) of this target.
    self.type = type
    # File representing whether any input dependencies necessary for
    # dependent actions have completed.
    self.preaction_stamp = None
    # File representing whether any input dependencies necessary for
    # dependent compiles have completed.
    self.precompile_stamp = None
    # File representing the completion of actions/rules/copies, if any.
    self.actions_stamp = None
    # Path to the output of the link step, if any.
    self.binary = None
    # Path to the file representing the completion of building the bundle,
    # if any.
    self.bundle = None
    # On Windows, incremental linking requires linking against all the .objs
    # that compose a .lib (rather than the .lib itself). That list is stored
    # here.
    self.component_objs = None
    # Windows/PS3 only. The import .lib is the output of a build step, but
    # because dependents only link against the lib (not both the lib and the
    # dll) we keep track of the import library here.
    # For PS3, this is the "stub" library.
    self.import_lib = None

  def Linkable(self):
    """Return true if this is a target that can be linked against."""
    return self.type in ('static_library', 'shared_library')

  def UsesToc(self, flavor):
    """Return true if the target should produce a restat rule based on a TOC
    file."""
    # For bundles, the .TOC should be produced for the binary, not for
    # FinalOutput(). But the naive approach would put the TOC file into the
    # bundle, so don't do this for bundles for now.
    if flavor in windows_host_flavors or self.bundle:
      return False
    return self.type in ('shared_library', 'loadable_module')

  def PreActionInput(self, flavor):
    """Return the path, if any, that should be used as a dependency of
    any dependent action step."""
    if self.UsesToc(flavor):
      return self.FinalOutput() + '.TOC'
    return self.FinalOutput() or self.preaction_stamp

  def PreCompileInput(self):
    """Return the path, if any, that should be used as a dependency of
    any dependent compile step."""
    return self.actions_stamp or self.precompile_stamp

  def FinalOutput(self):
    """Return the last output of the target, which depends on all prior
    steps."""
    return self.bundle or self.binary or self.actions_stamp


# A small discourse on paths as used within the Ninja build:
# All files we produce (both at gyp and at build time) appear in the
# build directory (e.g. out/Debug).
#
# Paths within a given .gyp file are always relative to the directory
# containing the .gyp file.  Call these "gyp paths".  This includes
# sources as well as the starting directory a given gyp rule/action
# expects to be run from.  We call the path from the source root to
# the gyp file the "base directory" within the per-.gyp-file
# NinjaWriter code.
#
# All paths as written into the .ninja files are relative to the build
# directory.  Call these paths "ninja paths".
#
# We translate between these two notions of paths with two helper
# functions:
#
# - GypPathToNinja translates a gyp path (i.e. relative to the .gyp file)
#   into the equivalent ninja path.
#
# - GypPathToUniqueOutput translates a gyp path into a ninja path to write
#   an output file; the result can be namespaced such that it is unique
#   to the input file name as well as the output target name.


class NinjaWriter:

  def __init__(self,
               qualified_target,
               target_outputs,
               base_dir,
               build_dir,
               output_file,
               flavor,
               case_sensitive_filesystem,
               abs_build_dir=None):
    """base_dir: path from source root to directory containing this gyp file,

              by gyp semantics, all input paths are relative to this
    build_dir: path from source root to build output
    abs_build_dir: absolute path to the build directory
    """
    self.case_sensitive_filesystem = case_sensitive_filesystem
    self.qualified_target = qualified_target
    self.target_outputs = target_outputs
    self.base_dir = base_dir
    self.build_dir = build_dir
    self.abs_build_dir = abs_build_dir
    self.ninja = ninja_syntax.Writer(output_file)
    self.flavor = flavor
    self.path_module = os.path

    self.abs_build_dir = abs_build_dir
    self.obj_ext = '.obj' if flavor == 'win' else '.o'
    if flavor in windows_host_flavors:
      # See docstring of msvs_emulation.GenerateEnvironmentFiles().
      self.win_env = {}
      for arch in ('x86', 'x64'):
        self.win_env[arch] = 'environment.' + arch

    # Relative path from build output dir to base dir.
    self.build_to_base = os.path.join(InvertRelativePath(build_dir), base_dir)
    # Relative path from base dir to build dir.
    self.base_to_build = os.path.join(InvertRelativePath(base_dir), build_dir)

  def ExpandSpecial(self, path, product_dir=None):
    """Expand specials like $!PRODUCT_DIR in |path|.

    If |product_dir| is None, assumes the cwd is already the product
    dir.  Otherwise, |product_dir| is the relative path to the product
    dir.
    """

    PRODUCT_DIR = '$!PRODUCT_DIR'
    if PRODUCT_DIR in path:
      if product_dir:
        path = path.replace(PRODUCT_DIR, product_dir)
      else:
        path = path.replace(PRODUCT_DIR + '/', '')
        path = path.replace(PRODUCT_DIR + '\\', '')
        path = path.replace(PRODUCT_DIR, '.')

    INTERMEDIATE_DIR = '$!INTERMEDIATE_DIR'
    if INTERMEDIATE_DIR in path:
      int_dir = self.GypPathToUniqueOutput('gen')
      # GypPathToUniqueOutput generates a path relative to the product dir,
      # so insert product_dir in front if it is provided.
      path = path.replace(INTERMEDIATE_DIR,
                          os.path.join(product_dir or '', int_dir))

    CONFIGURATION_NAME = '$|CONFIGURATION_NAME'
    path = path.replace(CONFIGURATION_NAME, self.config_name)

    return path

  def ExpandRuleVariables(self, path, root, dirname, source, ext, name):
    if self.flavor == 'win':
      path = GetToolchainOrNone(
          self.flavor).GetCompilerSettings().ConvertVSMacros(
              path, config=self.config_name)
    path = path.replace(generator_default_variables['RULE_INPUT_ROOT'], root)
    path = path.replace(generator_default_variables['RULE_INPUT_DIRNAME'],
                        dirname)
    path = path.replace(generator_default_variables['RULE_INPUT_PATH'], source)
    path = path.replace(generator_default_variables['RULE_INPUT_EXT'], ext)
    path = path.replace(generator_default_variables['RULE_INPUT_NAME'], name)
    return path

  def GypPathCaseCorrection(self, path):
    # Ninja's depfile handling gets confused when the case of a filename
    # changes on a case-insensitive file system. To work around that, always
    # convert filenames to lowercase on such file systems. See
    # https://github.com/martine/ninja/issues/402 for details.
    if self.case_sensitive_filesystem:
      return path
    else:
      return path.lower()

  def GypPathToNinja(self, path, env=None):
    """Translate a gyp path to a ninja path, optionally expanding environment
    variable references in |path| with |env|.

    See the above discourse on path conversions."""
    if env:
      if self.flavor == 'mac':
        path = gyp.xcode_emulation.ExpandEnvVars(path, env)
      elif GetToolchainOrNone(self.flavor):
        path = GetToolchainOrNone(self.flavor).ExpandEnvVars(path, env)
    if path.startswith('$!'):
      expanded = self.ExpandSpecial(path)
      if self.flavor == 'win':
        expanded = os.path.normpath(expanded)
      else:
        expanded = self.path_module.normpath(expanded)
      return self.GypPathCaseCorrection(expanded)
    if '$|' in path:
      path = self.ExpandSpecial(path)
    assert '$' not in path, path

    # TODO: this needs a proper fix.
    is_absolute = path.startswith('C:') or path.startswith(
        'c:') or path.startswith('/')
    if not is_absolute:
      path = self.path_module.normpath(os.path.join(self.build_to_base, path))

    return self.GypPathCaseCorrection(path)

  def GypPathToUniqueOutput(self, path, qualified=True):
    """Translate a gyp path to a ninja path for writing output.

    If qualified is True, qualify the resulting filename with the name
    of the target.  This is necessary when e.g. compiling the same
    path twice for two separate output targets.

    See the above discourse on path conversions."""

    path = self.ExpandSpecial(path)
    assert not path.startswith('$'), path

    # Translate the path following this scheme:
    #   Input: foo/bar.gyp, target targ, references baz/out.o
    #   Output: obj/foo/baz/targ.out.o (if qualified)
    #           obj/foo/baz/out.o (otherwise)
    #     (and obj.host instead of obj for cross-compiles)
    #
    # Why this scheme and not some other one?
    # 1) for a given input, you can compute all derived outputs by matching
    #    its path, even if the input is brought via a gyp file with '..'.
    # 2) simple files like libraries and stamps have a simple filename.

    obj = 'obj'
    if self.toolset != 'target':
      obj += '.' + self.toolset

    path_dir, path_basename = os.path.split(path)
    if qualified:
      path_basename = self.name + '.' + path_basename
    path = self.path_module.normpath(
        os.path.join(obj, self.base_dir, path_dir, path_basename))

    return self.GypPathCaseCorrection(path)

  def WriteCollapsedDependencies(self, name, targets):
    """Given a list of targets, return a path for a single file
    representing the result of building all the targets or None.

    Uses a stamp file if necessary."""

    assert targets == filter(None, targets), targets
    if len(targets) == 0:
      return None
    if len(targets) == 1:
      return targets[0]

    try:
      assert FindFirstInstanceOf(abstract.Stamp, GetHostToolchain(
          self.flavor)), 'Host toolchain must provide stamp tool.'
    except NotImplementedError:
      # Fall back to the legacy toolchain.
      pass

    stamp_output = self.GypPathToUniqueOutput(name + '.stamp')
    self.ninja.build(stamp_output, 'stamp', targets)
    self.ninja.newline()
    return stamp_output

  def WriteSpec(self, spec, config_name, generator_flags):
    """The main entry point for NinjaWriter: write the build rules for a spec.

    Returns a Target object, which represents the output paths for this spec.
    Returns None if there are no outputs (e.g. a settings-only 'none' type
    target)."""

    self.config_name = config_name
    self.name = spec['target_name']
    self.toolset = spec['toolset']
    config = spec['configurations'][config_name]
    self.target = Target(spec['type'])
    self.is_standalone_static_library = bool(
        spec.get('standalone_static_library', 0))

    self.is_mac_bundle = gyp.xcode_emulation.IsMacBundle(self.flavor, spec)
    self.xcode_settings = None
    if self.flavor == 'mac':
      self.xcode_settings = gyp.xcode_emulation.XcodeSettings(spec)
    if (self.flavor in windows_host_flavors and is_windows):
      if self.flavor in sony_flavors:
        self.msvs_settings = gyp.msvs_emulation.MsvsSettings(
            spec, generator_flags)
        arch = self.msvs_settings.GetArch(config_name)
      else:
        GetToolchainOrNone(self.flavor).InitCompilerSettings(
            spec, **{'generator_flags': generator_flags})
        arch = GetToolchainOrNone(
            self.flavor).GetCompilerSettings().GetArch(config_name)
      self.ninja.variable('arch', self.win_env[arch])
      None

    # Compute predepends for all rules.
    # actions_depends is the dependencies this target depends on before running
    # any of its action/rule/copy steps.
    # compile_depends is the dependencies this target depends on before running
    # any of its compile steps.
    actions_depends = []
    compile_depends = []
    # TODO(evan): it is rather confusing which things are lists and which
    # are strings.  Fix these.
    if 'dependencies' in spec:
      for dep in spec['dependencies']:
        if dep in self.target_outputs:
          target = self.target_outputs[dep]
          actions_depends.append(target.PreActionInput(self.flavor))
          compile_depends.append(target.PreCompileInput())
      actions_depends = filter(None, actions_depends)
      compile_depends = filter(None, compile_depends)
      actions_depends = self.WriteCollapsedDependencies('actions_depends',
                                                        actions_depends)
      compile_depends = self.WriteCollapsedDependencies('compile_depends',
                                                        compile_depends)
      self.target.preaction_stamp = actions_depends
      self.target.precompile_stamp = compile_depends

    # Write out actions, rules, and copies.  These must happen before we
    # compile any sources, so compute a list of predependencies for sources
    # while we do it.
    extra_sources = []
    mac_bundle_depends = []
    self.target.actions_stamp = self.WriteActionsRulesCopies(
        spec, extra_sources, actions_depends, mac_bundle_depends)

    # If we have actions/rules/copies, we depend directly on those, but
    # otherwise we depend on dependent target's actions/rules/copies etc.
    # We never need to explicitly depend on previous target's link steps,
    # because no compile ever depends on them.
    compile_depends_stamp = (self.target.actions_stamp or compile_depends)

    # Write out the compilation steps, if any.
    link_deps = []
    sources = spec.get('sources', []) + extra_sources
    if sources:
      pch = None
      if GetToolchainOrNone(self.flavor):
        GetToolchainOrNone(self.flavor).VerifyMissingSources(
            sources, **{
                'build_dir': self.abs_build_dir,
                'generator_flags': generator_flags,
                'gyp_path_to_ninja': self.GypPathToNinja
            })
        pch = GetToolchainOrNone(self.flavor).GetPrecompiledHeader(
            **{
                'settings':
                    GetToolchainOrNone(self.flavor).GetCompilerSettings(),
                'config':
                    config_name,
                'gyp_path_to_ninja':
                    self.GypPathToNinja,
                'gyp_path_to_unique_output':
                    self.GypPathToUniqueOutput,
                'obj_ext':
                    self.obj_ext
            })
      else:
        pch = gyp.xcode_emulation.MacPrefixHeader(
            self.xcode_settings, self.GypPathToNinja,
            lambda path, lang: self.GypPathToUniqueOutput(path + '-' + lang))
      link_deps = self.WriteSources(config_name, config, sources,
                                    compile_depends_stamp, pch, spec)
      # Some actions/rules output 'sources' that are already object files.
      link_deps += [
          self.GypPathToNinja(f) for f in sources if f.endswith(self.obj_ext)
      ]

    if (self.flavor in microsoft_flavors and
        self.target.type == 'static_library'):
      self.target.component_objs = link_deps

    # Write out a link step, if needed.
    output = None
    if link_deps or self.target.actions_stamp or actions_depends:
      output = self.WriteTarget(spec, config_name, config, link_deps,
                                self.target.actions_stamp or actions_depends)
      if self.is_mac_bundle:
        mac_bundle_depends.append(output)

    # Bundle all of the above together, if needed.
    if self.is_mac_bundle:
      output = self.WriteMacBundle(spec, mac_bundle_depends)

    if not output:
      return None

    assert self.target.FinalOutput(), output
    return self.target

  def _WinIdlRule(self, source, prebuild, outputs):
    """Handle the implicit VS .idl rule for one source file. Fills |outputs|
    with files that are generated."""
    outdir, output, vars, flags = GetToolchainOrNone(
        self.flavor).GetCompilerSettings().GetIdlBuildData(
            source, self.config_name)
    outdir = self.GypPathToNinja(outdir)

    def fix_path(path, rel=None):
      path = os.path.join(outdir, path)
      dirname, basename = os.path.split(source)
      root, ext = os.path.splitext(basename)
      path = self.ExpandRuleVariables(path, root, dirname, source, ext,
                                      basename)
      if rel:
        path = os.path.relpath(path, rel)
      return path

    vars = [(name, fix_path(value, outdir)) for name, value in vars]
    output = [fix_path(p) for p in output]
    vars.append(('outdir', outdir))
    vars.append(('idlflags', flags))
    input = self.GypPathToNinja(source)
    self.ninja.build(output, 'idl', input, variables=vars, order_only=prebuild)
    outputs.extend(output)

  def WriteWinIdlFiles(self, spec, prebuild):
    """Writes rules to match MSVS's implicit idl handling."""
    assert GetToolchainOrNone(self.flavor)
    if GetToolchainOrNone(
        self.flavor).GetCompilerSettings().HasExplicitIdlRules(spec):
      return []
    outputs = []
    for source in filter(lambda x: x.endswith('.idl'), spec['sources']):
      self._WinIdlRule(source, prebuild, outputs)
    return outputs

  def WriteActionsRulesCopies(self, spec, extra_sources, prebuild,
                              mac_bundle_depends):
    """Write out the Actions, Rules, and Copies steps.  Return a path
    representing the outputs of these steps."""
    outputs = []
    extra_mac_bundle_resources = []

    if 'actions' in spec:
      outputs += self.WriteActions(spec['actions'], extra_sources, prebuild,
                                   extra_mac_bundle_resources)
    if 'rules' in spec:
      outputs += self.WriteRules(spec['rules'], extra_sources, prebuild,
                                 extra_mac_bundle_resources)
    if 'copies' in spec:
      outputs += self.WriteCopies(spec['copies'], prebuild, mac_bundle_depends)

    stamp = self.WriteCollapsedDependencies('actions_rules_copies', outputs)

    if self.is_mac_bundle:
      mac_bundle_resources = spec.get('mac_bundle_resources', []) + \
                             extra_mac_bundle_resources
      self.WriteMacBundleResources(mac_bundle_resources, mac_bundle_depends)
      self.WriteMacInfoPlist(mac_bundle_depends)

    return stamp

  def GenerateDescription(self, verb, message, fallback):
    """Generate and return a description of a build step.

    |verb| is the short summary, e.g. ACTION or RULE.
    |message| is a hand-written description, or None if not available.
    |fallback| is the gyp-level name of the step, usable as a fallback.
    """
    if self.toolset != 'target':
      verb += '(%s)' % self.toolset
    if message:
      return '%s %s' % (verb, self.ExpandSpecial(message))
    else:
      return '%s %s: %s' % (verb, self.name, fallback)

  def IsCygwinRule(self, action):
    if self.flavor in sony_flavors:
      return str(action.get('msvs_cygwin_shell', 1)) != '0'
    return False

  def WriteActions(self, actions, extra_sources, prebuild,
                   extra_mac_bundle_resources):
    # Actions cd into the base directory.
    env = self.GetSortedXcodeEnv()
    if self.flavor == 'win':
      env = GetToolchainOrNone(self.flavor).GetCompilerSettings().GetVSMacroEnv(
          '$!PRODUCT_DIR', config=self.config_name)
    all_outputs = []
    for action in actions:
      # First write out a rule for the action.
      name = '%s_%s' % (action['action_name'],
                        hashlib.md5(self.qualified_target).hexdigest())
      description = self.GenerateDescription('ACTION',
                                             action.get('message', None), name)
      is_cygwin = self.IsCygwinRule(action)
      args = action['action']
      rule_name, _ = self.WriteNewNinjaRule(
          name, args, description, is_cygwin, env=env)

      inputs = [self.GypPathToNinja(i, env) for i in action['inputs']]
      if int(action.get('process_outputs_as_sources', False)):
        extra_sources += action['outputs']
      if int(action.get('process_outputs_as_mac_bundle_resources', False)):
        extra_mac_bundle_resources += action['outputs']
      outputs = [self.GypPathToNinja(o, env) for o in action['outputs']]

      # Then write out an edge using the rule.
      self.ninja.build(outputs, rule_name, inputs, order_only=prebuild)
      all_outputs += outputs

      self.ninja.newline()

    return all_outputs

  def WriteRules(self, rules, extra_sources, prebuild,
                 extra_mac_bundle_resources):
    env = self.GetSortedXcodeEnv()
    all_outputs = []
    for rule in rules:
      # First write out a rule for the rule action.
      name = '%s_%s' % (rule['rule_name'],
                        hashlib.md5(self.qualified_target).hexdigest())
      # Skip a rule with no action and no inputs.
      if 'action' not in rule and not rule.get('rule_sources', []):
        continue
      args = rule['action']
      description = self.GenerateDescription(
          'RULE',
          rule.get('message', None),
          ('%s ' + generator_default_variables['RULE_INPUT_PATH']) % name)
      is_cygwin = self.IsCygwinRule(rule)
      rule_name, args = self.WriteNewNinjaRule(
          name, args, description, is_cygwin, env=env)

      # TODO: if the command references the outputs directly, we should
      # simplify it to just use $out.

      # Rules can potentially make use of some special variables which
      # must vary per source file.
      # Compute the list of variables we'll need to provide.
      special_locals = ('source', 'root', 'dirname', 'ext', 'name')
      needed_variables = set(['source'])
      for argument in args:
        for var in special_locals:
          if ('${%s}' % var) in argument:
            needed_variables.add(var)

      def cygwin_munge(path):
        if is_cygwin:
          return path.replace('\\', '/')
        return path

      # For each source file, write an edge that generates all the outputs.
      for source in rule.get('rule_sources', []):
        dirname, basename = os.path.split(source)
        root, ext = os.path.splitext(basename)

        # Gather the list of inputs and outputs, expanding $vars if possible.
        outputs = [
            self.ExpandRuleVariables(o, root, dirname, source, ext, basename)
            for o in rule['outputs']
        ]
        inputs = [
            self.ExpandRuleVariables(i, root, dirname, source, ext, basename)
            for i in rule.get('inputs', [])
        ]

        if int(rule.get('process_outputs_as_sources', False)):
          extra_sources += outputs
        if int(rule.get('process_outputs_as_mac_bundle_resources', False)):
          extra_mac_bundle_resources += outputs

        extra_bindings = []
        for var in needed_variables:
          if var == 'root':
            extra_bindings.append(('root', cygwin_munge(root)))
          elif var == 'dirname':
            extra_bindings.append(('dirname', cygwin_munge(dirname)))
          elif var == 'source':
            # '$source' is a parameter to the rule action, which means
            # it shouldn't be converted to a Ninja path.  But we don't
            # want $!PRODUCT_DIR in there either.
            source_expanded = self.ExpandSpecial(source, self.base_to_build)
            extra_bindings.append(('source', cygwin_munge(source_expanded)))
          elif var == 'ext':
            extra_bindings.append(('ext', ext))
          elif var == 'name':
            extra_bindings.append(('name', cygwin_munge(basename)))
          else:
            assert var == None, repr(var)

        inputs = [self.GypPathToNinja(i, env) for i in inputs]
        outputs = [self.GypPathToNinja(o, env) for o in outputs]
        extra_bindings.append(('unique_name',
                               hashlib.md5(outputs[0]).hexdigest()))
        self.ninja.build(
            outputs,
            rule_name,
            self.GypPathToNinja(source),
            implicit=inputs,
            order_only=prebuild,
            variables=extra_bindings)

        all_outputs.extend(outputs)

    return all_outputs

  def WriteCopy(self, src, dst, prebuild, env, mac_bundle_depends):
    try:
      assert FindFirstInstanceOf(abstract.Copy, GetHostToolchain(
          self.flavor)), 'Host toolchain must provide copy tool.'
    except NotImplementedError:
      # Fall back to the legacy toolchain.
      pass

    dst = self.GypPathToNinja(dst, env)
    # Renormalize with the separator character of the os on which ninja will run
    dst = self.path_module.normpath(dst)

    self.ninja.build(dst, 'copy', src, order_only=prebuild)
    if self.is_mac_bundle:
      # gyp has mac_bundle_resources to copy things into a bundle's
      # Resources folder, but there's no built-in way to copy files to other
      # places in the bundle. Hence, some targets use copies for this. Check
      # if this file is copied into the current bundle, and if so add it to
      # the bundle depends so that dependent targets get rebuilt if the copy
      # input changes.
      if dst.startswith(self.xcode_settings.GetBundleContentsFolderPath()):
        mac_bundle_depends.append(dst)
    return [dst]

  def WriteCopies(self, copies, prebuild, mac_bundle_depends):
    outputs = []
    env = self.GetSortedXcodeEnv()
    for copy in copies:
      for path in copy['files']:
        # Normalize the path so trailing slashes don't confuse us.
        path = os.path.normpath(path)
        destination = copy['destination']
        basename = os.path.split(path)[1]

        # call GypPathToNinja() to resolve any special GYP $ tokens in src.
        # And figure out where this directory actually is on disk.
        ninja_path = self.GypPathToNinja(path)
        joined_path = os.path.join(self.abs_build_dir, ninja_path)
        joined_path = os.path.normpath(joined_path)

        # If src is a directory, expand it recursively,
        # so we have a build rule for every file.
        if os.path.isdir(joined_path):
          for root, dirs, files in os.walk(joined_path):
            rel_root = os.path.relpath(root, self.abs_build_dir)

            for f in files:
              src = self.GypPathToNinja(os.path.join(rel_root, f), env)
              common_prefix = os.path.commonprefix([joined_path, root])
              subdir = root[len(common_prefix) + 1:]

              dst = os.path.join(destination, basename, subdir, f)
              outputs += self.WriteCopy(src, dst, prebuild, env,
                                        mac_bundle_depends)
        else:
          src = self.GypPathToNinja(path, env)
          dst = os.path.join(destination, basename)
          outputs += self.WriteCopy(src, dst, prebuild, env, mac_bundle_depends)

    return outputs

  def WriteMacBundleResources(self, resources, bundle_depends):
    """Writes ninja edges for 'mac_bundle_resources'."""
    for output, res in gyp.xcode_emulation.GetMacBundleResources(
        self.ExpandSpecial(generator_default_variables['PRODUCT_DIR']),
        self.xcode_settings, map(self.GypPathToNinja, resources)):
      self.ninja.build(
          output,
          'mac_tool',
          res,
          variables=[('mactool_cmd', 'copy-bundle-resource')])
      bundle_depends.append(output)

  def WriteMacInfoPlist(self, bundle_depends):
    """Write build rules for bundle Info.plist files."""
    info_plist, out, defines, extra_env = gyp.xcode_emulation.GetMacInfoPlist(
        self.ExpandSpecial(generator_default_variables['PRODUCT_DIR']),
        self.xcode_settings, self.GypPathToNinja)
    if not info_plist:
      return
    if defines:
      # Create an intermediate file to store preprocessed results.
      intermediate_plist = self.GypPathToUniqueOutput(
          os.path.basename(info_plist))
      defines = ' '.join([Define(d, self.flavor) for d in defines])
      info_plist = self.ninja.build(
          intermediate_plist,
          'infoplist',
          info_plist,
          variables=[('defines', defines)])

    env = self.GetSortedXcodeEnv(additional_settings=extra_env)
    env = self.ComputeExportEnvString(env)

    self.ninja.build(
        out,
        'mac_tool',
        info_plist,
        variables=[('mactool_cmd', 'copy-info-plist'), ('env', env)])
    bundle_depends.append(out)

  def WriteSources(self, config_name, config, sources, predepends,
                   precompiled_header, spec):
    """Write build rules to compile all of |sources|."""

    try:
      shell = GetShell(self.flavor)

      if self.toolset == 'target':
        toolchain = GetTargetToolchain(self.flavor)
      else:
        toolchain = GetHostToolchain(self.flavor)

      defines = config.get('defines', [])
      include_dirs = [
          self.GypPathToNinja(include_dir)
          for include_dir in config.get('include_dirs', [])
      ]

      # TODO: This code emulates legacy toolchain behavior. We need to migrate
      #       to single-responsibility, toolchain-independent GYP keywords as
      #       per abstract toolchain design doc.
      cflags = GetConfigFlags(config, self.toolset, 'cflags')
      cflags_c = GetConfigFlags(config, self.toolset, 'cflags_c')
      cflags_cc = GetConfigFlags(config, self.toolset, 'cflags_cc')
      cflags_mm = GetConfigFlags(config, self.toolset, 'cflags_mm')

      c_compiler = FindFirstInstanceOf(abstract.CCompiler, toolchain)
      if c_compiler:
        c_compiler_flags = c_compiler.GetFlags(defines, include_dirs,
                                               cflags + cflags_c)
        self.ninja.variable(
            '{0}_flags'.format(GetNinjaRuleName(c_compiler, self.toolset)),
            shell.Join(c_compiler_flags))

      cxx_compiler = FindFirstInstanceOf(abstract.CxxCompiler, toolchain)
      if cxx_compiler:
        cxx_compiler_flags = cxx_compiler.GetFlags(defines, include_dirs,
                                                   cflags + cflags_cc)
        self.ninja.variable(
            '{0}_flags'.format(GetNinjaRuleName(cxx_compiler, self.toolset)),
            shell.Join(cxx_compiler_flags))

      objcxx_compiler = FindFirstInstanceOf(abstract.ObjectiveCxxCompiler,
                                            toolchain)
      if objcxx_compiler:
        objcxx_compiler_flags = objcxx_compiler.GetFlags(defines, include_dirs,
                                                         cflags + cflags_cc +
                                                         cflags_mm)
        self.ninja.variable(
            '{0}_flags'.format(GetNinjaRuleName(objcxx_compiler, self.toolset)),
            shell.Join(objcxx_compiler_flags))

      assembler = FindFirstInstanceOf(abstract.AssemblerWithCPreprocessor,
                                      toolchain)
      if assembler:
        assembler_flags = assembler.GetFlags(defines, include_dirs,
                                             cflags + cflags_c)
        self.ninja.variable(
            '{0}_flags'.format(GetNinjaRuleName(assembler, self.toolset)),
            shell.Join(assembler_flags))

      self.ninja.newline()

      outputs = []
      for source in sources:
        _, extension = os.path.splitext(source)
        if extension in ['.c']:
          assert c_compiler, ('Toolchain must provide C compiler in order to '
                              'build {0} for {1} platform.').format(
                                  source, self.toolset)
          rule_name = GetNinjaRuleName(c_compiler, self.toolset)
        elif extension in ['.cc', '.cpp', '.cxx']:
          assert cxx_compiler, ('Toolchain must provide C++ compiler in order '
                                'to build {0} for {1} platform.').format(
                                    source, self.toolset)
          rule_name = GetNinjaRuleName(cxx_compiler, self.toolset)
        elif extension in ['.mm']:
          assert objcxx_compiler, ('Toolchain must provide Objective-C++ '
                                   'compiler in order to build {0} for {1} '
                                   'platform.').format(source, self.toolset)
          rule_name = GetNinjaRuleName(objcxx_compiler, self.toolset)
        elif extension in ['.S', '.s']:
          assert assembler, ('Toolchain must provide assembler in order to '
                             'build {0} for {1} platform.').format(
                                 source, self.toolset)
          rule_name = GetNinjaRuleName(assembler, self.toolset)
        else:
          rule_name = None

        if rule_name:
          input = self.GypPathToNinja(source)
          output = '{0}.o'.format(self.GypPathToUniqueOutput(source))
          self.ninja.build(
              output,
              rule_name,
              input,
              implicit=None,  # TODO: Implemenet precompiled headers.
              order_only=predepends)
          outputs.append(output)

    except NotImplementedError:
      # Fall back to the legacy toolchain.

      if self.toolset == 'host':
        self.ninja.variable('ar', '$ar_host')
        self.ninja.variable('cc', '$cc_host')
        self.ninja.variable('cxx', '$cxx_host')
        self.ninja.variable('ld', '$ld_host')

      extra_defines = []
      if self.flavor == 'mac':
        cflags = self.xcode_settings.GetCflags(config_name)
        cflags_c = self.xcode_settings.GetCflagsC(config_name)
        cflags_cc = self.xcode_settings.GetCflagsCC(config_name)
        cflags_objc = ['$cflags_c'] + \
                      self.xcode_settings.GetCflagsObjC(config_name)
        cflags_objcc = ['$cflags_cc'] + \
                       self.xcode_settings.GetCflagsObjCC(config_name)
      elif GetToolchainOrNone(self.flavor):
        cflags = GetToolchainOrNone(
            self.flavor).GetCompilerSettings().GetCflags(config_name)
        cflags_c = GetToolchainOrNone(
            self.flavor).GetCompilerSettings().GetCflagsC(config_name)
        cflags_cc = GetToolchainOrNone(
            self.flavor).GetCompilerSettings().GetCflagsCC(config_name)
        extra_defines = GetToolchainOrNone(
            self.flavor).GetCompilerSettings().GetDefines(config_name)
        obj = 'obj'
        if self.toolset != 'target':
          obj += '.' + self.toolset
        pdbpath = os.path.normpath(
            os.path.join(obj, self.base_dir, self.name + '.pdb'))
        self.WriteVariableList('pdbname', [pdbpath])
        self.WriteVariableList('pchprefix', [self.name])
      else:
        cflags = config.get('cflags', [])
        cflags_c = config.get('cflags_c', [])
        cflags_cc = config.get('cflags_cc', [])

      cflags_host = config.get('cflags_host', cflags)
      cflags_c_host = config.get('cflags_c_host', cflags_c)
      cflags_cc_host = config.get('cflags_cc_host', cflags_cc)

      defines = config.get('defines', []) + extra_defines
      if GetToolchainOrNone(self.flavor):
        self.WriteVariableList('defines', [
            GetToolchainOrNone(self.flavor).Define(d) for d in defines
        ])
      else:
        self.WriteVariableList('defines',
                               [Define(d, self.flavor) for d in defines])
      if GetToolchainOrNone(self.flavor):
        self.WriteVariableList('rcflags', [
            QuoteShellArgument(self.ExpandSpecial(f), self.flavor)
            for f in GetToolchainOrNone(self.flavor).GetCompilerSettings()
            .GetRcFlags(config_name, self.GypPathToNinja)
        ])

      include_dirs = config.get('include_dirs', [])
      include_dirs += config.get('include_dirs_' + self.toolset, [])

      if GetToolchainOrNone(self.flavor):
        include_dirs = GetToolchainOrNone(
            self.flavor).GetCompilerSettings().ProcessIncludeDirs(
                include_dirs, config_name)
        self.WriteVariableList('includes', [
            '/I' + GetToolchainOrNone(self.flavor).QuoteForRspFile(
                self.GypPathToNinja(i)) for i in include_dirs
        ])
      else:
        self.WriteVariableList('includes', [
            QuoteShellArgument('-I' + self.GypPathToNinja(i), self.flavor)
            for i in include_dirs
        ])

      pch_commands = precompiled_header.GetPchBuildCommands()
      if self.flavor == 'mac':
        self.WriteVariableList('cflags_pch_c',
                               [precompiled_header.GetInclude('c')])
        self.WriteVariableList('cflags_pch_cc',
                               [precompiled_header.GetInclude('cc')])
        self.WriteVariableList('cflags_pch_objc',
                               [precompiled_header.GetInclude('m')])
        self.WriteVariableList('cflags_pch_objcc',
                               [precompiled_header.GetInclude('mm')])

      self.WriteVariableList('cflags', map(self.ExpandSpecial, cflags))
      self.WriteVariableList('cflags_c', map(self.ExpandSpecial, cflags_c))
      self.WriteVariableList('cflags_cc', map(self.ExpandSpecial, cflags_cc))

      self.WriteVariableList('cflags_host', map(self.ExpandSpecial,
                                                cflags_host))
      self.WriteVariableList('cflags_c_host',
                             map(self.ExpandSpecial, cflags_c_host))
      self.WriteVariableList('cflags_cc_host',
                             map(self.ExpandSpecial, cflags_cc_host))

      if self.flavor == 'mac':
        self.WriteVariableList('cflags_objc',
                               map(self.ExpandSpecial, cflags_objc))
        self.WriteVariableList('cflags_objcc',
                               map(self.ExpandSpecial, cflags_objcc))
      self.ninja.newline()

      outputs = []
      for source in sources:
        filename, ext = os.path.splitext(source)
        ext = ext[1:]
        obj_ext = self.obj_ext
        if ext in ('cc', 'cpp', 'cxx'):
          command = 'cxx'
        elif ext == 'c' or (ext == 'S' and self.flavor != 'win'):
          command = 'cc'
        elif ext == 's' and self.flavor != 'win':  # Doesn't generate .o.d files.
          command = 'cc_s'
        elif (self.flavor == 'win' and ext == 'asm' and GetToolchainOrNone(
            self.flavor).GetCompilerSettings().GetArch(config_name) == 'x86' and
              not GetToolchainOrNone(
                  self.flavor).GetCompilerSettings().HasExplicitAsmRules(spec)):
          # Asm files only get auto assembled for x86 (not x64).
          command = 'asm'
          # Add the _asm suffix as msvs is capable of handling .cc and
          # .asm files of the same name without collision.
          obj_ext = '_asm.obj'
        elif self.flavor == 'mac' and ext == 'm':
          command = 'objc'
        elif self.flavor == 'mac' and ext == 'mm':
          command = 'objcxx'
        elif self.flavor in microsoft_flavors and ext == 'rc':
          # TODO: Starboardize this.
          command = 'rc'
          obj_ext = '.res'
        else:
          # Ignore unhandled extensions.
          continue
        if self.toolset != 'target':
          command += '_' + self.toolset

        input = self.GypPathToNinja(source)
        output = self.GypPathToUniqueOutput(filename + obj_ext)
        implicit = precompiled_header.GetObjDependencies([input], [output])
        variables = []
        if GetToolchainOrNone(self.flavor):
          (variables, output,
           implicit) = precompiled_header.GetFlagsModifications(
               input, output, implicit, command, cflags_c, cflags_cc,
               self.ExpandSpecial)
        self.ninja.build(
            output,
            command,
            input,
            implicit=[gch for _, _, gch in implicit],
            order_only=predepends,
            variables=variables)
        outputs.append(output)

      self.WritePchTargets(pch_commands)

    self.ninja.newline()
    return outputs

  def WritePchTargets(self, pch_commands):
    """Writes ninja rules to compile prefix headers."""
    if not pch_commands:
      return

    for gch, lang_flag, lang, input in pch_commands:
      var_name = {
          'c': 'cflags_pch_c',
          'cc': 'cflags_pch_cc',
          'm': 'cflags_pch_objc',
          'mm': 'cflags_pch_objcc',
      }[lang]

      map = {
          'c': 'cc',
          'cc': 'cxx',
          'm': 'objc',
          'mm': 'objcxx',
      }
      cmd = map.get(lang)
      self.ninja.build(gch, cmd, input, variables=[(var_name, lang_flag)])

  def WriteLink(self, spec, config_name, config, link_deps):
    """Write out a link step. Fills out target.binary. """

    try:
      if self.toolset == 'target':
        toolchain = GetTargetToolchain(self.flavor)
      else:
        toolchain = GetHostToolchain(self.flavor)

      shell = GetShell(self.flavor)

      target_type = spec['type']
      if target_type == 'executable':
        executable_linker = FindFirstInstanceOf(abstract.ExecutableLinker,
                                                toolchain)
        assert executable_linker, ('Toolchain must provide executable linker '
                                   'for {0} platform.').format(self.toolset)

        rule_name = GetNinjaRuleName(executable_linker, self.toolset)

        # TODO: This code emulates legacy toolchain behavior. We need to migrate
        #       to single-responsibility, toolchain-independent GYP keywords as
        #       per abstract toolchain design doc.
        libraries_keyword = 'libraries{0}'.format('_host' if self.toolset ==
                                                  'host' else '')
        libraries = spec.get(libraries_keyword, []) + config.get(
            libraries_keyword, [])
        ldflags = gyp.common.uniquer(
            map(self.ExpandSpecial,
                GetConfigFlags(config, self.toolset, 'ldflags') + libraries))

        executable_linker_flags = executable_linker.GetFlags(ldflags)
        self.ninja.variable('{0}_flags'.format(rule_name),
                            shell.Join(executable_linker_flags))
      else:
        raise Exception('Target type {0} is not supported for target {1}.'
            .format(target_type, spec['target_name']))

      order_only_deps = set()

      if 'dependencies' in spec:
        # Two kinds of dependencies:
        # - Linkable dependencies (like a .a or a .so): add them to the link
        #   line.
        # - Non-linkable dependencies (like a rule that generates a file
        #   and writes a stamp file): add them to implicit_deps or
        #   order_only_deps
        extra_link_deps = []
        for dep in spec['dependencies']:
          target = self.target_outputs.get(dep)
          if not target:
            continue
          linkable = target.Linkable()
          if linkable:
            extra_link_deps.append(target.binary)

          final_output = target.FinalOutput()
          if not linkable or final_output != target.binary:
            order_only_deps.add(final_output)

        # dedup the extra link deps while preserving order
        seen = set()
        extra_link_deps = [
            x for x in extra_link_deps if x not in seen and not seen.add(x)
        ]

        link_deps.extend(extra_link_deps)

      output = self.ComputeOutput(spec)
      self.target.binary = output

      self.ninja.build(
          output, rule_name, link_deps, order_only=list(order_only_deps))

    except NotImplementedError:
      # Fall back to the legacy toolchain.

      command = {
          'executable': 'link',
          'loadable_module': 'solink_module',
          'shared_library': 'solink',
      }[spec['type']]

      implicit_deps = set()
      order_only_deps = set()
      solibs = set()

      if 'dependencies' in spec:
        # Two kinds of dependencies:
        # - Linkable dependencies (like a .a or a .so): add them to the link
        #   line.
        # - Non-linkable dependencies (like a rule that generates a file
        #   and writes a stamp file): add them to implicit_deps or
        #   order_only_deps
        extra_link_deps = []
        for dep in spec['dependencies']:
          target = self.target_outputs.get(dep)
          if not target:
            continue
          linkable = target.Linkable()
          # TODO: Starboardize.
          if linkable:
            if (self.flavor in microsoft_flavors and target.component_objs and
                GetToolchainOrNone(self.flavor).GetCompilerSettings()
                .IsUseLibraryDependencyInputs(config_name)):
              extra_link_deps.extend(target.component_objs)
            elif (self.flavor in (microsoft_flavors + ['ps3']) and
                  target.import_lib):
              extra_link_deps.append(target.import_lib)
            elif target.UsesToc(self.flavor):
              solibs.add(target.binary)
              implicit_deps.add(target.binary + '.TOC')
            else:
              extra_link_deps.append(target.binary)

          final_output = target.FinalOutput()
          if not linkable or final_output != target.binary:
            order_only_deps.add(final_output)

        # dedup the extra link deps while preserving order
        seen = set()
        extra_link_deps = [
            x for x in extra_link_deps if x not in seen and not seen.add(x)
        ]

        link_deps.extend(extra_link_deps)

      extra_bindings = []
      if self.is_mac_bundle:
        output = self.ComputeMacBundleBinaryOutput()
      else:
        output = self.ComputeOutput(spec)
        extra_bindings.append(('postbuilds', self.GetPostbuildCommand(
            spec, output, output)))

      if self.flavor == 'mac':
        ldflags = self.xcode_settings.GetLdflags(
            config_name,
            self.ExpandSpecial(generator_default_variables['PRODUCT_DIR']),
            self.GypPathToNinja)
      elif GetToolchainOrNone(self.flavor):
        libflags = GetToolchainOrNone(
            self.flavor).GetCompilerSettings().GetLibFlags(
                config_name, self.GypPathToNinja)
        self.WriteVariableList(
            'libflags', gyp.common.uniquer(map(self.ExpandSpecial, libflags)))
        is_executable = spec['type'] == 'executable'
        manifest_name = self.GypPathToUniqueOutput(
            self.ComputeOutputFileName(spec))
        ldflags, manifest_files = GetToolchainOrNone(
            self.flavor).GetCompilerSettings().GetLdFlags(
                config_name, **{
                    'gyp_path_to_ninja': self.GypPathToNinja,
                    'expand_special': self.ExpandSpecial,
                    'manifest_name': manifest_name,
                    'is_executable': is_executable
                })
        self.WriteVariableList('manifests', manifest_files)
      else:
        ldflags = config.get('ldflags', [])
        ldflags_host = config.get('ldflags_host', ldflags)

      self.WriteVariableList(
          'ldflags', gyp.common.uniquer(map(self.ExpandSpecial, ldflags)))
      if ('ldflags_host' in locals()):
        self.WriteVariableList(
            'ldflags_host',
            gyp.common.uniquer(map(self.ExpandSpecial, ldflags_host)))

      if self.toolset == 'host':
        libs = spec.get('libraries_host', [])
        libs.extend(config.get('libraries_host', []))
      else:
        libs = spec.get('libraries', [])
        libs.extend(config.get('libraries', []))

      libraries = gyp.common.uniquer(map(self.ExpandSpecial, libs))

      if self.flavor == 'mac':
        libraries = self.xcode_settings.AdjustLibraries(libraries)
      elif GetToolchainOrNone(self.flavor):
        libraries = GetToolchainOrNone(
            self.flavor).GetCompilerSettings().ProcessLibraries(libraries)
      self.WriteVariableList('libs', libraries)

      self.target.binary = output

      if command in ('solink', 'solink_module'):
        extra_bindings.append(('soname', os.path.split(output)[1]))
        extra_bindings.append(('lib',
                               gyp.common.EncodePOSIXShellArgument(output)))
        # TODO: Starboardize.
        if self.flavor in microsoft_flavors:
          extra_bindings.append(('dll', output))
          if '/NOENTRY' not in ldflags:
            self.target.import_lib = output + '.lib'
            extra_bindings.append(('implibflag',
                                   '/IMPLIB:%s' % self.target.import_lib))
            output = [output, self.target.import_lib]
        elif self.flavor in ['ps3']:
          # Tell Ninja we'll be generating a .sprx and a stub library.
          # Bind the variable '$prx' to our output binary so we can
          # refer to it in the linker rules.
          prx_output = output
          prx_output_base, prx_output_ext = os.path.splitext(prx_output)
          assert prx_output_ext == '.sprx'

          extra_bindings.append(('prx', output))
          # TODO: Figure out how to suppress the "removal" warning
          # generated from the prx generator when we remove a function.
          # For now, we'll just delete the 'verlog.txt' file before linking.
          # Bind it here so we can refer to it as $verlog in the PS3 solink
          # rule.
          verlog = output.replace(prx_output_ext, '_verlog.txt')
          extra_bindings.append(('verlog', verlog))
          self.target.import_lib = output.replace(prx_output_ext, '_stub.a')
          output = [prx_output, self.target.import_lib]

          # For PRXs, we need to convert any c++ exports into C. This is done
          # with an "export pickup" step that runs over the object files
          # and produces a new .c file. That .c file should be compiled and
          # linked into the PRX.
          gen_files_dir = os.path.join(
              self.ExpandSpecial(
                  generator_default_variables['SHARED_INTERMEDIATE_DIR']),
              'prx')

          export_pickup_output = os.path.join(
              gen_files_dir,
              os.path.basename(prx_output_base) + '.prx_export.c')
          prx_export_obj_file = export_pickup_output[:-2] + '.o'
          self.ninja.build(
              export_pickup_output,
              'prx_export_pickup',
              link_deps,
              implicit=list(implicit_deps),
              order_only=list(order_only_deps))

          self.ninja.build(prx_export_obj_file, 'cc', export_pickup_output)
          link_deps.append(prx_export_obj_file)

        else:
          output = [output, output + '.TOC']

      if len(solibs):
        extra_bindings.append(('solibs',
                               gyp.common.EncodePOSIXShellList(solibs)))

      if self.toolset != 'target':
        command += '_' + self.toolset

      self.ninja.build(
          output,
          command,
          link_deps,
          implicit=list(implicit_deps),
          order_only=list(order_only_deps),
          variables=extra_bindings)

  def WriteTarget(self, spec, config_name, config, link_deps, compile_deps):
    if spec['type'] == 'none':
      # TODO(evan): don't call this function for 'none' target types, as
      # it doesn't do anything, and we fake out a 'binary' with a stamp file.
      self.target.binary = compile_deps
    elif spec['type'] == 'static_library':
      self.target.binary = self.ComputeOutput(spec)
      variables = []

      try:
        if self.toolset == 'target':
          toolchain = GetTargetToolchain(self.flavor)
        else:
          toolchain = GetHostToolchain(self.flavor)

        static_linker = FindFirstInstanceOf(abstract.StaticLinker, toolchain)
        if not self.is_standalone_static_library:
          static_thin_linker = FindFirstInstanceOf(abstract.StaticThinLinker,
                                                   toolchain)
          if static_thin_linker:
            static_linker = static_thin_linker
        assert static_linker, ('Toolchain must provide static linker in order '
                               'to build {0} for {1} platform.').format(
                                   self.target.binary, self.toolset)

        rule_name = GetNinjaRuleName(static_linker, self.toolset)
      except NotImplementedError:
        # Fall back to the legacy toolchain.

        if GetToolchainOrNone(self.flavor):
          libflags = GetToolchainOrNone(
              self.flavor).GetCompilerSettings().GetLibFlags(
                  config_name, self.GypPathToNinja)
          # TODO: Starboardize libflags vs libtool_flags.
          variables.append(('libflags', ' '.join(libflags)))
        postbuild = self.GetPostbuildCommand(spec, self.target.binary,
                                             self.target.binary)
        if postbuild:
          variables.append(('postbuilds', postbuild))
        if self.xcode_settings:
          variables.append(('libtool_flags',
                            self.xcode_settings.GetLibtoolflags(config_name)))
        # TODO: Starboardize.
        if (self.flavor not in (['mac'] + microsoft_flavors) and
            not self.is_standalone_static_library):
          rule_name = 'alink_thin'
        else:
          rule_name = 'alink'
        if self.toolset != 'target':
          rule_name += '_' + self.toolset

      self.ninja.build(
          self.target.binary,
          rule_name,
          link_deps,
          order_only=compile_deps,
          variables=variables)
    else:
      self.WriteLink(spec, config_name, config, link_deps)
    return self.target.binary

  def WriteMacBundle(self, spec, mac_bundle_depends):
    assert self.is_mac_bundle
    package_framework = spec['type'] in ('shared_library', 'loadable_module')
    output = self.ComputeMacBundleOutput()
    postbuild = self.GetPostbuildCommand(
        spec,
        output,
        self.target.binary,
        is_command_start=not package_framework)
    variables = []
    if postbuild:
      variables.append(('postbuilds', postbuild))
    if package_framework:
      variables.append(('version', self.xcode_settings.GetFrameworkVersion()))
      self.ninja.build(
          output, 'package_framework', mac_bundle_depends, variables=variables)
    else:
      self.ninja.build(output, 'stamp', mac_bundle_depends, variables=variables)
    self.target.bundle = output
    return output

  def GetSortedXcodeEnv(self, additional_settings=None):
    """Returns the variables Xcode would set for build steps."""
    assert self.abs_build_dir
    abs_build_dir = self.abs_build_dir
    return gyp.xcode_emulation.GetSortedXcodeEnv(
        self.xcode_settings, abs_build_dir,
        os.path.join(abs_build_dir,
                     self.build_to_base), self.config_name, additional_settings)

  def GetSortedXcodePostbuildEnv(self):
    """Returns the variables Xcode would set for postbuild steps."""
    postbuild_settings = {}
    # CHROMIUM_STRIP_SAVE_FILE is a chromium-specific hack.
    # TODO(thakis): It would be nice to have some general mechanism instead.
    strip_save_file = self.xcode_settings.GetPerTargetSetting(
        'CHROMIUM_STRIP_SAVE_FILE')
    if strip_save_file:
      postbuild_settings['CHROMIUM_STRIP_SAVE_FILE'] = strip_save_file
    return self.GetSortedXcodeEnv(additional_settings=postbuild_settings)

  def GetPostbuildCommand(self,
                          spec,
                          output,
                          output_binary,
                          is_command_start=False):
    """Returns a shell command that runs all the postbuilds, and removes
    |output| if any of them fails. If |is_command_start| is False, then the
    returned string will start with ' && '."""
    if not self.xcode_settings or spec['type'] == 'none' or not output:
      return ''
    output = QuoteShellArgument(output, self.flavor)
    target_postbuilds = self.xcode_settings.GetTargetPostbuilds(
        self.config_name,
        os.path.normpath(os.path.join(self.base_to_build, output)),
        QuoteShellArgument(
            os.path.normpath(os.path.join(self.base_to_build, output_binary)),
            self.flavor),
        quiet=True)
    postbuilds = gyp.xcode_emulation.GetSpecPostbuildCommands(spec, quiet=True)
    postbuilds = target_postbuilds + postbuilds
    if not postbuilds:
      return ''
    # Postbuilds expect to be run in the gyp file's directory, so insert an
    # implicit postbuild to cd to there.
    postbuilds.insert(0,
                      gyp.common.EncodePOSIXShellList(
                          ['cd', self.build_to_base]))
    env = self.ComputeExportEnvString(self.GetSortedXcodePostbuildEnv())
    # G will be non-null if any postbuild fails. Run all postbuilds in a
    # subshell.
    commands = env + ' (F=0; ' + \
        ' '.join([ninja_syntax.escape(command) + ' || F=$$?;'
                                 for command in postbuilds])
    command_string = (
        commands + ' exit $$F); G=$$?; '
        # Remove the final output if any postbuild failed.
        '((exit $$G) || rm -rf %s) ' % output + '&& exit $$G)')
    if is_command_start:
      return '(' + command_string + ' && '
    else:
      return '$ && (' + command_string

  def ComputeExportEnvString(self, env):
    """Given an environment, returns a string looking like
        'export FOO=foo; export BAR="${FOO} bar;'
    that exports |env| to the shell."""
    export_str = []
    for k, v in env:
      export_str.append(
          'export %s=%s;' %
          (k, ninja_syntax.escape(gyp.common.EncodePOSIXShellArgument(v))))
    return ' '.join(export_str)

  def ComputeMacBundleOutput(self):
    """Return the 'output' (full output path) to a bundle output directory."""
    assert self.is_mac_bundle
    path = self.ExpandSpecial(generator_default_variables['PRODUCT_DIR'])
    return os.path.join(path, self.xcode_settings.GetWrapperName())

  def ComputeMacBundleBinaryOutput(self):
    """Return the 'output' (full output path) to the binary in a bundle."""
    assert self.is_mac_bundle
    path = self.ExpandSpecial(generator_default_variables['PRODUCT_DIR'])
    return os.path.join(path, self.xcode_settings.GetExecutablePath())

  def ComputeOutputFileName(self, spec, type=None):
    """Compute the filename of the final output for the current target."""
    if not type:
      type = spec['type']

    default_variables = copy.copy(generator_default_variables)
    if GetToolchainOrNone(self.flavor):
      GetToolchainOrNone(
          self.flavor).SetAdditionalGypVariables(default_variables)
    else:
      CalculateVariables(default_variables, {'flavor': self.flavor})

    # Compute filename prefix: the product prefix, or a default for
    # the product type.
    DEFAULT_PREFIX = {
        'loadable_module': default_variables['SHARED_LIB_PREFIX'],
        'shared_library': default_variables['SHARED_LIB_PREFIX'],
        'static_library': default_variables['STATIC_LIB_PREFIX'],
        'executable': default_variables['EXECUTABLE_PREFIX'],
    }
    prefix = spec.get('product_prefix', DEFAULT_PREFIX.get(type, ''))

    # Compute filename extension: the product extension, or a default
    # for the product type.
    DEFAULT_EXTENSION = {
        'loadable_module': default_variables['SHARED_LIB_SUFFIX'],
        'shared_library': default_variables['SHARED_LIB_SUFFIX'],
        'static_library': default_variables['STATIC_LIB_SUFFIX'],
        'executable': default_variables['EXECUTABLE_SUFFIX'],
    }
    extension = spec.get('product_extension')
    if extension:
      extension = '.' + extension
    elif self.toolset == 'host':
      # TODO: Based on a type of target, ask a corresponding
      #       tool from a toolchain to compute the file name.
      if is_windows:
        extension = '.exe'
      else:
        extension = ''
    else:
      extension = DEFAULT_EXTENSION.get(type, '')

    if 'product_name' in spec:
      # If we were given an explicit name, use that.
      target = spec['product_name']
    else:
      # Otherwise, derive a name from the target name.
      target = spec['target_name']
      if prefix == 'lib':
        # Snip out an extra 'lib' from libs if appropriate.
        target = StripPrefix(target, 'lib')

    if type in ('static_library', 'loadable_module', 'shared_library',
                'executable'):
      return '%s%s%s' % (prefix, target, extension)
    elif type == 'none':
      return '%s.stamp' % target
    else:
      raise Exception('Unhandled output type %s' % type)

  def ComputeOutput(self, spec, type=None):
    """Compute the path for the final output of the spec."""
    assert not self.is_mac_bundle or type

    if not type:
      type = spec['type']

    if self.flavor == 'win':
      override = GetToolchainOrNone(
          self.flavor).GetCompilerSettings().GetOutputName(
              self.config_name, self.ExpandSpecial)
      if override:
        return override

    if self.flavor == 'mac' and type in ('static_library', 'executable',
                                         'shared_library', 'loadable_module'):
      filename = self.xcode_settings.GetExecutablePath()
    else:
      filename = self.ComputeOutputFileName(spec, type)

    if 'product_dir' in spec:
      path = os.path.join(spec['product_dir'], filename)
      return self.ExpandSpecial(path)

    # Some products go into the output root, libraries go into shared library
    # dir, and everything else goes into the normal place.
    type_in_output_root = ['executable', 'loadable_module']
    if self.flavor == 'mac' and self.toolset == 'target':
      type_in_output_root += ['shared_library', 'static_library']
    elif self.flavor in ['win', 'ps3'] and self.toolset == 'target':
      type_in_output_root += ['shared_library']

    if type in type_in_output_root or self.is_standalone_static_library:
      return filename
    elif type == 'shared_library':
      libdir = 'lib'
      if self.toolset != 'target':
        libdir = os.path.join('lib', '%s' % self.toolset)
      return os.path.join(libdir, filename)
    else:
      return self.GypPathToUniqueOutput(filename, qualified=False)

  def WriteVariableList(self, var, values):
    assert not isinstance(values, str)
    if values is None:
      values = []
    self.ninja.variable(var, ' '.join(values))

  def WriteNewNinjaRule(self, name, args, description, is_cygwin, env):
    """Write out a new ninja "rule" statement for a given command.

    Returns the name of the new rule, and a copy of |args| with variables
    expanded."""

    if self.flavor == 'win':
      args = [
          GetToolchainOrNone(self.flavor).GetCompilerSettings().ConvertVSMacros(
              arg, self.base_to_build, config=self.config_name) for arg in args
      ]
      description = GetToolchainOrNone(
          self.flavor).GetCompilerSettings().ConvertVSMacros(
              description, config=self.config_name)
    elif self.flavor == 'mac':
      # |env| is an empty list on non-mac.
      args = [gyp.xcode_emulation.ExpandEnvVars(arg, env) for arg in args]
      description = gyp.xcode_emulation.ExpandEnvVars(description, env)

    # TODO: we shouldn't need to qualify names; we do it because
    # currently the ninja rule namespace is global, but it really
    # should be scoped to the subninja.
    rule_name = self.name
    if self.toolset == 'target':
      rule_name += '.' + self.toolset
    rule_name += '.' + name
    rule_name = re.sub('[^a-zA-Z0-9_]', '_', rule_name)

    # Remove variable references, but not if they refer to the magic rule
    # variables.  This is not quite right, as it also protects these for
    # actions, not just for rules where they are valid. Good enough.
    protect = ['${root}', '${dirname}', '${source}', '${ext}', '${name}']
    protect = '(?!' + '|'.join(map(re.escape, protect)) + ')'
    description = re.sub(protect + r'\$', '_', description)

    # gyp dictates that commands are run from the base directory.
    # cd into the directory before running, and adjust paths in
    # the arguments to point to the proper locations.
    rspfile = None
    rspfile_content = None
    args = [self.ExpandSpecial(arg, self.base_to_build) for arg in args]
    if (self.flavor in windows_host_flavors and is_windows):
      rspfile = rule_name + '.$unique_name.rsp'
      # The cygwin case handles this inside the bash sub-shell.
      run_in = '' if is_cygwin else ' ' + self.build_to_base
      if is_cygwin:
        rspfile_content = self.msvs_settings.BuildCygwinBashCommandLine(
            args, self.build_to_base)
      elif self.flavor in sony_flavors:
        rspfile_content = gyp.msvs_emulation.EncodeRspFileList(args)
      else:
        rspfile_content = GetToolchainOrNone(
            self.flavor).EncodeRspFileList(args)

      command = ('%s gyp-win-tool action-wrapper $arch ' % sys.executable +
                 rspfile + run_in)
    else:
      env = self.ComputeExportEnvString(env)
      command = gyp.common.EncodePOSIXShellList(args)
      command = 'cd %s; ' % self.build_to_base + env + command

    # GYP rules/actions express being no-ops by not touching their outputs.
    # Avoid executing downstream dependencies in this case by specifying
    # restat=1 to ninja.
    self.ninja.rule(
        rule_name,
        command,
        description,
        restat=True,
        rspfile=rspfile,
        rspfile_content=rspfile_content)
    self.ninja.newline()

    return rule_name, args


def CalculateVariables(default_variables, params):
  """Calculate additional variables for use in the build (called by gyp)."""
  global generator_additional_non_configuration_keys
  global generator_additional_path_sections
  flavor = gyp.common.GetFlavor(params)
  if flavor == 'mac':
    default_variables.setdefault('OS', 'mac')
    default_variables.setdefault('SHARED_LIB_SUFFIX', '.dylib')
    default_variables.setdefault('SHARED_LIB_DIR',
                                 generator_default_variables['PRODUCT_DIR'])
    default_variables.setdefault('LIB_DIR',
                                 generator_default_variables['PRODUCT_DIR'])

    # Copy additional generator configuration data from Xcode, which is shared
    # by the Mac Ninja generator.
    import gyp.generator.xcode as xcode_generator
    generator_additional_non_configuration_keys = getattr(
        xcode_generator, 'generator_additional_non_configuration_keys', [])
    generator_additional_path_sections = getattr(
        xcode_generator, 'generator_additional_path_sections', [])
    global generator_extra_sources_for_rules
    generator_extra_sources_for_rules = getattr(
        xcode_generator, 'generator_extra_sources_for_rules', [])
  elif flavor in ['ps3']:
    if is_windows:
      # This is required for BuildCygwinBashCommandLine() to work.
      import gyp.generator.msvs as msvs_generator
      generator_additional_non_configuration_keys = getattr(
          msvs_generator, 'generator_additional_non_configuration_keys', [])
      generator_additional_path_sections = getattr(
          msvs_generator, 'generator_additional_path_sections', [])

    default_variables['SHARED_LIB_PREFIX'] = ''
    default_variables['SHARED_LIB_SUFFIX'] = '.sprx'
    generator_flags = params.get('generator_flags', {})

  elif flavor in ['ps4', 'ps4-vr']:
    if is_windows:
      # This is required for BuildCygwinBashCommandLine() to work.
      import gyp.generator.msvs as msvs_generator
      generator_additional_non_configuration_keys = getattr(
          msvs_generator, 'generator_additional_non_configuration_keys', [])
      generator_additional_path_sections = getattr(
          msvs_generator, 'generator_additional_path_sections', [])

    default_variables['EXECUTABLE_SUFFIX'] = '.elf'
    default_variables['SHARED_LIB_PREFIX'] = 'lib'
    default_variables['SHARED_LIB_SUFFIX'] = '.so'

    # Copy additional generator configuration data from VS, which is shared
    # by the Windows Ninja generator.
    import gyp.generator.msvs as msvs_generator
    generator_additional_non_configuration_keys = getattr(
        msvs_generator, 'generator_additional_non_configuration_keys', [])
    generator_additional_path_sections = getattr(
        msvs_generator, 'generator_additional_path_sections', [])
  else:
    operating_system = flavor
    if flavor == 'android':
      operating_system = 'linux'  # Keep this legacy behavior for now.
    default_variables.setdefault('OS', operating_system)
    default_variables.setdefault('SHARED_LIB_SUFFIX', '.so')
    default_variables.setdefault('SHARED_LIB_DIR',
                                 os.path.join('$!PRODUCT_DIR', 'lib'))
    default_variables.setdefault('LIB_DIR', os.path.join(
        '$!PRODUCT_DIR', 'obj'))


def ComputeOutputDir(params):
  """Returns the path from the toplevel_dir to the build output directory."""
  # generator_dir: relative path from pwd to where make puts build files.
  # Makes migrating from make to ninja easier, ninja doesn't put anything here.
  generator_dir = os.path.relpath(params['options'].generator_output or '.')

  # output_dir: relative path from generator_dir to the build directory.
  output_dir = params.get('generator_flags', {}).get('output_dir', 'out')

  # Relative path from source root to our output files.  e.g. "out"
  return os.path.normpath(os.path.join(generator_dir, output_dir))


def CalculateGeneratorInputInfo(params):
  """Called by __init__ to initialize generator values based on params."""
  user_config = params.get('generator_flags', {}).get('config', None)
  toplevel = params['options'].toplevel_dir
  qualified_out_dir = os.path.normpath(os.path.join(
      toplevel, ComputeOutputDir(params), user_config, 'gypfiles'))

  global generator_filelist_paths
  generator_filelist_paths = {
      'toplevel': toplevel,
      'qualified_out_dir': qualified_out_dir,
  }


def OpenOutput(path, mode='w'):
  """Open |path| for writing, creating directories if necessary."""
  try:
    os.makedirs(os.path.dirname(path))
  except OSError:
    pass
  return open(path, mode)


def GetDefaultConcurrentLinks():
  """Returns a best-guess for a number of concurrent links."""
  pool_size = int(os.getenv('GYP_LINK_CONCURRENCY', 0))
  if pool_size:
    return pool_size

  if sys.platform in ('win32', 'cygwin'):
    import ctypes

    class MEMORYSTATUSEX(ctypes.Structure):
      _fields_ = [
          ('dwLength', ctypes.c_ulong),
          ('dwMemoryLoad', ctypes.c_ulong),
          ('ullTotalPhys', ctypes.c_ulonglong),
          ('ullAvailPhys', ctypes.c_ulonglong),
          ('ullTotalPageFile', ctypes.c_ulonglong),
          ('ullAvailPageFile', ctypes.c_ulonglong),
          ('ullTotalVirtual', ctypes.c_ulonglong),
          ('ullAvailVirtual', ctypes.c_ulonglong),
          ('sullAvailExtendedVirtual', ctypes.c_ulonglong),
      ]

    stat = MEMORYSTATUSEX()
    stat.dwLength = ctypes.sizeof(stat)
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(stat))

    # VS 2015 uses 20% more working set than VS 2013 and can consume all RAM
    # on a 64 GB machine.
    mem_limit = max(1, stat.ullTotalPhys / (5 * (2**30)))  # total / 5GB
    hard_cap = max(1, int(os.getenv('GYP_LINK_CONCURRENCY_MAX', 2**32)))
    return min(mem_limit, hard_cap)
  elif sys.platform.startswith('linux'):
    if os.path.exists('/proc/meminfo'):
      with open('/proc/meminfo') as meminfo:
        memtotal_re = re.compile(r'^MemTotal:\s*(\d*)\s*kB')
        for line in meminfo:
          match = memtotal_re.match(line)
          if not match:
            continue
          # Allow 6Gb per link on Linux because Gold is quite memory hungry
          return max(1, int(match.group(1)) / (6 * (2**20)))
    return 1
  elif sys.platform == 'darwin':
    try:
      avail_bytes = int(subprocess.check_output(['sysctl', '-n', 'hw.memsize']))
      # A static library debug build of Chromium's unit_tests takes ~2.7GB, so
      # 4GB per ld process allows for some more bloat.
      return max(1, avail_bytes / (4 * (2**30)))  # total / 4GB
    except:
      return 1
  else:
    # TODO(scottmg): Implement this for other platforms.
    return 1


def MaybeWritePathVariable(ninja, tool, toolset):
  if tool.GetPath():
    ninja.variable('{0}_path'.format(GetNinjaRuleName(tool, toolset)),
                   tool.GetPath())


def MaybeWriteExtraFlagsVariable(ninja, tool, toolset, shell):
  if tool.GetExtraFlags():
    ninja.variable(
        '{0}_extra_flags'.format(GetNinjaRuleName(tool, toolset)),
        shell.Join(tool.GetExtraFlags()))


def MaybeWritePool(ninja, tool, toolset):
  if tool.GetMaxConcurrentProcesses():
    ninja.pool(
        '{0}_pool'.format(GetNinjaRuleName(tool, toolset)),
        depth=tool.GetMaxConcurrentProcesses())


def MaybeWriteRule(ninja, tool, toolset, shell):
  if tool.GetRuleName():
    name = GetNinjaRuleName(tool, toolset)

    path = '${0}_path'.format(name)
    extra_flags = '${0}_extra_flags'.format(name)
    flags = '${0}_flags'.format(name)
    pool = '{0}_pool'.format(name) if tool.GetMaxConcurrentProcesses() else None

    ninja.rule(
        name,
        tool.GetCommand(path, extra_flags, flags, shell),
        description=tool.GetDescription(),
        depfile=tool.GetHeaderDependenciesFilePath(),
        deps=tool.GetHeaderDependenciesFormat(),
        pool=pool,
        rspfile=tool.GetRspFilePath(),
        rspfile_content=tool.GetRspFileContent())


def GenerateOutputForConfig(target_list, target_dicts, data, params,
                            config_name):
  options = params['options']
  flavor = gyp.common.GetFlavor(params)

  generator_flags = params.get('generator_flags', {})

  # build_dir: relative path from source root to our output files.
  # e.g. "out/Debug"
  build_dir = os.path.normpath(
      os.path.join(ComputeOutputDir(params), config_name))

  toplevel_build = os.path.join(options.toplevel_dir, build_dir)

  master_ninja = ninja_syntax.Writer(
      OpenOutput(os.path.join(toplevel_build, 'build.ninja')), width=120)
  case_sensitive_filesystem = True

  build_file, _, _ = gyp.common.ParseQualifiedTarget(target_list[0])
  make_global_settings = data[build_file].get('make_global_settings', [])

  try:
    # To avoid duplication, platform-agnostic tools (such as stamp and copy)
    # will be processed only in the host toolchain.
    target_toolchain = [
        target_tool for target_tool in GetTargetToolchain(flavor)
        if not target_tool.IsPlatformAgnostic()
    ]
    host_toolchain = GetHostToolchain(flavor)

    shell = GetShell(flavor)

    for target_tool in target_toolchain:
      MaybeWritePathVariable(master_ninja, target_tool, 'target')
    for host_tool in host_toolchain:
      MaybeWritePathVariable(master_ninja, host_tool, 'host')
    master_ninja.newline()

    for target_tool in target_toolchain:
      MaybeWriteExtraFlagsVariable(master_ninja, target_tool, 'target', shell)
    for host_tool in host_toolchain:
      MaybeWriteExtraFlagsVariable(master_ninja, host_tool, 'host', shell)
    master_ninja.newline()

    for target_tool in target_toolchain:
      MaybeWritePool(master_ninja, target_tool, 'target')
    for host_tool in host_toolchain:
      MaybeWritePool(master_ninja, host_tool, 'host')
    master_ninja.newline()

    for target_tool in target_toolchain:
      MaybeWriteRule(master_ninja, target_tool, 'target', shell)
    for host_tool in host_toolchain:
      MaybeWriteRule(master_ninja, host_tool, 'host', shell)
    master_ninja.newline()
  except NotImplementedError:
    # Fall back to the legacy toolchain.

    # Put build-time support tools in out/{config_name}.
    gyp.common.CopyTool(flavor, toplevel_build)

    # Grab make settings for CC/CXX.
    # The rules are
    # - The priority from low to high is gcc/g++, the 'make_global_settings' in
    #   gyp, the environment variable.
    # - If there is no 'make_global_settings' for CC.host/CXX.host or
    #   'CC_host'/'CXX_host' enviroment variable, cc_host/cxx_host should be set
    #   to cc/cxx.
    if (flavor in sony_flavors and is_windows):
      cc = 'cl.exe'
      cxx = 'cl.exe'
      ld = 'link.exe'
      gyp.msvs_emulation.GenerateEnvironmentFiles(toplevel_build,
                                                  generator_flags, OpenOutput)
      ld_host = '$ld'
    elif GetToolchainOrNone(flavor):
      # TODO: starboardize.
      cc = 'cl.exe'
      cxx = 'cl.exe'
      ld = 'link.exe'
      GetToolchainOrNone(flavor).GenerateEnvironmentFiles(
          toplevel_build, generator_flags, OpenOutput)
      ld_host = '$ld'
    else:
      cc = 'gcc'
      cxx = 'g++'
      ld = '$cxx'
      ld_host = '$cxx_host'

    cc_host = None
    cxx_host = None
    cc_host_global_setting = None
    cxx_host_global_setting = None

    build_to_root = InvertRelativePath(build_dir)
    for key, value in make_global_settings:
      if key == 'CC':
        cc = os.path.join(build_to_root, value)
      if key == 'CXX':
        cxx = os.path.join(build_to_root, value)
      if key == 'LD':
        ld = os.path.join(build_to_root, value)
      if key == 'CC.host':
        cc_host = os.path.join(build_to_root, value)
        cc_host_global_setting = value
      if key == 'CXX.host':
        cxx_host = os.path.join(build_to_root, value)
        cxx_host_global_setting = value
      if key == 'LD.host':
        ld_host = os.path.join(build_to_root, value)

    flock = 'flock'
    if flavor == 'mac':
      flock = './gyp-mac-tool flock'
    cc = GetEnvironFallback(['CC_target', 'CC'], cc)
    master_ninja.variable('cc', cc)
    cxx = GetEnvironFallback(['CXX_target', 'CXX'], cxx)
    master_ninja.variable('cxx', cxx)
    ld = GetEnvironFallback(['LD_target', 'LD'], ld)
    rc = GetEnvironFallback(['RC'], 'rc.exe')

    if not cc_host:
      cc_host = cc
    if not cxx_host:
      cxx_host = cxx

    # gyp-win-tool wrappers have a winpython only flock implementation.
    if sys.platform == 'cygwin':
      python_exec = '$python'
    else:
      python_exec = sys.executable

    ar_flags = ''
    if flavor in microsoft_flavors:
      master_ninja.variable('ld', ld)
      master_ninja.variable('ar', os.environ.get('AR', 'ar'))
      master_ninja.variable('rc', rc)
      master_ninja.variable('asm', 'ml.exe')
      master_ninja.variable('mt', 'mt.exe')
      master_ninja.variable('use_dep_database', '1')
    elif flavor in sony_flavors:
      # Require LD to be set.
      master_ninja.variable('ld', os.environ.get('LD'))
      master_ninja.variable('ar', os.environ.get('AR', 'ar'))
      if flavor in ['ps3']:
        master_ninja.variable('prx_export_pickup',
                              os.environ['PRX_EXPORT_PICKUP'])
      ar_flags = os.environ.get('ARFLAGS', 'rcs')
      master_ninja.variable('arFlags', ar_flags)
      # On the PS3, when we use ps3snarl.exe with a response file, we cannot
      # pass it flags (like 'rcs'), so ARFLAGS is likely set to '' for this
      # platform.  In that case, do not append the thin archive 'T' flag
      # to the flags string.
      # Likewise for PS4, but using orbis-snarl.exe
      thin_flag_to_add = ''
      if len(ar_flags) >= 1 and ar_flags.find('T') == -1:
        thin_flag_to_add = 'T'
      master_ninja.variable('arThinFlags', ar_flags + thin_flag_to_add)

    else:
      master_ninja.variable('ld', ld)
      master_ninja.variable('ar', GetEnvironFallback(['AR_target', 'AR'], 'ar'))
      ar_flags = os.environ.get('ARFLAGS', 'rcs')
      master_ninja.variable('arFlags', ar_flags)
      thin_flag_to_add = ''
      if ar_flags.find('T') == -1:
        thin_flag_to_add = 'T'
      master_ninja.variable('arThinFlags', ar_flags + thin_flag_to_add)
    master_ninja.variable('ar_host', GetEnvironFallback(['AR_host'], 'ar'))
    cc_host = GetEnvironFallback(['CC_host'], cc_host)
    cxx_host = GetEnvironFallback(['CXX_host'], cxx_host)
    ld_host = GetEnvironFallback(['LD_host'], ld_host)
    arflags_host = GetEnvironFallback(['ARFLAGS_host'], ar_flags)
    arthinflags_host = GetEnvironFallback(['ARTHINFLAGS_host'], arflags_host)

    # The environment variable could be used in 'make_global_settings', like
    # ['CC.host', '$(CC)'] or ['CXX.host', '$(CXX)'], transform them here.
    if '$(CC)' in cc_host and cc_host_global_setting:
      cc_host = cc_host_global_setting.replace('$(CC)', cc)
    if '$(CXX)' in cxx_host and cxx_host_global_setting:
      cxx_host = cxx_host_global_setting.replace('$(CXX)', cxx)
    master_ninja.variable('cc_host', cc_host)
    master_ninja.variable('cxx_host', cxx_host)
    master_ninja.variable('arFlags_host', arflags_host)
    master_ninja.variable('arThinFlags_host', arthinflags_host)
    master_ninja.variable('ld_host', ld_host)

    if sys.platform == 'cygwin':
      python_path = cygpath.to_nt(
          '/cygdrive/c/python_27_amd64/files/python.exe')
    else:
      python_path = 'python'
    master_ninja.variable('python', python_path)
    master_ninja.newline()

    master_ninja.pool('link_pool', depth=GetDefaultConcurrentLinks())
    master_ninja.newline()

    if flavor not in microsoft_flavors:
      if flavor in sony_flavors:
        # uca := Unnamed Console A
        dep_format = 'snc' if (flavor in ['ps3']) else 'uca'
        master_ninja.rule(
            'cc',
            description='CC $out',
            command=('$cc @$out.rsp'),
            rspfile='$out.rsp',
            rspfile_content=('-c $in -o $out '
                             '-MMD $defines $includes $cflags $cflags_c '
                             '$cflags_pch_c'),
            depfile='$out_no_ext.d',
            deps='gcc',
            depformat=dep_format)
        master_ninja.rule(
            'cxx',
            description='CXX $out',
            command=('$cxx @$out.rsp'),
            rspfile='$out.rsp',
            rspfile_content=('-c $in -o $out '
                             '-MMD $defines $includes $cflags $cflags_cc '
                             '$cflags_pch_cc'),
            depfile='$out_no_ext.d',
            deps='gcc',
            depformat=dep_format)
      else:
        master_ninja.rule(
            'cc',
            description='CC $out',
            command=('$cc -MMD -MF $out.d $defines $includes $cflags $cflags_c '
                     '$cflags_pch_c -c $in -o $out'),
            deps='gcc',
            depfile='$out.d')
        master_ninja.rule(
            'cc_s',
            description='CC $out',
            command=('$cc $defines $includes $cflags $cflags_c '
                     '$cflags_pch_c -c $in -o $out'))
        master_ninja.rule(
            'cxx',
            description='CXX $out',
            command=(
                '$cxx -MMD -MF $out.d $defines $includes $cflags $cflags_cc '
                '$cflags_pch_cc -c $in -o $out'),
            deps='gcc',
            depfile='$out.d')

    else:
      cc_command = ('$cc /nologo /showIncludes /FC '
                    '@$out.rsp /c $in /Fo$out /Fd$pdbname ')
      cxx_command = ('$cxx /nologo /showIncludes /FC '
                     '@$out.rsp /c $in /Fo$out /Fd$pdbname ')
      master_ninja.rule(
          'cc',
          description='CC $out',
          command=cc_command,
          deps='msvc',
          rspfile='$out.rsp',
          rspfile_content='$defines $includes $cflags $cflags_c')
      master_ninja.rule(
          'cxx',
          description='CXX $out',
          command=cxx_command,
          deps='msvc',
          rspfile='$out.rsp',
          rspfile_content='$defines $includes $cflags $cflags_cc')

      master_ninja.rule(
          'rc',
          description='RC $in',
          # Note: $in must be last otherwise rc.exe complains.
          command=('%s gyp-win-tool rc-wrapper '
                   '$arch $rc $defines $includes $rcflags /fo$out $in' %
                   python_exec))
      master_ninja.rule(
          'asm',
          description='ASM $in',
          command=(
              '%s gyp-win-tool asm-wrapper '
              '$arch $asm $defines $includes /c /Fo $out $in' % python_exec))

    if flavor not in (['mac'] + microsoft_flavors):
      alink_command = 'rm -f $out && $ar $arFlags $out @$out.rsp'
      # TODO: Use rcsT on Linux only.
      alink_thin_command = 'rm -f $out && $ar $arThinFlags $out @$out.rsp'

      ld_cmd = '$ld'

      if flavor in sony_flavors and is_windows:
        alink_command = 'cmd.exe /c ' + alink_command
        alink_thin_command = 'cmd.exe /c ' + alink_thin_command
        ld_cmd = '%s gyp-win-tool link-wrapper $arch $ld' % python_exec

      master_ninja.rule(
          'alink',
          description='AR $out',
          command=alink_command,
          rspfile='$out.rsp',
          rspfile_content='$in_newline')
      master_ninja.rule(
          'alink_thin',
          description='AR $out',
          command=alink_thin_command,
          rspfile='$out.rsp',
          rspfile_content='$in_newline')

      if flavor in ['ps3']:
        # TODO: Can we suppress the warnings from verlog.txt rather than
        # rm'ing it?
        ld_cmd = 'rm -f $verlog && ' + ld_cmd
        if is_windows:
          ld_cmd = 'cmd.exe /c ' + ld_cmd

        prx_flags = '--oformat=fsprx --prx-with-runtime --zgenprx -zgenstub'
        master_ninja.rule(
            'solink',
            description='LINK(PRX) $lib',
            restat=True,
            command=ld_cmd + ' @$prx.rsp',
            rspfile='$prx.rsp',
            rspfile_content='$ldflags %s -o $prx $in $libs' % prx_flags,
            pool='link_pool')
        master_ninja.rule(
            'prx_export_pickup',
            description='PRX-EXPORT-PICKUP $out',
            command='$prx_export_pickup --output-src=$out $in')

      else:  # Assume it is a Linux platform
        # This allows targets that only need to depend on $lib's API to declare
        # an order-only dependency on $lib.TOC and avoid relinking such
        # downstream dependencies when $lib changes only in non-public ways.
        # The resulting string leaves an uninterpolated %{suffix} which
        # is used in the final substitution below.
        mtime_preserving_solink_base = (
            'if [ ! -e $lib -o ! -e ${lib}.TOC ]; then %(solink)s && '
            '%(extract_toc)s > ${lib}.TOC; else %(solink)s && %(extract_toc)s '
            '> ${lib}.tmp && if ! cmp -s ${lib}.tmp ${lib}.TOC; then mv '
            '${lib}.tmp ${lib}.TOC ; fi; fi' % {
                'solink': (
                    ld_cmd +
                    ' -shared $ldflags -o $lib -Wl,-soname=$soname %(suffix)s'),
                'extract_toc': ('{ readelf -d ${lib} | grep SONAME ; '
                                'nm -gD -f p ${lib} | cut -f1-2 -d\' \'; }')
            })

        master_ninja.rule(
            'solink',
            description='SOLINK $lib',
            restat=True,
            command=(mtime_preserving_solink_base % {
                'suffix':
                    '-Wl,--whole-archive $in $solibs -Wl,--no-whole-archive '
                    '$libs'
            }))
        master_ninja.rule(
            'solink_module',
            description='SOLINK(module) $lib',
            restat=True,
            command=(mtime_preserving_solink_base % {
                'suffix': '-Wl,--start-group $in $solibs -Wl,--end-group $libs'
            }))

      if flavor in sony_flavors:
        # PS3 and PS4 linkers don't know about rpath.
        rpath = ''
      else:
        rpath = r'-Wl,-rpath=\$$ORIGIN/lib'

      master_ninja.rule(
          'link',
          description='LINK $out',
          command=(ld_cmd + ' @$out.rsp'),
          rspfile='$out.rsp',
          rspfile_content=('$ldflags -o $out %s -Wl,--start-group $in $solibs '
                           '-Wl,--end-group $libs' % rpath),
          pool='link_pool')
    elif flavor in microsoft_flavors:
      master_ninja.rule(
          'alink',
          description='LIB $out',
          command=(
              '%s gyp-win-tool link-wrapper $arch '
              '$ar /nologo /ignore:4221 /OUT:$out @$out.rsp' % python_exec),
          rspfile='$out.rsp',
          rspfile_content='$in_newline $libflags')
      dlldesc = 'LINK(DLL) $dll'
      dllcmd = ('%s gyp-win-tool link-wrapper $arch '
                '$ld /nologo $implibflag /DLL /OUT:$dll '
                '/PDB:$dll.pdb @$dll.rsp' % python_exec)
      if not flavor in microsoft_flavors:
        # XB1 doesn't need a manifest.
        dllcmd += (
            ' && %s gyp-win-tool manifest-wrapper $arch '
            '$mt -nologo -manifest $manifests -out:$dll.manifest' % python_exec)
      master_ninja.rule(
          'solink',
          description=dlldesc,
          command=dllcmd,
          rspfile='$dll.rsp',
          rspfile_content='$libs $in_newline $ldflags',
          restat=True)
      master_ninja.rule(
          'solink_module',
          description=dlldesc,
          command=dllcmd,
          rspfile='$dll.rsp',
          rspfile_content='$libs $in_newline $ldflags',
          restat=True)
      # Note that ldflags goes at the end so that it has the option of
      # overriding default settings earlier in the command line.
      if flavor == 'win':
        link_command = ('%s gyp-win-tool link-wrapper $arch '
                        '$ld /nologo /OUT:$out /PDB:$out.pdb @$out.rsp && '
                        '%s gyp-win-tool manifest-wrapper $arch '
                        '$mt -nologo -manifest $manifests -out:$out.manifest' %
                        (python_exec, python_exec))
      else:
        assert flavor in microsoft_flavors
        # XB1 doesn't need a manifest.
        link_command = ('%s gyp-win-tool link-wrapper $arch '
                        '$ld /nologo /OUT:$out /PDB:$out.pdb @$out.rsp' %
                        (python_exec))

      master_ninja.rule(
          'link',
          description='LINK $out',
          command=link_command,
          rspfile='$out.rsp',
          rspfile_content='$in_newline $libs $ldflags',
          pool='link_pool')
    else:
      master_ninja.rule(
          'objc',
          description='OBJC $out',
          command=(
              '$cc -MMD -MF $out.d $defines $includes $cflags $cflags_objc '
              '$cflags_pch_objc -c $in -o $out'),
          depfile='$out.d')
      master_ninja.rule(
          'objcxx',
          description='OBJCXX $out',
          command=(
              '$cxx -MMD -MF $out.d $defines $includes $cflags $cflags_objcc '
              '$cflags_pch_objcc -c $in -o $out'),
          depfile='$out.d')
      master_ninja.rule(
          'alink',
          description='LIBTOOL-STATIC $out, POSTBUILDS',
          command='rm -f $out && '
          './gyp-mac-tool filter-libtool libtool $libtool_flags '
          '-static -o $out $in'
          '$postbuilds')

      # Record the public interface of $lib in $lib.TOC. See the corresponding
      # comment in the posix section above for details.
      mtime_preserving_solink_base = (
          'if [ ! -e $lib -o ! -e ${lib}.TOC ] || '
          # Always force dependent targets to relink if this library
          # reexports something. Handling this correctly would require
          # recursive TOC dumping but this is rare in practice, so punt.
          'otool -l $lib | grep -q LC_REEXPORT_DYLIB ; then '
          '%(solink)s && %(extract_toc)s > ${lib}.TOC; '
          'else '
          '%(solink)s && %(extract_toc)s > ${lib}.tmp && '
          'if ! cmp -s ${lib}.tmp ${lib}.TOC; then '
          'mv ${lib}.tmp ${lib}.TOC ; '
          'fi; '
          'fi' % {
              'solink': '$ld -shared $ldflags -o $lib %(suffix)s',
              'extract_toc':
                  '{ otool -l $lib | grep LC_ID_DYLIB -A 5; '
                  'nm -gP $lib | cut -f1-2 -d\' \' | grep -v U$$; true; }'
          })

      # TODO(thakis): The solink_module rule is likely wrong. Xcode seems to
      # pass -bundle -single_module here (for osmesa.so).
      master_ninja.rule(
          'solink',
          description='SOLINK $lib, POSTBUILDS',
          restat=True,
          command=(mtime_preserving_solink_base % {
              'suffix': '$in $solibs $libs$postbuilds'
          }))
      master_ninja.rule(
          'solink_module',
          description='SOLINK(module) $lib, POSTBUILDS',
          restat=True,
          command=(mtime_preserving_solink_base % {
              'suffix': '$in $solibs $libs$postbuilds'
          }))

      master_ninja.rule(
          'link',
          description='LINK $out, POSTBUILDS',
          command=('$ld $ldflags -o $out '
                   '$in $solibs $libs$postbuilds'),
          pool='link_pool')
      master_ninja.rule(
          'infoplist',
          description='INFOPLIST $out',
          command=('$cc -E -P -Wno-trigraphs -x c $defines $in -o $out && '
                   'plutil -convert xml1 $out $out'))
      master_ninja.rule(
          'mac_tool',
          description='MACTOOL $mactool_cmd $in',
          command='$env ./gyp-mac-tool $mactool_cmd $in $out')
      master_ninja.rule(
          'package_framework',
          description='PACKAGE FRAMEWORK $out, POSTBUILDS',
          command='./gyp-mac-tool package-framework $out $version$postbuilds '
          '&& touch $out')
    if flavor in microsoft_flavors:
      master_ninja.rule(
          'stamp',
          description='STAMP $out',
          command='%s gyp-win-tool stamp $out' % python_exec)
      master_ninja.rule(
          'copy',
          description='COPY $in $out',
          command='%s gyp-win-tool recursive-mirror $in $out' % python_exec)
    elif sys.platform in ['cygwin', 'win32']:
      master_ninja.rule(
          'stamp',
          description='STAMP $out',
          command='$python gyp-win-tool stamp $out')
      master_ninja.rule(
          'copy',
          description='COPY $in $out',
          command='$python gyp-win-tool recursive-mirror $in $out')
    else:
      master_ninja.rule(
          'stamp', description='STAMP $out', command='${postbuilds}touch $out')
      master_ninja.rule(
          'copy',
          description='COPY $in $out',
          command='rm -rf $out && cp -af $in $out')
    master_ninja.newline()

    # Output host building rules
    if is_windows:
      cc_command = ('$cc /nologo /showIncludes /FC '
                    '@$out.rsp /c $in /Fo$out /Fd$pdbname ')
      cxx_command = ('$cxx /nologo /showIncludes /FC '
                     '@$out.rsp /c $in /Fo$out /Fd$pdbname ')
      master_ninja.rule(
          'cc_host',
          description='CC_HOST $out',
          command=cc_command,
          deps='msvc',
          rspfile='$out.rsp',
          rspfile_content='$defines $includes $cflags_host $cflags_c_host')
      master_ninja.rule(
          'cxx_host',
          description='CXX_HOST $out',
          command=cxx_command,
          deps='msvc',
          rspfile='$out.rsp',
          rspfile_content='$defines $includes $cflags_host $cflags_cc_host')

      master_ninja.rule(
          'alink_host',
          description='LIB_HOST $out',
          command=(
              '%s gyp-win-tool link-wrapper $arch '
              '$ar /nologo /ignore:4221 /OUT:$out @$out.rsp' % python_exec),
          rspfile='$out.rsp',
          rspfile_content='$in_newline $libflags_host')

      master_ninja.rule(
          'alink_thin_host',
          description='LIB_HOST $out',
          command=(
              '%s gyp-win-tool link-wrapper $arch '
              '$ar /nologo /ignore:4221 /OUT:$out @$out.rsp' % python_exec),
          rspfile='$out.rsp',
          rspfile_content='$in_newline $libflags_host')

      link_command = ('%s gyp-win-tool link-wrapper $arch '
                      '$ld /nologo /OUT:$out /PDB:$out.pdb @$out.rsp' %
                      (python_exec))

      master_ninja.rule(
          'link_host',
          description='LINK_HOST $out',
          command=link_command,
          rspfile='$out.rsp',
          rspfile_content='$in_newline $libs $ldflags',
          pool='link_pool')
    else:
      cc_command = 'bash -c "$cc_host @$out.rsp"'
      cxx_command = 'bash -c "$cxx_host @$out.rsp"'
      master_ninja.rule(
          'cc_host',
          description='CC_HOST $out',
          command=cc_command,
          rspfile='$out.rsp',
          rspfile_content=('-MMD -MF $out.d $defines $includes $cflags_host '
                           '$cflags_c_host $cflags_pch_c -c $in -o $out'),
          depfile='$out.d')
      master_ninja.rule(
          'cxx_host',
          description='CXX_HOST $out',
          command=cxx_command,
          rspfile='$out.rsp',
          rspfile_content=('-MMD -MF $out.d $defines $includes $cflags_host '
                           '$cflags_cc_host $cflags_pch_cc -c $in -o $out'),
          depfile='$out.d')

      alink_command = 'rm -f $out && $ar_host $arFlags_host $out @$out.rsp'
      alink_thin_command = ('rm -f $out && $ar_host $arThinFlags_host $out '
                            '@$out.rsp')

      master_ninja.rule(
          'alink_host',
          description='AR_HOST $out',
          command='bash -c "' + alink_command + '"',
          rspfile='$out.rsp',
          rspfile_content='$in_newline')
      master_ninja.rule(
          'alink_thin_host',
          description='AR_HOST $out',
          command='bash -c "' + alink_thin_command + '"',
          rspfile='$out.rsp',
          rspfile_content='$in_newline')
      beginlinkinlibs = ''
      endlinkinlibs = ''
      if is_linux:
        beginlinkinlibs = '-Wl,--start-group'
        endlinkinlibs = '-Wl,--end-group'
      rpath = '-Wl,-rpath=\$$ORIGIN/lib'
      master_ninja.rule(
          'link_host',
          description='LINK_HOST $out',
          command=('bash -c "$ld_host $ldflags_host -o $out %s '
                   '%s $in $solibs %s $libs"' % (rpath, beginlinkinlibs,
                                                 endlinkinlibs)))

  all_targets = set()
  for build_file in params['build_files']:
    for target in gyp.common.AllTargets(target_list, target_dicts,
                                        os.path.normpath(build_file)):
      all_targets.add(target)
  all_outputs = set()

  # target_outputs is a map from qualified target name to a Target object.
  target_outputs = {}
  # target_short_names is a map from target short name to a list of Target
  # objects.
  target_short_names = {}
  default_project = None
  for qualified_target in target_list:
    # qualified_target is like: third_party/icu/icu.gyp:icui18n#target
    build_file, name, toolset = \
        gyp.common.ParseQualifiedTarget(qualified_target)

    this_make_global_settings = data[build_file].get('make_global_settings', [])
    assert make_global_settings == this_make_global_settings, (
        'make_global_settings needs to be the same for all targets.')

    spec = target_dicts[qualified_target]

    if spec.get('default_project', 0):
      if default_project is None or default_project == name:
        default_project = name
      else:
        raise Exception('More than one default_project specified.'
                        'First in {0} and now in {1}'.format(
                            default_project, name))

    if flavor == 'mac':
      gyp.xcode_emulation.MergeGlobalXcodeSettingsToSpec(data[build_file], spec)

    build_file = gyp.common.RelativePath(build_file, options.toplevel_dir)

    base_path = os.path.dirname(build_file)
    obj = 'obj'
    if toolset != 'target':
      obj += '.' + toolset
    output_file = os.path.join(obj, base_path, name + '.ninja')

    abs_build_dir = os.path.abspath(toplevel_build)
    writer = NinjaWriter(
        qualified_target,
        target_outputs,
        base_path,
        build_dir,
        OpenOutput(os.path.join(toplevel_build, output_file)),
        flavor,
        case_sensitive_filesystem,
        abs_build_dir=abs_build_dir)
    master_ninja.subninja(output_file)

    target = writer.WriteSpec(spec, config_name, generator_flags)
    if target:
      if name != target.FinalOutput():
        out_name = name
        if toolset != 'target':
          out_name = out_name + '.' + toolset
        target_short_names.setdefault(out_name, []).append(target)
      target_outputs[qualified_target] = target
      if qualified_target in all_targets:
        all_outputs.add(target.FinalOutput())

  if target_short_names:
    # Write a short name to build this target.  This benefits both the
    # "build chrome" case as well as the gyp tests, which expect to be
    # able to run actions and build libraries by their short name.
    master_ninja.newline()
    master_ninja.comment('Short names for targets.')
    for short_name in target_short_names:
      master_ninja.build(short_name, 'phony', [
          x.FinalOutput() for x in target_short_names[short_name]
      ])

  if all_outputs:
    master_ninja.newline()
    master_ninja.build('all', 'phony', list(all_outputs))
    if default_project:
      master_ninja.default(default_project)
    else:
      master_ninja.default('all')


def PerformBuild(data, configurations, params):
  options = params['options']
  for config in configurations:
    builddir = os.path.join(options.toplevel_dir, 'out', config)
    arguments = ['ninja', '-C', builddir]
    print 'Building [%s]: %s' % (config, arguments)
    subprocess.check_call(arguments)


def CallGenerateOutputForConfig(arglist):
  # Ignore the interrupt signal so that the parent process catches it and
  # kills all multiprocessing children.
  signal.signal(signal.SIGINT, signal.SIG_IGN)

  (target_list, target_dicts, data, params, config_name) = arglist
  GenerateOutputForConfig(target_list, target_dicts, data, params, config_name)


def GenerateOutput(target_list, target_dicts, data, params):
  user_config = params.get('generator_flags', {}).get('config', None)
  # TODO: Replace MSVSUtil with calls to abstract toolchain.
  if gyp.common.GetFlavor(params) in microsoft_flavors:
    target_list, target_dicts = MSVSUtil.ShardTargets(target_list, target_dicts)
  if user_config:
    GenerateOutputForConfig(target_list, target_dicts, data, params,
                            user_config)
  else:
    config_names = target_dicts[target_list[0]]['configurations'].keys()
    if params['parallel']:
      try:
        pool = multiprocessing.Pool(len(config_names))
        arglists = []
        for config_name in config_names:
          arglists.append((target_list, target_dicts, data, params,
                           config_name))
          pool.map(CallGenerateOutputForConfig, arglists)
      except KeyboardInterrupt, e:
        pool.terminate()
        raise e
    else:
      for config_name in config_names:
        GenerateOutputForConfig(target_list, target_dicts, data, params,
                                config_name)
