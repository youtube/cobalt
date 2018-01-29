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

#####################################################################
# If you modify this file, PLEASE REMEMBER TO UPDATE
# //starboard/build/config/base.gni AS WELL
#####################################################################

# This contains the defaults and documentation for all Starboard-defined GYP
# variables. Applications that build on top of Starboard are likely to have
# their own base configuration, as well.
#
# starboard/build/gyp_runner includes this file automatically in all .gyp files
# that it processes.

{
  'variables': {
    # We need to define some variables inside of an inner 'variables' scope
    # so that they can be referenced by other outer variables here.  Also, it
    # allows for the specification of default values that get referenced by
    # a top level scope.
    'variables': {
      # 'sb_enable_lib' is initially defined inside this inner 'variables' dict
      # so that it can be accessed by 'sb_enable_lib' below.
      'sb_enable_lib%': 0,
    },

    # Enabling this variable enables pedantic levels of warnings for the current
    # toolchain.
    'sb_pedantic_warnings%': 0,

    # Enables embedding Cobalt as a shared library within another app. This
    # requires a 'lib' starboard implementation for the corresponding platform.
    'sb_enable_lib%': '<(sb_enable_lib)',

    # When this is set to true, the web bindings for the microphone
    # are disabled
    'sb_disable_microphone_idl%': 0,

    # Directory path to static contents.
    'sb_static_contents_output_base_dir%': '<(PRODUCT_DIR)/content',

    # Directory path to static contents' data.
    'sb_static_contents_output_data_dir%': '<(PRODUCT_DIR)/content/data',

    # Contains the name of the hosting OS. The value is defined by the gyp
    # wrapper script.
    'host_os%': 'win',

    # The target platform id as a string, like 'linux-x64x11', 'win32', etc..
    'sb_target_platform': '',

    # The operating system of the target, separate from the target_arch. In many
    # cases, an 'unknown' value is fine, but, if set to 'linux', then we can
    # assume some things, and it'll save us some configuration time.
    'target_os%': 'unknown',

    # The variables allow changing the target type on platforms where the
    # native code may require an additional packaging step (ex. Android).
    'gtest_target_type%': 'executable',
    'final_executable_type%': 'executable',

    # Halt execution on failure to allocate memory.
    'abort_on_allocation_failure%': 1,

    # The relative path from src/ to the directory containing the
    # starboard_platform.gyp file.  It is currently set to
    # 'starboard/<(target_arch)' to make semi-starboard platforms work.
    # TODO: Set the default value to '' once all semi-starboard platforms are
    # moved to starboard.
    'starboard_path%': 'starboard/<(target_arch)',

    # The source of EGL and GLES headers and libraries.
    # Valid values (case and everything sensitive!):
    #   'none'   - No EGL + GLES implementation is available on this platform.
    #   'system_gles3' - Use the system implementation of EGL + GLES3. The
    #                    headers and libraries must be on the system include and
    #                    link paths.
    #   'system_gles2' - Use the system implementation of EGL + GLES2. The
    #                    headers and libraries must be on the system include and
    #                    link paths.
    #   'glimp'  - Cobalt's own EGL + GLES2 implementation. This requires a
    #              valid Glimp implementation for the platform.
    #   'angle'  - A DirectX-to-OpenGL adaptation layer. This requires a valid
    #              ANGLE implementation for the platform.
    # Choosing an unsupported value will result in a GYP error:
    #   "cobalt/renderer/egl_and_gles/egl_and_gles_<gl_type>.gyp not found"
    'gl_type%': 'system_gles2',

    # Temporary indicator for Tizen - should eventually move to feature defines.
    'tizen_os%': 0,

    # The event polling mechanism available on this platform to support libevent.
    # Platforms may redefine to 'poll' if necessary.
    # Other mechanisms, e.g. devpoll, kqueue, select, are not yet supported.
    'sb_libevent_method%': 'epoll',

    # Used to pick a proper media platform.
    'sb_media_platform%': 'starboard',

    # Compiler configuration.

    # The following variables are used to specify compiler and linker
    # flags by build configuration. Platform specific .gypi includes and
    # .gyp targets can use them to add additional flags.

    'compiler_flags%': [],
    'linker_flags%': [],

    'compiler_flags_debug%': [],
    'compiler_flags_c_debug%': [],
    'compiler_flags_cc_debug%': [],
    'linker_flags_debug%': [],
    'defines_debug%': [],

    'compiler_flags_devel%': [],
    'compiler_flags_c_devel%': [],
    'compiler_flags_cc_devel%': [],
    'linker_flags_devel%': [],
    'defines_devel%': [],

    'compiler_flags_qa%': [],
    'compiler_flags_c_qa%': [],
    'compiler_flags_cc_qa%': [],
    'linker_flags_qa%': [],
    'defines_qa%': [],

    'compiler_flags_gold%': [],
    'compiler_flags_c_gold%': [],
    'compiler_flags_cc_gold%': [],
    'linker_flags_gold%': [],
    'defines_gold%': [],

    'compiler_flags_host%': [],
    'compiler_flags_c_host%': [],
    'compiler_flags_cc_host%': [],
    'linker_flags_host%': [],
    'defines_host%': [],

    'platform_libraries%': [],

    # TODO: Remove these compatibility variables from Starboard.

    # List of platform-specific targets that get compiled into cobalt.
    'cobalt_platform_dependencies%': [],
  },

  'target_defaults': {
    'variables': {
      # The condition that operates on sb_pedantic_warnings is in a
      # target_conditions section, and will not have access to the default
      # fallback value of sb_pedantic_warnings at the top of this file,
      # or to the sb_pedantic_warnings variable placed at the root
      # variables scope of .gyp files, because those variables are not set at
      # target scope.  As a workaround, if sb_pedantic_warnings is not
      # set at target scope, define it in target scope to contain whatever
      # value it has during early variable expansion. That's enough to make
      # it available during target conditional processing.
      'sb_pedantic_warnings%': '<(sb_pedantic_warnings)',
    },
    'cflags': [ '<@(compiler_flags)' ],
    'ldflags': [ '<@(linker_flags)' ],
    'cflags_host': [ '<@(compiler_flags_host)', ],
    'cflags_c_host': [ '<@(compiler_flags_c_host)', ],
    'cflags_cc_host': [ '<@(compiler_flags_cc_host)', ],
    'ldflags_host': [ '<@(linker_flags_host)' ],

    # Location of Cygwin which is used by the build system when running on a
    # Windows platform.
    'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],

    # Allows any source file to include files relative to the source tree.
    'include_dirs': [ '<(DEPTH)' ],
    'libraries': [ '<@(platform_libraries)' ],

    'conditions': [
      ['final_executable_type=="shared_library"', {
        'target_conditions': [
          ['_toolset=="target"', {
            'defines': [
              # Rewrite main() functions into StarboardMain. TODO: This is a
              # hack, it would be better to be more surgical, here.
              'main=StarboardMain',
            ],
            'cflags': [
              # To link into a shared library on Linux and similar platforms,
              # the compiler must be told to generate Position Independent Code.
              # This appears to cause errors when linking the code statically,
              # however.
              '-fPIC',
            ],
          }],
        ],
      }],
      ['OS == "starboard"', {
        'target_conditions': [
          ['_toolset=="target"', {
            # Keeps common Starboard target changes in the starboard project.
            'includes': [
              '../../starboard/starboard_base_target.gypi',
            ],
          }],
        ],
      }],  # OS == "starboard"
    ],

    # TODO: Revisit and remove unused configurations.
    'configurations': {
      'debug_base': {
        'abstract': 1,
        # no optimization, include symbols:
        'cflags': [ '<@(compiler_flags_debug)' ],
        'cflags_c': [ '<@(compiler_flags_c_debug)' ],
        'cflags_cc': [ '<@(compiler_flags_cc_debug)' ],
        'ldflags': [ '<@(linker_flags_debug)' ],
        'defines': [
          '<@(defines_debug)',
          'STARBOARD_BUILD_TYPE_DEBUG',
          '_DEBUG',
        ],
      }, # end of debug_base
      'devel_base': {
        'abstract': 1,
        # optimize, include symbols:
        'cflags': [ '<@(compiler_flags_devel)' ],
        'cflags_c': [ '<@(compiler_flags_c_devel)' ],
        'cflags_cc': [ '<@(compiler_flags_cc_devel)' ],
        'ldflags': [ '<@(linker_flags_devel)' ],
        'defines': [
          '<@(defines_devel)',
          'STARBOARD_BUILD_TYPE_DEVEL',
          '_DEBUG',
        ],
      }, # end of devel_base
      'qa_base': {
        'abstract': 1,
        # optimize:
        'cflags': [ '<@(compiler_flags_qa)' ],
        'cflags_c': [ '<@(compiler_flags_c_qa)' ],
        'cflags_cc': [ '<@(compiler_flags_cc_qa)' ],
        'ldflags': [ '<@(linker_flags_qa)' ],
        'defines': [
          '<@(defines_qa)',
          'STARBOARD_BUILD_TYPE_QA',
          'NDEBUG',
        ],
      }, # end of devel_base
      'gold_base': {
        'abstract': 1,
        # optimize:
        'cflags': [ '<@(compiler_flags_gold)' ],
        'cflags_c': [ '<@(compiler_flags_c_gold)' ],
        'cflags_cc': [ '<@(compiler_flags_cc_gold)' ],
        'ldflags': [ '<@(linker_flags_gold)' ],
        'defines': [
          '<@(defines_gold)',
          'STARBOARD_BUILD_TYPE_GOLD',
          'NDEBUG',
        ],
      }, # end of gold_base
    }, # end of configurations
  }, # end of target_defaults

  'conditions': [
    ['cobalt_config != "gold"', {
      'variables' : {
        'sb_allows_memory_tracking': 1,
      },
    },
    {
      'variables' : {
        'sb_allows_memory_tracking': 0,
      },
    }],
  ],
}
