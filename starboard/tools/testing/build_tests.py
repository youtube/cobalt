import logging
import os
import subprocess

def BuildTargets(targets, out_directory, dry_run=False, extra_build_flags=[]):
  """Builds all specified targets.

  Args:
    extra_build_flags: Additional command line flags to pass to ninja.
  """
  if not targets:
    logging.info('No targets specified, nothing to build.')
    return

  args_list = ["ninja", "-C", out_directory]
  if dry_run:
    args_list.append("-n")

  args_list.extend(["{}_deploy".format(test_name) for test_name in targets])
  args_list.extend(extra_build_flags)

  if "TEST_RUNNER_BUILD_FLAGS" in os.environ:
    args_list.append(os.environ["TEST_RUNNER_BUILD_FLAGS"])

  logging.info('Building targets with command: %s', str(args_list))

  try:
    # We set shell=True because otherwise Windows doesn't recognize
    # PATH properly.
    #   https://bugs.python.org/issue15451
    # We flatten the arguments to a string because with shell=True, Linux
    # doesn't parse them properly.
    #   https://bugs.python.org/issue6689
    return subprocess.check_call(" ".join(args_list), shell=True)
  except KeyboardInterrupt:
    return 1
