# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

# This file is meant to be included into a platform deploy gypi to provide an
# action to collect static content that should be deployed with a given target.
#
# The action builds a symlink farm in the content/deploy/<executable_name>
# directory, pointing to the subset of data in content/data that was populated
# by some dependency of the target being deployed.
{
  'variables': {
    # Root of the content tree that should be deployed with a given target.
    'content_deploy_dir': '<(target_deploy_dir)/content',

    # Stamp file that will be updated after the content symlink farm is built.
    'content_deploy_stamp_file': '<(content_deploy_dir).stamp',

    # This is a list of relative paths within both input_dir and output_dir,
    # and is named such that GYP does not relativize it. Values are merged in
    # from the all_dependent_settings blocks of gypi files that copy static data
    # into content/data.
    'content_deploy_subdirs': [],

    # Some platforms will symlink to a folder containing relative symlinks,
    # and this will cause an invalid path. To get around this these platforms
    # should set |content_deploy_use_absolute_symlinks| to 1.
    'content_deploy_use_absolute_symlinks%': 0,

    # Some platforms can only support a limited depth of directory
    # structure, and this allows the depth of deploy content to be
    # checked not to exceed that.
    'content_deploy_max_depth%': 0,

    # Implementation detail to add conditional args.
    'collect_deploy_content_extra_args': [],
  },
  'conditions': [
    ['content_deploy_use_absolute_symlinks == 1', {
      'variables': {
        'collect_deploy_content_extra_args': [ '--use_absolute_symlinks' ],
      }
    }],
    ['cobalt_docker_build == 1 and host_os == "win" and cobalt_fastbuild != 1', {
      'variables': {
        'collect_deploy_content_extra_args': [ '--copy_override' ],
      }
    }],
  ],
  'actions': [{
    'action_name': 'collect_deploy_content',
    'variables': {
      'input_dir': '<(sb_static_contents_output_data_dir)',
      'output_dir': '<(content_deploy_dir)',
    },
    # Re-collect the content whenever the executable is rebuilt, but wait until
    # cleaning the deploy dir is done.
    'inputs': [
        '<(executable_file)',
        '<(target_deploy_stamp_file)',
    ],
    'outputs': [ '<(content_deploy_stamp_file)' ],
    'action': [
      'python2',
      '<(DEPTH)/starboard/build/collect_deploy_content.py',
      '-i', '<(input_dir)',
      '-o', '<(output_dir)',
      '-s', '<(content_deploy_stamp_file)',
      '--max_depth', '<(content_deploy_max_depth)',
      '<@(collect_deploy_content_extra_args)',
      '>@(content_deploy_subdirs)',
    ],
    'message': 'Collect content: <(content_deploy_dir)',
  }],
}
