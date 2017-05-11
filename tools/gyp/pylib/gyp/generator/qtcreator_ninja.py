# Copyright 2015 Google Inc. All Rights Reserved.
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
"""Gyp generator for QT Creator projects that invoke ninja."""

import os
import re

PROJECT_DIR_RELATIVE_TO_OUTPUT_DIR = 'qtcreator_projects'


def RebaseRelativePaths(file_set, original_base, new_base):
  ret = set()
  for item in file_set:
    # TODO: For now, ignore special vars like $PRODUCT_DIR.
    if '$' in item:
      continue
    ret.add(os.path.relpath(os.path.join(original_base, item), new_base))
  return ret


def SplitAssignmentDefine(line):
  matched = re.match(r'([^=\b]+)=(.*)', line)
  if matched:
    return matched.group(1) + ' ' + matched.group(2)
  else:
    return line


def GenProject(project_def, proj_dir):
  """Generates QT Creator project (.creator) and supporting files.

  Args:
    project_def: Dictionary of parameters defining the project that will be
                 generated.
    proj_dir: The directory the generated project file will be placed into.
  """

  if not os.path.exists(proj_dir):
    os.makedirs(proj_dir)

  configuration = project_def['configuration']
  current_config = configuration['out_dir']

  if not os.path.exists(proj_dir):
    os.makedirs(proj_dir)

  basefilename = os.path.join(
      proj_dir, current_config + '_' + project_def['session_name_prefix'])
  with open(basefilename + '.creator', 'w') as f:
    f.write('[General]')

  with open(basefilename + '.config', 'w') as f:
    for define in configuration['defines']:
      f.write('#define %s\n' % SplitAssignmentDefine(define))

  with open(basefilename + '.files', 'w') as f:
    for source in project_def['sources']:
      f.write('%s\n' % source)

  with open(basefilename + '.includes', 'w') as f:
    for include_path in configuration['include_paths']:
      f.write('%s\n' % include_path)

  # TODO: Use Jinja2 template engine to generate the project files.
  with open(basefilename + '.creator.shared', 'w') as f:
    f.write("""<!DOCTYPE QtCreatorProject>
      <qtcreator>
       <data>
        <variable>ProjectExplorer.Project.EditorSettings</variable>
        <valuemap type="QVariantMap">
         <value type="bool" key="EditorConfiguration.AutoIndent">false</value>
         <value type="bool" key="EditorConfiguration.AutoSpacesForTabs">true</value>
         <value type="bool" key="EditorConfiguration.CamelCaseNavigation">true</value>
         <valuemap type="QVariantMap" key="EditorConfiguration.CodeStyle.0">
          <value type="QString" key="language">Cpp</value>
         </valuemap>
         <value type="int" key="EditorConfiguration.CodeStyle.Count">1</value>
         <value type="QByteArray" key="EditorConfiguration.Codec">UTF-8</value>
         <value type="bool" key="EditorConfiguration.ConstrainTooltips">false</value>
         <value type="int" key="EditorConfiguration.IndentSize">2</value>
         <value type="bool" key="EditorConfiguration.KeyboardTooltips">false</value>
         <value type="bool" key="EditorConfiguration.MouseNavigation">true</value>
         <value type="int" key="EditorConfiguration.PaddingMode">1</value>
         <value type="bool" key="EditorConfiguration.ScrollWheelZooming">true</value>
         <value type="int" key="EditorConfiguration.SmartBackspaceBehavior">0</value>
         <value type="bool" key="EditorConfiguration.SpacesForTabs">true</value>
         <value type="int" key="EditorConfiguration.TabKeyBehavior">0</value>
         <value type="int" key="EditorConfiguration.TabSize">2</value>
         <value type="bool" key="EditorConfiguration.UseGlobal">true</value>
         <value type="int" key="EditorConfiguration.Utf8BomBehavior">1</value>
         <value type="bool" key="EditorConfiguration.addFinalNewLine">true</value>
         <value type="bool" key="EditorConfiguration.cleanIndentation">true</value>
         <value type="bool" key="EditorConfiguration.cleanWhitespace">true</value>
         <value type="bool" key="EditorConfiguration.inEntireDocument">false</value>
        </valuemap>
       </data>
    """)
    f.write("""
       <data>
        <variable>ProjectExplorer.Project.Target.0</variable>
        <valuemap type="QVariantMap">
         <value key="ProjectExplorer.ProjectConfiguration.DefaultDisplayName" type="QString">Default Name</value>
         <value key="ProjectExplorer.ProjectConfiguration.DisplayName" type="QString">Display Name</value>
         <value key="ProjectExplorer.ProjectConfiguration.Id" type="QString">GenericProjectManager.GenericTarget</value>
    """)

    project_number = 0
    for (project_name, project_target) in project_def['projects']:
      f.write("""
         <valuemap key="ProjectExplorer.Target.BuildConfiguration.%(project_number)d" type="QVariantMap">
          <value key="ProjectExplorer.ProjectConfiguration.Id" type="QString">GenericProjectManager.GenericBuildConfiguration</value>
          <value key="ProjectExplorer.ProjectConfiguration.DisplayName" type="QString">%(project_name)s:%(project_target)s</value>
          <value key="GenericProjectManager.GenericBuildConfiguration.BuildDirectory" type="QString">%(out_dir)s</value>
          <value key="ProjectExplorer.BuildConfiguration.ToolChain" type="QString">INVALID</value>

          <valuemap key="ProjectExplorer.BuildConfiguration.BuildStep.0" type="QVariantMap">
           <valuelist key="GenericProjectManager.GenericMakeStep.BuildTargets" type="QVariantList"/>
           <valuelist key="GenericProjectManager.GenericMakeStep.MakeArguments" type="QVariantList">
            <value type="QString">%(project_target)s</value>
           </valuelist>
           <value key="GenericProjectManager.GenericMakeStep.MakeCommand" type="QString">ninja</value>
           <value key="ProjectExplorer.ProjectConfiguration.DisplayName" type="QString">Ninja</value>
           <value key="ProjectExplorer.ProjectConfiguration.Id" type="QString">GenericProjectManager.GenericMakeStep</value>
          </valuemap>

          <value key="ProjectExplorer.BuildConfiguration.BuildStepsCount" type="int">1</value>

          <valuemap key="ProjectExplorer.BuildConfiguration.CleanStep.0" type="QVariantMap">
           <valuelist key="GenericProjectManager.GenericMakeStep.BuildTargets" type="QVariantList"/>
           <valuelist key="GenericProjectManager.GenericMakeStep.MakeArguments" type="QVariantList">
            <value type="QString">-tclean</value>
           </valuelist>
           <value key="GenericProjectManager.GenericMakeStep.MakeCommand" type="QString">ninja</value>
           <value key="ProjectExplorer.ProjectConfiguration.DisplayName" type="QString">Ninja</value>
           <value key="ProjectExplorer.ProjectConfiguration.Id" type="QString">GenericProjectManager.GenericMakeStep</value>
          </valuemap>
          <value key="ProjectExplorer.BuildConfiguration.CleanStepsCount" type="int">1</value>
          <value key="ProjectExplorer.BuildConfiguration.ClearSystemEnvironment" type="bool">false</value>
          <valuelist type="QVariantList" key="ProjectExplorer.BuildConfiguration.UserEnvironmentChanges">
           <value type="QString">TARGET=%(project_target)s</value>
          </valuelist>
         </valuemap>
      """ % {
          'project_number': project_number,
          'project_name': project_name,
          'project_target': project_target,
          'out_dir': project_def['out_dir'],
      })
      project_number += 1

    f.write("""
         <value key="ProjectExplorer.Target.BuildConfigurationCount" type="int">%d</value>
    """ % project_number)

    f.write("""
         <valuemap key="ProjectExplorer.Target.RunConfiguration.0" type="QVariantMap">
          <valuelist key="ProjectExplorer.CustomExecutableRunConfiguration.Arguments" type="QVariantList"/>
          <value key="ProjectExplorer.CustomExecutableRunConfiguration.BaseEnvironmentBase" type="int">2</value>
          <value key="ProjectExplorer.CustomExecutableRunConfiguration.Executable" type="QString">./$TARGET</value>
          <value key="ProjectExplorer.CustomExecutableRunConfiguration.UseTerminal" type="bool">false</value>
          <value key="ProjectExplorer.CustomExecutableRunConfiguration.WorkingDirectory" type="QString">$BUILDDIR</value>
          <value key="ProjectExplorer.ProjectConfiguration.DisplayName" type="QString">Run Target</value>
          <value key="ProjectExplorer.ProjectConfiguration.Id" type="QString">ProjectExplorer.CustomExecutableRunConfiguration</value>
          <value type="bool" key="RunConfiguration.UseCppDebugger">true</value>
          <value type="bool" key="RunConfiguration.UseCppDebuggerAuto">false</value>
         </valuemap>

         <valuemap key="ProjectExplorer.Target.RunConfiguration.1" type="QVariantMap">
          <valuelist key="ProjectExplorer.CustomExecutableRunConfiguration.Arguments" type="QVariantList"/>
          <value key="ProjectExplorer.CustomExecutableRunConfiguration.BaseEnvironmentBase" type="int">2</value>
          <value key="ProjectExplorer.CustomExecutableRunConfiguration.Executable" type="QString">valgrind ./$TARGET</value>
          <value key="ProjectExplorer.CustomExecutableRunConfiguration.UseTerminal" type="bool">false</value>
          <value key="ProjectExplorer.CustomExecutableRunConfiguration.WorkingDirectory" type="QString">$BUILDDIR</value>
          <value key="ProjectExplorer.ProjectConfiguration.DisplayName" type="QString">Valgrind Target</value>
          <value key="ProjectExplorer.ProjectConfiguration.Id" type="QString">ProjectExplorer.CustomExecutableRunConfiguration</value>
          <value type="bool" key="RunConfiguration.UseCppDebugger">true</value>
          <value type="bool" key="RunConfiguration.UseCppDebuggerAuto">false</value>
         </valuemap>

         <value key="ProjectExplorer.Target.RunConfigurationCount" type="int">2</value>
    """)

    f.write("""
        </valuemap>
       </data>
    """)
    f.write("""
       <data>
        <variable>ProjectExplorer.Project.TargetCount</variable>
        <value type="int">1</value>
       </data>
       <data>
        <variable>ProjectExplorer.Project.Updater.FileVersion</variable>
        <value type="int">6</value>
       </data>
      </qtcreator>
      """)


