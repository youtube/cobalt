# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Android shared platform configuration for gyp_cobalt."""

from __future__ import print_function

import imp  # pylint: disable=deprecated-module
import os
import subprocess

from starboard.android.shared import sdk_utils
from starboard.build import clang as clang_build
from starboard.build.platform_configuration import PlatformConfiguration
from starboard.tools import build
from starboard.tools.testing import test_filter
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import bash
from starboard.tools.toolchain import clang
from starboard.tools.toolchain import clangxx
from starboard.tools.toolchain import cp
from starboard.tools.toolchain import touch

_APK_DIR = os.path.join(os.path.dirname(__file__), os.path.pardir, 'apk')
_APK_BUILD_ID_FILE = os.path.join(_APK_DIR, 'build.id')
_COBALT_GRADLE = os.path.join(_APK_DIR, 'cobalt-gradle.sh')

# The API level of NDK tools to use. This should be the minimum API level on
# which the app is expected to run. If some feature from a newer NDK level is
# needed, this may be increased with caution.
# https://developer.android.com/ndk/guides/stable_apis.html
#
# Using 24 will lead to missing symbols on API 23 devices.
# https://github.com/android-ndk/ndk/issues/126
_ANDROID_NDK_API_LEVEL = '21'

# Maps the Android ABI to the clang & ar executable names.
_ABI_TOOL_NAMES = {
    'x86': ('i686-linux-android{}-clang'.format(_ANDROID_NDK_API_LEVEL),
            'i686-linux-android-ar'),
    'x86_64': ('x86_64-linux-android{}-clang'.format(_ANDROID_NDK_API_LEVEL),
               'x86_64-linux-android-ar'),
    'armeabi':
        ('armv7a-linux-androideabi{}-clang'.format(_ANDROID_NDK_API_LEVEL),
         'armv7a-linux-androideabi-ar'),
    'armeabi-v7a':
        ('armv7a-linux-androideabi{}-clang'.format(_ANDROID_NDK_API_LEVEL),
         'arm-linux-androideabi-ar'),
    'arm64-v8a':
        ('aarch64-linux-android{}-clang'.format(_ANDROID_NDK_API_LEVEL),
         'aarch64-linux-android-ar'),
}


