#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Developers invoke this script via autosiso or autosiso.bat to simply run
Siso/Reclient builds.
"""
# TODO(b/278976196): `siso ninja` command should handle the reclient and
# authentication accordingly.

import os
import re
import sys

import reclient_helper
import siso


def _use_remoteexec(argv):
  out_dir = reclient_helper.find_ninja_out_dir(argv)
  gn_args_path = os.path.join(out_dir, 'args.gn')
  if not os.path.exists(gn_args_path):
    return False
  with open(gn_args_path) as f:
    for line in f:
      line_without_comment = line.split('#')[0]
      if re.search(r'(^|\s)use_remoteexec\s*=\s*true($|\s)',
                   line_without_comment):
        return True
  return False


def main(argv):
  if not _use_remoteexec(argv):
    print(
        "`use_remoteexec=true` is not detected.\n"
        "Please run `siso` command directly.",
        file=sys.stderr)
    return 1

  with reclient_helper.build_context(argv) as ret_code:
    if ret_code:
      return ret_code
    argv = [
        argv[0],
        'ninja',
        # Do not authenticate when using Reproxy.
        '-project=',
        '-reapi_instance=',
    ] + argv[1:]
    return siso.main(argv)


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv))
  except KeyboardInterrupt:
    sys.exit(1)