def GenerateProjects(definition):
  """Generates a solution and all corresponding projects.

  Args:
    definition: A dictionary specifying all solution and project parameters.
  Raises:
    RuntimeError: if the specified repo directory cannot be found.
  """

  repo_dir = os.path.abspath(definition['repo_dir'])
  projects_dir = os.path.abspath(definition['projects_dir'])

  if not os.path.exists(repo_dir):
    raise RuntimeError('Repo directory not found: {}'.format(repo_dir))

  # Generate project files
  for proj in definition['projects']:
    GenProject(proj, projects_dir)


def GetProjectsDirectory(output_dir):
  return os.path.join(output_dir, PROJECT_DIR_RELATIVE_TO_OUTPUT_DIR)


def GetValue(dictionary, path, default=None):
  """Returns a value from a nest dictionaries.

  Example: d = { 'base': { 'child': 1 } }
           GetValue(d, 'base/child') == 1
           GetValue(d, 'base/not_found', 6) == 6

  Args:
    dictionary: The dictionary of dictionaries to traverse.
    path: A string indicating the value we want to extract from dictionary.
    default: A string to use if the specified value can't be found.
  Returns:
    Returns the value from dictionary specified by path.
  """

  path = path.split('/')
  for p in path[:-1]:
    dictionary = dictionary.get(p, {})
  return dictionary.get(path[-1], default)


