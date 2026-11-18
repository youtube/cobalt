#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shutil
import subprocess
import sys


def install(output_prefix, deps_prefix):
  # Use unbuffered output for logging
  sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', buffering=1)

  env = os.environ.copy()
  env['USE_BAZEL_VERSION'] = env.get('_3PP_VERSION', '3.7.2')

  # Set up JAVA_HOME and PATH for Bazel to find the JDK.
  jdk_home = os.path.join(deps_prefix, 'current')
  env['JAVA_HOME'] = jdk_home
  env['PATH'] = '%s:%s' % (os.path.join(jdk_home, 'bin'), env.get('PATH', ''))

  # Try to inject Devtoolset library path and nonshared library into LDFLAGS
  devtoolset_lib_dir = '/opt/rh/devtoolset-10/root/usr/lib/gcc/x86_64-redhat-linux/10'
  env['LDFLAGS'] = env.get('LDFLAGS', '') + f' -L{devtoolset_lib_dir} -lstdc++_nonshared'

  print("Environment variables:")
  for k, v in sorted(env.items()):
    print(f"  {k}={v}")

  cc = env.get('CC', 'gcc')
  print(f"Using CC: {cc}")

  subprocess.run([
      'bazelisk', 'build',
      f'--linkopt=-L{devtoolset_lib_dir}',
      '--linkopt=-lstdc++_nonshared',
      f'--host_linkopt=-L{devtoolset_lib_dir}',
      '--host_linkopt=-lstdc++_nonshared',
      '//src/java_tools/buildjar/java/com/google/devtools/build/buildjar:BazelJavaBuilder_deploy.jar'
  ],
                 env=env,
                 check=True)

  os.makedirs(output_prefix, exist_ok=True)
  jar_path = os.path.join(
      'bazel-bin', 'src', 'java_tools', 'buildjar', 'java', 'com', 'google',
      'devtools', 'build', 'buildjar', 'BazelJavaBuilder_deploy.jar')
  shutil.copy(jar_path, output_prefix)


if __name__ == '__main__':
  install(sys.argv[1], sys.argv[2])
