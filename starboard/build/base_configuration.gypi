# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
      'sb_enable_lib%': 0,
      # TODO: Remove the "data" subdirectory.
      'sb_static_contents_output_data_dir%': '<(PRODUCT_DIR)/content/data',
      'sb_deploy_output_dir%': '<(PRODUCT_DIR)/deploy',
      'sb_evergreen_compatible%': 0,
      'sb_evergreen_compatible_libunwind%': 0,
      'sb_evergreen_compatible_lite%': 0,
    },

    # Enables the yasm compiler to be used to compile .asm files.
    'yasm_exists%': 0,

    # Where yasm can be found on the target device.
    'path_to_yasm%': "yasm",

    # The Starboard API version of the current build configuration. The default
    # value is meant to be overridden by a Starboard ABI file.
    'sb_api_version%': 0,

    # Enabling this variable enables pedantic levels of warnings for the current
    # toolchain.
    'sb_pedantic_warnings%': 0,

    # Enables embedding Cobalt as a shared library within another app. This
    # requires a 'lib' starboard implementation for the corresponding platform.
    'sb_enable_lib%': '<(sb_enable_lib)',

    # Disables an NPLB audit of C++17 support.
    'sb_disable_cpp17_audit': 0,

    # When this is set to true, the web bindings for the microphone
    # are disabled
    'sb_disable_microphone_idl%': 0,

    # Directory path to static contents' data.
    'sb_static_contents_output_data_dir%': '<(sb_static_contents_output_data_dir)',

    # Top-level directory for staging deploy build output. Platform deploy
    # actions should use <(target_deploy_dir) defined in deploy.gypi to place
    # artifacts for each deploy target in its own subdirectoy.
    'sb_deploy_output_dir%': '<(sb_deploy_output_dir)',

    # Contains the name of the hosting OS. The value is defined by the gyp
    # wrapper script.
    'host_os%': 'win',

    # The target platform id as a string, like 'linux-x64x11', 'win32', etc..
    'sb_target_platform': '',

    # Whether this is an evergreen build.
    'sb_evergreen': 0,

    # Whether this is an evergreen compatible platform. A compatible platform
    # can run the elf_loader and launch the evergreen build.
    'sb_evergreen_compatible%': '<(sb_evergreen_compatible)',

    # Whether to use the libunwind library on evergreen compatible platform.
    'sb_evergreen_compatible_libunwind%': '<(sb_evergreen_compatible_libunwind)',

    # Whether to adopt Evergreen Lite on the evergreen compatible platform.
    'sb_evergreen_compatible_lite%': '<(sb_evergreen_compatible_lite)',

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
    #   'system_gles3' - Deprecated. Use system_gles2 instead.
    #                    Use the system implementation of EGL + GLES3. The
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

    # Used to indicate that the player is filter based.
    'sb_filter_based_player%': 1,

    # Used to enable benchmarks.
    'sb_enable_benchmark%': 0,

    # This variable dictates whether a given gyp target should be compiled with
    # optimization flags for size vs. speed.
    'optimize_target_for_speed%': 0,

    # Compiler configuration.

    # The following variables are used to specify compiler and linker
    # flags by build configuration. Platform specific .gypi includes and
    # .gyp targets can use them to add additional flags.

    'compiler_flags%': [],
    'linker_flags%': [],

    # TODO: Replace linker_flags_(config) with linker_shared_flags_(config)
    # and linker_executable_flags_(config) to distinguish the flags for
    # SharedLibraryLinker and ExecutableLinker.
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

    # For qa and gold configs, different compiler flags may be specified for
    # gyp targets that should be built for size than for speed. Targets which
    # specify 'optimize_target_for_speed == 1', will compile with flags:
    #   ['compiler_flags_*<config>', 'compiler_flags_*<config>_speed'].
    # Otherwise, targets will use compiler flags:
    #   ['compiler_flags_*<config>', 'compiler_flags_*<config>_size'].
    # Platforms may decide to use the same optimization flags for both
    # target types by leaving the '*_size' and '*_speed' variables empty.
    'compiler_flags_qa%': [],
    'compiler_flags_qa_size%': [],
    'compiler_flags_qa_speed%': [],
    'compiler_flags_c_qa%': [],
    'compiler_flags_c_qa_size%': [],
    'compiler_flags_c_qa_speed%': [],
    'compiler_flags_cc_qa%': [],
    'compiler_flags_cc_qa_size%': [],
    'compiler_flags_cc_qa_speed%': [],
    'linker_flags_qa%': [],
    'defines_qa%': [],

    'compiler_flags_gold%': [],
    'compiler_flags_gold_size%': [],
    'compiler_flags_gold_speed%': [],
    'compiler_flags_c_gold%': [],
    'compiler_flags_c_gold_size%': [],
    'compiler_flags_c_gold_speed%': [],
    'compiler_flags_cc_gold%': [],
    'compiler_flags_cc_gold_size%': [],
    'compiler_flags_cc_gold_speed%': [],
    'linker_flags_gold%': [],
    'defines_gold%': [],

    'compiler_flags_host%': [],
    'compiler_flags_c_host%': [],
    'defines_host%': [],

    'platform_libraries%': [],

    # TODO: Remove these compatibility variables from Starboard.

    # List of platform-specific targets that get compiled into cobalt.
    'cobalt_platform_dependencies%': [],

    'conditions': [
      ['host_os=="linux"', {
        'conditions': [
          ['target_arch=="arm" or target_arch=="x86"', {
            # All the 32 bit CPU architectures v8 supports.
            'compiler_flags_cc_host%': [
              '-m32',
              '--std=gnu++14',
            ],
            'compiler_flags_host': [
              '-m32',
            ],
            'compiler_flags_host': [
              '-m32',
            ],
            'linker_flags_host%': [
              '-target', 'i386-unknown-linux-gnu',
              '-pthread',
              '-latomic',
            ],
          }, {
            'compiler_flags_cc_host%': [
              '--std=gnu++14',
            ],
            'linker_flags_host%': [
            '-pthread',
            ],
          }],
        ],
      }, {
        'compiler_flags_cc_host%': [],
        'linker_flags_host%': [],
      }],
    ],
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

      # This workaround is used to surface the default setting for the variable
      # 'optimize_target_for_speed' if the gyp target does not explicitly set
      # it.
      'optimize_target_for_speed%': '<(optimize_target_for_speed)',
    },
    'cflags': [ '<@(compiler_flags)' ],
    'ldflags': [ '<@(linker_flags)' ],
    'cflags_host': [ '<@(compiler_flags_host)', ],
    'cflags_c_host': [ '<@(compiler_flags_c_host)', ],
    'cflags_cc_host': [ '<@(compiler_flags_cc_host)', ],
    'ldflags_host': [ '<@(linker_flags_host)' ],

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
        'target_conditions': [
          ['optimize_target_for_speed==1', {
            'cflags': [
              '<@(compiler_flags_qa)',
              '<@(compiler_flags_qa_speed)',
            ],
            'cflags_c': [
              '<@(compiler_flags_c_qa)',
              '<@(compiler_flags_c_qa_speed)',
            ],
            'cflags_cc': [
              '<@(compiler_flags_cc_qa)',
              '<@(compiler_flags_cc_qa_speed)',
            ],
          }, {
            'cflags': [
              '<@(compiler_flags_qa)',
              '<@(compiler_flags_qa_size)',
            ],
            'cflags_c': [
              '<@(compiler_flags_c_qa)',
              '<@(compiler_flags_c_qa_size)',
            ],
            'cflags_cc': [
              '<@(compiler_flags_cc_qa)',
              '<@(compiler_flags_cc_qa_size)',
            ],
          }]
        ],
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
        'target_conditions': [
          ['optimize_target_for_speed==1', {
            'cflags': [
              '<@(compiler_flags_gold)',
              '<@(compiler_flags_gold_speed)',
            ],
            'cflags_c': [
              '<@(compiler_flags_c_gold)',
              '<@(compiler_flags_c_gold_speed)',
            ],
            'cflags_cc': [
              '<@(compiler_flags_cc_gold)',
              '<@(compiler_flags_cc_gold_speed)',
            ],
          }, {
            'cflags': [
              '<@(compiler_flags_gold)',
              '<@(compiler_flags_gold_size)',
            ],
            'cflags_c': [
              '<@(compiler_flags_c_gold)',
              '<@(compiler_flags_c_gold_size)',
            ],
            'cflags_cc': [
              '<@(compiler_flags_cc_gold)',
              '<@(compiler_flags_cc_gold_size)',
            ],
          }]
        ],
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