def GetSet(dictionary, value):
  return set(dictionary.get(value, []))


def GenerateQTCreatorFiles(target_dicts, params):
  """Transforms GYP data into a more suitable form and feed it to the generator.

  The function will collapse individual targets into a single data set which
  is broken down by configuration types.

  Args:
    target_dicts: Dictionaries specifying targets as provided by Gyp.
    params: Parameters used for the current Gyp build.
  Raises:
    RuntimeError: Thrown if assumptions on the input parameters are not met.
  """

  generator_flags = params['generator_flags']
  current_config = generator_flags['config']
  repo_dir = params['options'].toplevel_dir

  session_name_prefix = generator_flags['qtcreator_session_name_prefix']
  project_list = set()
  sources = set()

  configuration = {'include_paths': set(), 'defines': set()}

  if len(params['build_files']) != 1:
    raise RuntimeError('Expected only a single Gyp build file.')

  output_dir = os.path.join(repo_dir, generator_flags['output_dir'])
  projects_dir = GetProjectsDirectory(output_dir)
  configuration['out_dir'] = current_config

  # At the moment we are not tracking which dependencies were actually used
  # to construct each executable target and instead pretend that all of them
  # are needed for each executable target
  for target_name, target in target_dicts.iteritems():
    gyp_abspath = os.path.abspath(target_name[:target_name.rfind(':')])
    gyp_dirname = os.path.dirname(gyp_abspath)

    config = target['configurations'][current_config]
    configuration['defines'] |= (
        GetSet(config, 'defines') - GetSet(config, 'defines_excluded'))
    configuration['include_paths'] |= RebaseRelativePaths(
        (GetSet(config, 'include_dirs')
         | GetSet(config, 'include_dirs_target')), gyp_dirname, projects_dir)

    sources |= RebaseRelativePaths(
        (GetSet(target, 'sources') - GetSet(target, 'sources_excluded')),
        gyp_dirname, projects_dir)
    sources |= set([os.path.relpath(gyp_abspath, projects_dir)])
    # Generate projects for targets with the ide_deploy_target variable set.
    if GetValue(target, 'variables/ide_deploy_target', 0):
      executable_target = GetValue(target, 'variables/executable_name')
      assert executable_target
      executable_folder = os.path.relpath(gyp_dirname, repo_dir)
      project_list.add((executable_folder, executable_target))

  projects = [{
      'session_name_prefix': session_name_prefix,
      'out_dir': os.path.join(output_dir, current_config),
      'name': current_config,
      'sources': sources,
      'configuration': configuration,
      'projects': project_list,
  }]

  definition = {
      'name': current_config,
      'projects_dir': projects_dir,
      'repo_dir': repo_dir,
      'projects': projects,
  }

  GenerateProjects(definition)


#
# GYP generator external functions
#
def PerformBuild(data, configurations, params):  # pylint: disable=unused-argument
  # Not used by this generator
  pass


def GenerateOutput(target_list, target_dicts, data, params):  # pylint: disable=unused-argument
  """Generates QT Creator project files for Linux.

  A BuildConfiguration will be generated for each executable target that
  contains a deploy step.
  Example:
    {
      'target_name': 'project_name_deploy',
      'variables': {
        'executable_name': 'project_name',
      },
    },

  A project file with all targets will be generated for each configuration
  (ex. Debug, Devel, etc..).

  Args:
    target_list: Unused.
    target_dicts: List of dictionaries specifying Gyp targets.
    data: Unused.
    params: Gyp parameters.
  """
  GenerateQTCreatorFiles(target_dicts, params)
