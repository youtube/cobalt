#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib
import subprocess
import os
import sys
import re

# Set up path to be able to import action_helpers.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir, 'build'))
import action_helpers

# This script wraps rustc for (currently) these reasons:
# * To work around some ldflags escaping performed by ninja/gn
# * To remove dependencies on some environment variables from the .d file.
# * To enable use of .rsp files.
# * To work around two gn bugs on Windows
#
# LDFLAGS ESCAPING
#
# This script performs a simple function to work around some of the
# parameter escaping performed by ninja/gn.
#
# rustc invocations are given access to {{rustflags}} and {{ldflags}}.
# We want to pass {{ldflags}} into rustc, using -Clink-args="{{ldflags}}".
# Unfortunately, ninja assumes that each item in {{ldflags}} is an
# independent command-line argument and will have escaped them appropriately
# for use on a bare command line, instead of in a string.
#
# This script converts such {{ldflags}} into individual -Clink-arg=X
# arguments to rustc.
#
# RUSTENV dependency stripping
#
# When Rust code depends on an environment variable at build-time
# (using the env! macro), rustc spots that and adds it to the .d file.
# Ninja then parses that .d file and determines that the environment
# dependency means that the target always needs to be rebuilt.
#
# That's all correct, but _we_ know that some of these environment
# variables (typically, all of them) are set by .gn files which ninja
# tracks independently. So we remove them from the .d file.
#
# RSP files:
#
# We want to put the ninja/gn variables {{rustdeps}} and {{externs}}
# in an RSP file. Unfortunately, they are space-separated variables
# but Rust requires a newline-separated input. This script duly makes
# the adjustment. This works around a gn issue:
# TODO(https://bugs.chromium.org/p/gn/issues/detail?id=249): fix this
#
# WORKAROUND WINDOWS BUGS:
#
# On Windows platforms, this temporarily works around some issues in gn.
# See comments inline, linking to the relevant gn fixes.
#
# Usage:
#   rustc_wrapper.py --rustc <path to rustc> --depfile <path to .d file>
#      -- <normal rustc args> LDFLAGS {{ldflags}} RUSTENV {{rustenv}}
# The LDFLAGS token is discarded, and everything after that is converted
# to being a series of -Clink-arg=X arguments, until or unless RUSTENV
# is encountered, after which those are interpreted as environment
# variables to pass to rustc (and which will be removed from the .d file).
#
# Both LDFLAGS and RUSTENV **MUST** be specified, in that order, even if
# the list following them is empty.
#
# TODO(https://github.com/rust-lang/rust/issues/73632): avoid using rustc
# for linking in the first place. Most of our binaries are linked using
# clang directly, but there are some types of Rust build product which
# must currently be created by rustc (e.g. unit test executables). As
# part of support for using non-rustc linkers, we should arrange to extract
# such functionality from rustc so that we can make all types of binary
# using our clang toolchain. That will remove the need for most of this
# script.


# Equivalent of python3.9 built-in
def remove_lib_suffix_from_l_args(text):
  if text.startswith("-l") and text.endswith(".lib"):
    return text[:-len(".lib")]
  return text


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--rustc', required=True, type=pathlib.Path)
  parser.add_argument('--depfile', type=pathlib.Path)
  parser.add_argument('--rsp', type=pathlib.Path)
  parser.add_argument('args', metavar='ARG', nargs='+')

  args = parser.parse_args()

  remaining_args = args.args

  ldflags_separator = remaining_args.index("LDFLAGS")
  rustenv_separator = remaining_args.index("RUSTENV", ldflags_separator)
  rustc_args = remaining_args[:ldflags_separator]
  ldflags = remaining_args[ldflags_separator + 1:rustenv_separator]
  rustenv = remaining_args[rustenv_separator + 1:]

  is_windows = os.name == 'nt'

  rustc_args.extend(["-Clink-arg=%s" % arg for arg in ldflags])

  # Workaround for https://bugs.chromium.org/p/gn/issues/detail?id=249
  if args.rsp:
    with open(args.rsp) as rspfile:
      rsp_args = [l.rstrip() for l in rspfile.read().split(' ') if l.rstrip()]
    if is_windows:
      # Work around for hard-coded string in gn; full fix will come from
      # https://gn-review.googlesource.com/c/gn/+/12460
      rsp_args = [arg for arg in rsp_args if not arg.endswith("-Bdynamic")]
      # Work around for "-l<foo>.lib", where ".lib" suffix is undesirable.
      # Full fix will come from https://gn-review.googlesource.com/c/gn/+/12480
      rsp_args = [remove_lib_suffix_from_l_args(arg) for arg in rsp_args]
    with open(args.rsp, 'w') as rspfile:
      rspfile.write("\n".join(rsp_args))
    rustc_args.append(f'@{args.rsp}')

  env = os.environ.copy()
  fixed_env_vars = []
  for item in rustenv:
    (k, v) = item.split("=", 1)
    env[k] = v
    fixed_env_vars.append(k)

  r = subprocess.run([args.rustc, *rustc_args], env=env, check=False)
  if r.returncode != 0:
    sys.exit(r.returncode)

  # Now edit the depfile produced
  if args.depfile is not None:
    env_dep_re = re.compile("# env-dep:(.*)=.*")
    replacement_lines = []
    dirty = False
    with open(args.depfile, encoding="utf-8") as d:
      for line in d:
        m = env_dep_re.match(line)
        if m and m.group(1) in fixed_env_vars:
          dirty = True  # skip this line
        else:
          replacement_lines.append(line)
    if dirty:  # we made a change, let's write out the file
      with action_helpers.atomic_output(args.depfile) as output:
        output.write("\n".join(replacement_lines).encode("utf-8"))


if __name__ == '__main__':
  sys.exit(main())
