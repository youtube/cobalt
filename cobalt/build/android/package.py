#!/usr/bin/env python3
"""Packages Cobalt for Android."""

# pylint: disable=line-too-long

import os
import shutil
from cobalt.build import packaging


def lay_out(out_dir, base_dir):
  directories = [
      'content/data',
      'gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/androidx/appcompat',
      'gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/androidx/core',
      'gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/androidx/fragment',
      'gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/gen/base_module',
      'gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/org/chromium',
      'lib.java',
  ]

  files = [
      'content_shell.pak',
      'icudtl.dat',
      'libchrome_crashpad_handler.so',
      'libcobalt_content_shell_content_view.so',
      'libnplb__library.so',
      'libstarboard_jni_state.so',
      'libtest_trace_processor.so',
      'lib.unstripped/libcobalt_content_shell_content_view.so',
      'obj/cobalt/android/cobalt_java_resources.resources.zip',
      'obj/components/metrics/metrics_java.javac.jar',
      'obj/content/shell/android/content_shell_java_resources.resources.zip',
      'obj/third_party/android_deps/chromium_play_services_availability_java.javac.jar',
  ]

  for directory in directories:
    shutil.copytree(
        os.path.join(out_dir, directory),
        os.path.join(base_dir, directory),
        dirs_exist_ok=True)

  for file in files:
    packaging.copy(os.path.join(out_dir, file), os.path.join(base_dir, file))


if __name__ == '__main__':
  packaging.run(lay_out)
