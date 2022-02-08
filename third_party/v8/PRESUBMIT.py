"""Top-level presubmit script for Cobalt."""

import imp

# TODO(uzhilinsky): Temporarily use Steel's PRESUBMIT checks until Cobalt can
# transition to use the canned checks from depot_tools.
steel_presubmit_checks = imp.load_source(
    'steel_presubmit_checks', '../tools/repo-hooks/steel_presubmit_checks.py')

CPPLINT_FILTERS_PRIVATE = [
    # We don't require copyright messages in platform specific source files
    '-legal/copyright',
]

CPPLINT_FILTERS_PUBLIC = []

CPPLINT_EXCLUDE = (
)

_LICENSE_FILE = (
    r"LICENSE"
)

def CheckChangeOnUpload(input_api, output_api):
  """Upload hook, called by depot_tools presubmit_support.

  This hook is used to check code before it's uploaded to GoB for review.

  Args:
    input_api: presubmit_support input API
    output_api: presubmit_support output API

  Returns:
    List of failed test results. Empty if all tests were successful.
  """
  output = []
  cpplint_options = {
      'exclude': CPPLINT_EXCLUDE,
      'public_filters': CPPLINT_FILTERS_PUBLIC,
      'private_filters': CPPLINT_FILTERS_PRIVATE,
      'header_guard_use_repo_name': False,
  }
  output.extend(steel_presubmit_checks.RunCobaltChecks(
      input_api, output_api, cpplint_options))
  output.extend(steel_presubmit_checks.RunPyTests(input_api, output_api))

  return output


def CheckChangeOnCommit(input_api, output_api):
  """Commit hook, called by depot_tools presubmit_support.

  This hook is used to check code that went through a review and is now
  ready to be committed to the main repository. This hook is not used by Steel
  because our code is committed from GoB via the web interface.

  Args:
    input_api: presubmit_support input API
    output_api: presubmit_support output API

  Returns:
    List of failed test results. Empty if all tests were successful.
  """
  # Currently not used
  _ = input_api, output_api
  return []