class AndroidConfiguration(PlatformConfiguration):
  """Starboard Android platform configuration."""

  # TODO: make ASAN work with NDK tools and enable it by default
  def __init__(self,
               platform,
               android_abi,
               asan_enabled_by_default=False,
               sabi_json_path='starboard/sabi/default/sabi.json'):
    super(AndroidConfiguration, self).__init__(platform,
                                               asan_enabled_by_default)
    self._target_toolchain = None
    self._host_toolchain = None

    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

    self.android_abi = android_abi
    self.sabi_json_path = sabi_json_path

    self.android_home = sdk_utils.GetSdkPath()
    self.android_ndk_home = sdk_utils.GetNdkPath()

    print('Using Android SDK at {}'.format(self.android_home))
    print('Using Android NDK at {}'.format(self.android_ndk_home))

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, configuration):
    variables = super(AndroidConfiguration, self).GetVariables(
        configuration, use_clang=1)
    variables.update({
        'ANDROID_HOME':
            self.android_home,
        'NDK_HOME':
            self.android_ndk_home,
        'ANDROID_ABI':
            self.android_abi,
        'include_path_platform_deploy_gypi':
            'starboard/android/shared/platform_deploy.gypi',
    })
    return variables

  def GetDeployPathPatterns(self):
    # example src/out/android-arm64/devel/cobalt.apk
    return ['*.apk']

  def GetGeneratorVariables(self, config_name):
    _ = config_name
    generator_variables = {
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables

  def SetupPlatformTools(self, build_number):
    sdk_utils.InstallSdkIfNeeded()
    subprocess.call([_COBALT_GRADLE, '--reset'])
    with open(_APK_BUILD_ID_FILE, 'w') as build_id_file:
      build_id_file.write('{}'.format(build_number))

  def GetEnvironmentVariables(self):
    env_variables = {}

    # Android builds tend to consume significantly more memory than the
    # default settings permit, so cap this at 1 in order to avoid build
    # issues.  Without this, 32GB machines end up getting automatically
    # configured to run 5 at a time, which can be too much for at least
    # android-arm64_debug.
    # TODO: Eventually replace this with something more robust, like an
    #       implementation of the abstract toolchain for Android.
    env_variables.update({'GYP_LINK_CONCURRENCY': '1'})

    return env_variables

  def GetTargetToolchain(self, **kwargs):
    if not self._target_toolchain:
      tool_prefix = os.path.join(sdk_utils.GetNdkPath(), 'toolchains', 'llvm',
                                 'prebuilt', 'linux-x86_64', 'bin', '')
      cc_path = self.build_accelerator + ' ' + tool_prefix + _ABI_TOOL_NAMES[
          self.android_abi][0]
      cxx_path = cc_path + '++'
      ar_path = tool_prefix + _ABI_TOOL_NAMES[self.android_abi][1]
      clang_flags = [
          # libwebp uses the cpufeatures library to detect ARM NEON support
          '-I{}/sources/android/cpufeatures'.format(self.android_ndk_home),

          # Mimic build/cmake/android.toolchain.cmake in the Android NDK.
          '-ffunction-sections',
          '-funwind-tables',
          '-fstack-protector-strong',
          '-no-canonical-prefixes',

          # Other flags
          '-fsigned-char',
          '-fno-limit-debug-info',
          '-fno-exceptions',
          '-fcolor-diagnostics',
          '-fno-strict-aliasing',  # See http://crbug.com/32204

          # Default visibility is hidden to enable dead stripping.
          '-fvisibility=hidden',
          # Any warning will stop the build.
          '-Werror',

          # Disable errors for the warning till the Android NDK r19 is fixed.
          # The warning is trigger when compiling .c files and complains for
          # '-stdlib=libc++' which is added by the NDK.
          '-Wno-error=unused-command-line-argument',

          # Don't warn about register variables (in base and net)
          '-Wno-deprecated-register',
          # Don't warn about deprecated ICU methods (in googleurl and net)
          '-Wno-deprecated-declarations',
          # Skia doesn't use overrides.
          '-Wno-inconsistent-missing-override',
          # shifting a negative signed value is undefined
          '-Wno-shift-negative-value',
          # Don't warn for implicit sign conversions. (in v8 and protobuf)
          '-Wno-sign-conversion',
      ]
      clang_defines = [
          # Enable compile-time decisions based on the ABI
          'ANDROID_ABI={}'.format(self.android_abi),
          # -DANDROID is an argument to some ifdefs in the NDK's eglplatform.h
          'ANDROID',
          # Cobalt on Linux flag
          'COBALT_LINUX',
          # So that we get the PRI* macros from inttypes.h
          '__STDC_FORMAT_MACROS',
          # Enable GNU extensions to get prototypes like ffsl.
          '_GNU_SOURCE=1',
          # Undefining __linux__ causes the system headers to make wrong
          # assumptions about which C-library is used on the platform.
          '__BIONIC__',
          # Undefining __linux__ leaves libc++ without a threads implementation.
          # TODO: See if there's a way to make libcpp threading use Starboard.
          '_LIBCPP_HAS_THREAD_API_PTHREAD',
      ]
      linker_flags = [
          # Use the static LLVM libc++.
          '-static-libstdc++',

          # Mimic build/cmake/android.toolchain.cmake in the Android NDK, but
          # force build-id to sha1, so that older lldb versions can still find
          # debugsymbols, see https://github.com/android-ndk/ndk/issues/885
          '-Wl,--build-id=sha1',
          '-Wl,--warn-shared-textrel',
          '-Wl,--fatal-warnings',
          '-Wl,--gc-sections',
          '-Wl,-z,nocopyreloc',

          # Wrapper synchronizes punch-out video bounds with the UI frame.
          '-Wl,--wrap=eglSwapBuffers',
      ]
      self._target_toolchain = [
          clang.CCompiler(
              path=cc_path, defines=clang_defines, extra_flags=clang_flags),
          clang.CxxCompiler(
              path=cxx_path,
              defines=clang_defines,
              extra_flags=clang_flags + [
                  '-std=c++14',
              ]),
          clang.AssemblerWithCPreprocessor(
              path=cc_path, defines=clang_defines, extra_flags=clang_flags),
          ar.StaticThinLinker(path=ar_path),
          ar.StaticLinker(path=ar_path),
          clangxx.SharedLibraryLinker(path=cxx_path, extra_flags=linker_flags),
          clangxx.ExecutableLinker(path=cxx_path, extra_flags=linker_flags),
      ]
    return self._target_toolchain

  def GetHostToolchain(self, **kwargs):
    if not self._host_toolchain:
      if not hasattr(self, 'host_compiler_environment'):
        self.host_compiler_environment = build.GetHostCompilerEnvironment(
            clang_build.GetClangSpecification(), self.build_accelerator)
      cc_path = self.host_compiler_environment['CC_host']
      cxx_path = self.host_compiler_environment['CXX_host']
      self._host_toolchain = [
          clang.CCompiler(path=cc_path),
          clang.CxxCompiler(path=cxx_path),
          clang.AssemblerWithCPreprocessor(path=cc_path),
          ar.StaticThinLinker(),
          ar.StaticLinker(),
          clangxx.ExecutableLinker(path=cxx_path),
          clangxx.SharedLibraryLinker(path=cxx_path),
          cp.Copy(),
          touch.Stamp(),
          bash.Shell(),
      ]
    return self._host_toolchain

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), 'launcher.py'))
    launcher_module = imp.load_source('launcher', module_path)
    return launcher_module

  def GetTestFilters(self):
    filters = super(AndroidConfiguration, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  # A map of failing or crashing tests per target.
  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'player_filter_tests': [
          # GetMaxNumberOfCachedFrames() on Android is device dependent,
          # and Android doesn't provide an API to get it. So, this function
          # doesn't make sense on Android. But HoldFramesUntilFull tests depend
          # on this number strictly.
          'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/*',

          # Currently, invalid input tests are not supported.
          'VideoDecoderTests/VideoDecoderTest.SingleInvalidInput/*',
          'VideoDecoderTests/VideoDecoderTest'
          '.MultipleValidInputsAfterInvalidKeyFrame/*',
          'VideoDecoderTests/VideoDecoderTest.MultipleInvalidInput/*',

          # Android currently does not support multi-video playback, which
          # the following tests depend upon.
          'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/*',

          # The video pipeline will hang if it doesn't receive any input.
          'PlayerComponentsTests/PlayerComponentsTest.EOSWithoutInput/*',

          # The e/eac3 audio time reporting during pause will be revisitied.
          'PlayerComponentsTests/PlayerComponentsTest.Pause/15',
      ],
      'nplb': [
          # This test is failing because localhost is not defined for IPv6 in
          # /etc/hosts.
          'SbSocketAddressTypes/SbSocketResolveTest.Localhost/1',

          # These tests are taking longer due to interop on android. Work is
          # underway to investigate whether this is acceptable.
          'SbMediaCanPlayMimeAndKeySystem.ValidatePerformance',
          'SbMediaConfigurationTest.ValidatePerformance',

          # SbDirectory has problems with empty Asset dirs.
          'SbDirectoryCanOpenTest.SunnyDayStaticContent',
          'SbDirectoryGetNextTest.SunnyDayStaticContent',
          'SbDirectoryOpenTest.SunnyDayStaticContent',
          'SbFileGetPathInfoTest.WorksOnStaticContentDirectories',

          # These tests are disabled due to not receiving the kEndOfStream
          # player state update within the specified timeout.
          'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.NoInput/*',

          # Android does not use SbDrmSessionClosedFunc, which these tests
          # depend on.
          'SbDrmSessionTest.SunnyDay',
          'SbDrmSessionTest.CloseDrmSessionBeforeUpdateSession',

          # This test is failing because Android calls the
          # SbDrmSessionUpdateRequestFunc with SbDrmStatus::kSbDrmStatusSuccess
          # when invalid initialization data is passed to
          # SbDrmGenerateSessionUpdateRequest().
          'SbDrmSessionTest.InvalidSessionUpdateRequestParams',
      ],
  }

  def GetPathToSabiJsonFile(self):
    return self.sabi_json_path
