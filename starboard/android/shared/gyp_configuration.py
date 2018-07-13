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

import imp
import os
from subprocess import call

import gyp_utils
import starboard.android.shared.sdk_utils as sdk_utils
from starboard.build.platform_configuration import PlatformConfiguration
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

# Maps the Android ABI to the name of the toolchain.
_ABI_TOOLCHAIN_NAME = {
    'x86': 'i686-linux-android',
    'armeabi': 'arm-linux-androideabi',
    'armeabi-v7a': 'arm-linux-androideabi',
    'arm64-v8a': 'aarch64-linux-android',
}


class AndroidConfiguration(PlatformConfiguration):
  """Starboard Android platform configuration."""

  # TODO: make ASAN work with NDK tools and enable it by default
  def __init__(self, platform, android_abi, asan_enabled_by_default=False):
    super(AndroidConfiguration, self).__init__(platform,
                                               asan_enabled_by_default)
    self._target_toolchain = None
    self._host_toolchain = None

    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

    self.android_abi = android_abi

    self.host_compiler_environment = gyp_utils.GetHostCompilerEnvironment()
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
        'enable_remote_debugging':
            0,
        'include_path_platform_deploy_gypi':
            'starboard/android/shared/platform_deploy.gypi',
        'javascript_engine':
            'v8',
        'cobalt_enable_jit':
            1,
    })
    return variables

  def GetGeneratorVariables(self, configuration):
    _ = configuration
    generator_variables = {
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables

  def GetEnvironmentVariables(self):
    sdk_utils.InstallSdkIfNeeded(self.android_abi)
    call([_COBALT_GRADLE, '--reset'])
    with open(_APK_BUILD_ID_FILE, 'w') as build_id_file:
      build_id_file.write('{}'.format(gyp_utils.GetBuildNumber()))

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

  def GetTargetToolchain(self):
    if not self._target_toolchain:
      tool_prefix = os.path.join(
          sdk_utils.GetToolsPath(self.android_abi), 'bin',
          _ABI_TOOLCHAIN_NAME[self.android_abi] + '-')
      cc_path = tool_prefix + 'clang'
      cxx_path = tool_prefix + 'clang++'
      ar_path = tool_prefix + 'ar'
      clang_flags = [
          # We'll pretend not to be Linux, but Starboard instead.
          '-U__linux__',

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

          # Mimic build/cmake/android.toolchain.cmake in the Android NDK.
          '-Wl,--build-id',
          '-Wl,--warn-shared-textrel',
          '-Wl,--fatal-warnings',
          '-Wl,--gc-sections',
          '-Wl,-z,nocopyreloc',

          # Wrapper synchronizes punch-out video bounds with the UI frame.
          '-Wl,--wrap=eglSwapBuffers',
      ]
      self._target_toolchain = [
          clang.CCompiler(
              path=cc_path,
              defines=clang_defines,
              extra_flags=clang_flags),
          clang.CxxCompiler(
              path=cxx_path,
              defines=clang_defines,
              extra_flags=clang_flags + [
                  '-std=c++11',
              ]),
          clang.AssemblerWithCPreprocessor(
              path=cc_path,
              defines=clang_defines,
              extra_flags=clang_flags),
          ar.StaticThinLinker(path=ar_path),
          ar.StaticLinker(path=ar_path),
          clangxx.SharedLibraryLinker(path=cxx_path, extra_flags=linker_flags),
          clangxx.ExecutableLinker(path=cxx_path, extra_flags=linker_flags),
      ]
    return self._target_toolchain

  def GetHostToolchain(self):
    if not self._host_toolchain:
      cc_path = self.host_compiler_environment['CC_host'],
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
    for target, tests in self._FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  # A map of failing or crashing tests per target.
  _FILTERED_TESTS = {
      'nplb': [
          'SbAudioSinkTest.AllFramesConsumed',
          'SbAudioSinkTest.SomeFramesConsumed',
          'SbMicrophoneGetAvailableTest.LabelIsValid',
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDayDestination/0',
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDaySourceForDestination/0',
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDaySourceForDestination/1',
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDaySourceNotLoopback/0',
          'SbSocketBindTest.SunnyDayLocalInterface',
          'SbSocketGetLocalAddressTest.SunnyDayBoundSpecified',
          'SpeechRecognizerTest.StartIsCalledMultipleTimes',
          'SpeechRecognizerTest.StartRecognizerWith10MaxAlternatives',
          'SpeechRecognizerTest.StartRecognizerWithContinuousRecognition',
          'SpeechRecognizerTest.StartRecognizerWithInterimResults',
          'SpeechRecognizerTest.StartTestSunnyDay',

          # The following fail on MiBox
          'SbAudioSinkTest.ContinuousAppend',
          'SbAudioSinkTest.MultipleAppendAndConsume',
          'SbAudioSinkTest.Pause',
          # The following reboot on MiBox
          'SpeechRecognizerTest.DestroyRecognizerWithoutStopping',
          'SpeechRecognizerTest.StartWithInvalidSpeechRecognizer',
          'SpeechRecognizerTest.StopIsCalledMultipleTimes',
      ],
      'player_filter_tests': [
          # TODO: debug these crashes & failures.
          'AudioDecoderTests/AudioDecoderTest.SingleInput/0',
          'AudioDecoderTests/AudioDecoderTest.EndOfStreamWithoutAnyInput/0',
          'AudioDecoderTests/AudioDecoderTest.ResetBeforeInput/0',
          'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/*',
          'VideoDecoderTests/VideoDecoderTest'
          '.GetCurrentDecodeTargetBeforeWriteInputBuffer/*',
          'VideoDecoderTests/VideoDecoderTest.SingleInvalidInput/*',
          'VideoDecoderTests/VideoDecoderTest.EndOfStreamWithoutAnyInput/*',
          'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/*',
          'VideoDecoderTests/VideoDecoderTest.DecodeFullGOP/*',
      ],
  }
