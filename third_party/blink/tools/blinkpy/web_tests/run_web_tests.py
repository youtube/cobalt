# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
# Copyright (C) 2011 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import optparse
import traceback

from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.web_tests.controllers.manager import Manager
from blinkpy.web_tests.models import test_run_results
from blinkpy.web_tests.port.factory import configuration_options
from blinkpy.web_tests.port.factory import platform_options
from blinkpy.web_tests.port.factory import wpt_options
from blinkpy.web_tests.port.factory import python_server_options
from blinkpy.web_tests.views import printing

_log = logging.getLogger(__name__)


def main(argv, stderr):
    options, args = parse_args(argv)

    if options.platform and 'test' in options.platform and not 'browser_test' in options.platform:
        # It's a bit lame to import mocks into real code, but this allows the user
        # to run tests against the test platform interactively, which is useful for
        # debugging test failures.
        from blinkpy.common.host_mock import MockHost
        host = MockHost()
    else:
        host = Host()

    if stderr.isatty():
        stderr.reconfigure(write_through=True)
    printer = printing.Printer(host, options, stderr)

    try:
        port = host.port_factory.get(options.platform, options)
    except (NotImplementedError, ValueError) as error:
        _log.error(error)
        printer.cleanup()
        return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

    try:
        return run(port, options, args, printer).exit_code

    # We need to still handle KeyboardInterrupt, at least for blinkpy unittest cases.
    except KeyboardInterrupt:
        return exit_codes.INTERRUPTED_EXIT_STATUS
    except test_run_results.TestRunException as error:
        _log.error(error.msg)
        return error.code
    except BaseException as error:
        if isinstance(error, Exception):
            _log.error('\n%s raised: %s', error.__class__.__name__, error)
            traceback.print_exc(file=stderr)
        return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS
    finally:
        printer.cleanup()


def deprecate(option, opt_str, _, parser):
    """
    Prints a error message for a deprecated option.
    Usage:
        optparse.make_option(
            '--some-option',
            action='callback',
            callback=deprecate,
            help='....')
    """
    parser.error('%s: %s' % (opt_str, option.help))


def parse_args(args):
    option_group_definitions = []

    option_group_definitions.append(('Platform options', platform_options()))

    option_group_definitions.append(('Configuration options',
                                     configuration_options()))

    option_group_definitions.append(('Printing Options',
                                     printing.print_options()))

    option_group_definitions.append(('web-platform-tests (WPT) Options',
                                     wpt_options()))

    option_group_definitions.append(('Python Server Options',
                                     python_server_options()))

    option_group_definitions.append((
        'Android-specific Options',
        [
            optparse.make_option(
                '--adb-device',
                action='append',
                default=[],
                dest='adb_devices',
                help='Run Android web tests on these devices'),
            # FIXME: Flip this to be off by default once we can log the
            # device setup more cleanly.
            optparse.make_option(
                '--no-android-logging',
                dest='android_logging',
                action='store_false',
                default=True,
                help=('Do not log android-specific debug messages (default '
                      'is to log as part of --debug-rwt-logging)')),
        ]))

    option_group_definitions.append(('Fuchsia-specific Options', [
        optparse.make_option(
            '--zircon-logging',
            dest='zircon_logging',
            action='store_true',
            default=True,
            help=('Log Zircon debug messages (enabled by default).')),
        optparse.make_option('--no-zircon-logging',
                             dest='zircon_logging',
                             action='store_false',
                             default=True,
                             help=('Do not log Zircon debug messages.')),
        optparse.make_option('--device',
                             choices=['qemu', 'device', 'fvdl'],
                             default='fvdl',
                             help=('Choose device to launch Fuchsia with. '
                                   'Defaults to fvdl.')),
        optparse.make_option('--fuchsia-target-cpu',
                             choices=['x64', 'arm64'],
                             default='x64',
                             help=('cpu architecture of the device. Defaults '
                                   'to x64.')),
        optparse.make_option('--fuchsia-out-dir',
                             help=('Path to Fuchsia build output directory.')),
        optparse.make_option('--custom-image',
                             help=('Specify an image used for booting up the '
                                   'emulator.')),
        optparse.make_option(
            '--fuchsia-ssh-config',
            help=('The path to the SSH configuration used for '
                  'connecting to the target device.')),
        optparse.make_option('--fuchsia-target-id',
                             help=('The node-name of the device to boot or '
                                   'deploy to.')),
        optparse.make_option(
            '--fuchsia-host-ip',
            help=('The IP address of the test host observed by the Fuchsia '
                  'device. Required if running on hardware devices.')),
        optparse.make_option('--logs-dir',
                             help='Location of diagnostics logs'),
    ]))

    option_group_definitions.append((
        'Results Options',
        [
            optparse.make_option(
                '--flag-specific',
                dest='flag_specific',
                action='store',
                default=None,
                help=
                ('Name of a flag-specific configuration defined in FlagSpecificConfig. '
                 'It is like a shortcut of --additional-driver-flag, '
                 '--additional-expectations and --additional-platform-directory. '
                 'When setting up flag-specific testing on bots, we should use '
                 'this option instead of the discrete options. '
                 'See crbug.com/1238155 for details.')),
            optparse.make_option(
                '--additional-driver-flag',
                '--additional-drt-flag',
                dest='additional_driver_flag',
                action='append',
                default=[],
                help=
                ('Additional command line flag to pass to the driver. Specify multiple '
                 'times to add multiple flags.')),
            optparse.make_option(
                '--additional-expectations',
                action='append',
                default=[],
                help=
                ('Path to a test_expectations file that will override previous '
                 'expectations. Specify multiple times for multiple sets of overrides.'
                 )),
            optparse.make_option(
                '--ignore-default-expectations',
                action='store_true',
                help=(
                    'Do not use the default set of TestExpectations files.')),
            optparse.make_option(
                '--no-expectations',
                action='store_true',
                help=(
                    'Do not use TestExpectations, only run the tests without '
                    'reporting any results. Useful for generating code '
                    'coverage reports.')),
            optparse.make_option(
                '--additional-platform-directory',
                action='append',
                default=[],
                help=
                ('Additional directory where to look for test baselines (will take '
                 'precedence over platform baselines). Specify multiple times to add '
                 'multiple search path entries.')),
            optparse.make_option(
                '--build-directory',
                default='out',
                help=
                ('Path to the directory where build files are kept, not including '
                 'configuration. In general this will be "out".')),
            optparse.make_option(
                '--clobber-old-results',
                action='store_true',
                default=False,
                help='Clobbers test results from previous runs.'),
            optparse.make_option(
                '--compare-port',
                action='store',
                default=None,
                help="Use the specified port's baselines first"),
            optparse.make_option(
                '--copy-baselines',
                action='store_true',
                default=False,
                help=
                ('If the actual result is different from the current baseline, '
                 'copy the current baseline into the *most-specific-platform* '
                 'directory, or the flag-specific generic-platform directory if '
                 '--additional-driver-flag is specified. See --reset-results.'
                 )),
            optparse.make_option('--driver-name',
                                 type='string',
                                 help='Alternative driver binary to use'),
            optparse.make_option(
                '--json-test-results',  # New name from json_results_generator
                '--write-full-results-to',  # Old argument name
                '--isolated-script-test-output',  # Isolated API
                help='Path to write the JSON test results for *all* tests.'),
            optparse.make_option(
                '--write-run-histories-to',
                help='Path to write the JSON test run histories.'),
            # FIXME(tansell): Remove this option if nobody is found who needs it.
            optparse.make_option(
                '--json-failing-test-results',
                help=
                'Path to write the JSON test results for only *failing* tests.'
            ),
            optparse.make_option(
                '--no-show-results',
                dest='show_results',
                action='store_false',
                default=True,
                help=
                "Don't launch a browser with results after the tests are done"
            ),
            optparse.make_option(
                '--reset-results',
                action='store_true',
                default=False,
                help=
                ('Reset baselines to the generated results in their existing location or the default '
                 'location if no baseline exists. For virtual tests, reset the virtual baselines. '
                 'If --additional-driver-flag is specified, reset the flag-specific baselines. '
                 'If --copy-baselines is specified, the copied baselines will be reset.'
                 )),
            optparse.make_option('--results-directory',
                                 help='Location of test results'),
            optparse.make_option('--smoke',
                                 action='store_true',
                                 help='Run just the SmokeTests'),
            optparse.make_option('--no-smoke',
                                 dest='smoke',
                                 action='store_false',
                                 help='Do not run just the SmokeTests'),
        ]))

    option_group_definitions.append((
        'Testing Options',
        [
            optparse.make_option(
                '--additional-env-var',
                type='string',
                action='append',
                default=[],
                help=('Passes that environment variable to the tests '
                      '(--additional-env-var=NAME=VALUE)')),
            optparse.make_option(
                '--build',
                dest='build',
                action='store_true',
                default=True,
                help=('Check to ensure the build is up to date (default).')),
            optparse.make_option(
                '--no-build',
                dest='build',
                action='store_false',
                help="Don't check to see if the build is up to date."),
            optparse.make_option('--wpt-only',
                                 action='store_true',
                                 default=False,
                                 help=('Run web platform tests only.')),
            optparse.make_option('--child-processes',
                                 '--jobs',
                                 '-j',
                                 help='Number of drivers to run in parallel.'),
            optparse.make_option(
                '--disable-breakpad',
                action='store_true',
                help="Don't use breakpad to symbolize unexpected crashes."),
            optparse.make_option(
                '--driver-logging',
                action='store_true',
                help='Print detailed logging of the driver/content_shell'),
            optparse.make_option(
                '--enable-leak-detection',
                action='store_true',
                help='Enable the leak detection of DOM objects.'),
            optparse.make_option(
                '--enable-sanitizer',
                action='store_true',
                help='Only alert on sanitizer-related errors and crashes'),
            optparse.make_option(
                '--enable-tracing',
                type='string',
                help='Capture and write a trace file with the specified '
                'categories for each test. Passes appropriate --trace-startup '
                'flags to the driver. If in doubt, use "*".'),
            optparse.make_option(
                '--exit-after-n-crashes-or-timeouts',
                type='int',
                default=None,
                help=
                'Exit after the first N crashes instead of running all tests'),
            optparse.make_option(
                '--exit-after-n-failures',
                type='int',
                default=None,
                help=
                'Exit after the first N failures instead of running all tests'
            ),
            optparse.make_option(
                '--fuzzy-diff',
                action='store_true',
                default=False,
                help=
                ('When running tests on an actual GPU, variance in pixel '
                 'output can leads image differences causing failed expectations. '
                 'Instead a fuzzy diff is used to account for this variance. '
                 'See tools/imagediff/image_diff.cc')),
            optparse.make_option(
                '--ignore-builder-category',
                action='store',
                help=
                ('The category of builders to use with the --ignore-flaky-tests option '
                 "('layout' or 'deps').")),
            optparse.make_option(
                '--ignore-flaky-tests',
                action='store',
                help=
                ('Control whether tests that are flaky on the bots get ignored. '
                 "'very-flaky' == Ignore any tests that flaked more than once on the bot. "
                 "'maybe-flaky' == Ignore any tests that flaked once on the bot. "
                 "'unexpected' == Ignore any tests that had unexpected results on the bot."
                 )),
            optparse.make_option(
                '--iterations',
                '--isolated-script-test-repeat',
                # TODO(crbug.com/893235): Remove the gtest alias when FindIt no longer uses it.
                '--gtest_repeat',
                type='int',
                default=1,
                help='Number of times to run the set of tests (e.g. ABCABCABC)'
            ),
            optparse.make_option(
                '--layout-tests-directory',
                help=('Path to a custom web tests directory')),
            optparse.make_option(
                '--max-locked-shards',
                type='int',
                default=0,
                help='Set the maximum number of locked shards'),
            optparse.make_option(
                '--nocheck-sys-deps',
                action='store_true',
                default=False,
                help="Don't check the system dependencies (themes)"),
            optparse.make_option(
                '--order',
                action='store',
                default='random',
                help=
                ('Determine the order in which the test cases will be run. '
                 "'none' == use the order in which the tests were listed "
                 'either in arguments or test list, '
                 "'random' == pseudo-random order (default). Seed can be specified "
                 'via --seed, otherwise it will default to the current unix timestamp. '
                 "'natural' == use the natural order")),
            optparse.make_option('--profile',
                                 action='store_true',
                                 help='Output per-test profile information.'),
            optparse.make_option(
                '--profiler',
                action='store',
                help=
                'Output per-test profile information, using the specified profiler.'
            ),
            optparse.make_option(
                '--restart-shell-between-tests',
                type='choice',
                action='store',
                choices=[
                    'always',
                    'never',
                    'on_retry',
                ],
                default='on_retry',
                help=
                ('Restarting the shell between tests produces more '
                 'consistent results, as it prevents state from carrying over '
                 'from previous tests. It also increases test run time by at '
                 'least 2X. By default, the shell is restarted when tests get '
                 'retried, since leaking state between retries can sometimes '
                 'mask underlying flakiness, and the whole point of retries is '
                 'to look for flakiness.')),
            optparse.make_option(
                '--repeat-each',
                type='int',
                default=1,
                help='Number of times to run each test (e.g. AAABBBCCC)'),
            optparse.make_option(
                '--num-retries',
                '--test-launcher-retry-limit',
                '--isolated-script-test-launcher-retry-limit',
                type='int',
                default=None,
                help=('Number of times to retry failures. Default (when this '
                      'flag is not specified) is to retry 3 times, unless an '
                      'explicit list of tests is passed to run_web_tests.py. '
                      'If a non-zero value is given explicitly, failures are '
                      'retried regardless.')),
            optparse.make_option(
                '--no-retry-failures',
                dest='num_retries',
                action='store_const',
                const=0,
                help="Don't retry any failures (equivalent to --num-retries=0)."
            ),
            optparse.make_option(
                '--total-shards',
                type=int,
                help=('Total number of shards being used for this test run. '
                      'Must be used with --shard-index. '
                      '(The user of this script is responsible for spawning '
                      'all of the shards.)')),
            optparse.make_option(
                '--shard-index',
                type=int,
                help=('Shard index [0..total_shards) of this test run. '
                      'Must be used with --total-shards.')),
            optparse.make_option(
                '--seed',
                type='int',
                help=('Seed to use for random test order (default: %default). '
                      'Only applicable in combination with --order=random.')),
            optparse.make_option(
                '--skipped',
                action='store',
                default=None,
                help=
                ('Control how tests marked SKIP are run. '
                 '"default" == Skip tests unless explicitly listed on the command line, '
                 '"ignore" == Run them anyway, '
                 '"only" == only run the SKIP tests, '
                 '"always" == always skip, even if listed on the command line.'
                 )),
            optparse.make_option(
                '--isolated-script-test-also-run-disabled-tests',
                # TODO(crbug.com/893235): Remove the gtest alias when FindIt no longer uses it.
                '--gtest_also_run_disabled_tests',
                action='store_const',
                const='ignore',
                dest='skipped',
                help=('Equivalent to --skipped=ignore.')),
            optparse.make_option(
                '--skip-failing-tests',
                action='store_true',
                default=False,
                help=
                ('Skip tests that are expected to fail. Note: When using this option, '
                 'you might miss new crashes in these tests.')),
            optparse.make_option(
                '--skip-timeouts',
                action='store_true',
                default=False,
                help=
                ('Skip tests marked TIMEOUT. Use it to speed up running the entire '
                 'test suite.')),
            optparse.make_option(
                '--fastest',
                action='store',
                type='float',
                help=
                'Run the N% fastest tests as well as any tests listed on the command line'
            ),
            optparse.make_option('--test-list',
                                 action='append',
                                 metavar='FILE',
                                 help='read filters for tests to run'),
            optparse.make_option(
                '--isolated-script-test-filter-file',
                '--test-launcher-filter-file',
                action='append',
                metavar='FILE',
                help=
                'read filters for tests to not run as if they were specified on the command line'
            ),
            optparse.make_option(
                '--isolated-script-test-filter',
                action='append',
                type='string',
                help=
                'A list of test globs to run or skip, separated by TWO colons, e.g. fast::css/test.html; '
                'prefix the glob with "-" to skip it'),
            # TODO(crbug.com/893235): Remove gtest_filter when FindIt no longer uses it.
            optparse.make_option(
                '--gtest_filter',
                type='string',
                help='A colon-separated list of tests to run. Wildcards are '
                'NOT supported. It is the same as listing the tests as '
                'positional arguments.'),
            optparse.make_option('--timeout-ms',
                                 help='Set the timeout for each test'),
            optparse.make_option(
                '--initialize-webgpu-adapter-at-startup-timeout-ms',
                type='float',
                help='Initialize WebGPU adapter before running any tests.'),
            optparse.make_option(
                '--wrapper',
                help=
                ('wrapper command to insert before invocations of the driver; option '
                 'is split on whitespace before running. (Example: --wrapper="valgrind '
                 '--smc-check=all")')),
            # FIXME: Display the default number of child processes that will run.
            optparse.make_option('-f',
                                 '--fully-parallel',
                                 action='store_true',
                                 help='run all tests in parallel'),
            optparse.make_option(
                '--virtual-parallel',
                action='store_true',
                help=
                'When running in parallel, include virtual tests. Useful for running a single '
                'virtual test suite, but will be slower in other cases.'),
            optparse.make_option(
                '-i',
                '--ignore-tests',
                action='append',
                default=[],
                help=
                'directories or test to ignore (may specify multiple times)'),
            optparse.make_option(
                '-n',
                '--dry-run',
                action='store_true',
                default=False,
                help=
                'Do everything but actually run the tests or upload results.'),
            optparse.make_option(
                '-w',
                '--watch',
                action='store_true',
                help='Re-run tests quickly (e.g. avoid restarting the server)'
            ),
            optparse.make_option(
                '--zero-tests-executed-ok',
                action='store_true',
                help='If set, exit with a success code when no tests are run.'
                ' Used on trybots when web tests are retried without patch.'),
            optparse.make_option(
                '--driver-kill-timeout-secs',
                type=float,
                default=1.0,
                help=
                ('Number of seconds to wait before killing a driver, and the main '
                 'use case is to leave enough time to allow the process to '
                 'finish post-run hooks, such as dumping code coverage data. '
                 'Default is 1 second, can be overriden for specific use cases.'
                 )),
            optparse.make_option(
                '--git-revision',
                help=(
                    'The Chromium git revision being tested. This is only used '
                    'for an experimental Skia Gold dryrun.')),
            optparse.make_option(
                '--gerrit-issue',
                help=(
                    'The Gerrit issue/CL number being tested, if applicable. '
                    'This is only used for an experimental Skia Gold dryrun.'
                )),
            optparse.make_option(
                '--gerrit-patchset',
                help=(
                    'The Gerrit patchset being tested, if applicable. This is '
                    'only used for an experimental Skia Gold dryrun.')),
            optparse.make_option(
                '--buildbucket-id',
                help=(
                    'The Buildbucket ID of the bot running the test. This is '
                    'only used for an experimental Skia Gold dryrun.')),
            optparse.make_option(
                '--ignore-testharness-expected-txt',
                action='store_true',
                help=('Ignore *-expected.txt for all testharness tests. All '
                      'testharness test failures will be shown, even if the '
                      'failures are expected in *-expected.txt.')),
        ]))

    # FIXME: Move these into json_results_generator.py.
    option_group_definitions.append((
        'Result JSON Options',
        [
            # TODO(qyearsley): --build-name is unused and should be removed.
            optparse.make_option('--build-name', help=optparse.SUPPRESS_HELP),
            optparse.make_option(
                '--step-name',
                default='blink_web_tests',
                help='The name of the step in a build running this script.'),
            optparse.make_option(
                '--build-number',
                default='DUMMY_BUILD_NUMBER',
                help='The build number of the builder running this script.'),
            optparse.make_option(
                '--builder-name',
                default='',
                help='The name of the builder shown on the waterfall running '
                'this script, e.g. "Mac10.13 Tests".'),
        ]))

    option_parser = optparse.OptionParser(
        prog='run_web_tests.py',
        usage='%prog [options] [tests]',
        description=
        'Runs Blink web tests as described in docs/testing/web_tests.md')

    for group_name, group_options in option_group_definitions:
        option_group = optparse.OptionGroup(option_parser, group_name)
        option_group.add_options(group_options)
        option_parser.add_option_group(option_group)

    (options, args) = option_parser.parse_args(args)

    return (options, args)


def _set_up_derived_options(port, options, args):
    """Sets the options values that depend on other options values."""
    # --restart-shell-between-tests is implemented by changing the batch size.
    if options.restart_shell_between_tests == 'always':
        options.derived_batch_size = 1
        options.must_use_derived_batch_size = True
    elif options.restart_shell_between_tests == 'never':
        options.derived_batch_size = 0
        options.must_use_derived_batch_size = True
    else:
        # If 'repeat_each' or 'iterations' has been set, then implicitly set the
        # batch size to 1. If we're already repeating the tests more than once,
        # then we're not particularly concerned with speed. Restarting content
        # shell provides more consistent results.
        if options.repeat_each > 1 or options.iterations > 1:
            options.derived_batch_size = 1
            options.must_use_derived_batch_size = True
        else:
            options.derived_batch_size = port.default_batch_size()
            options.must_use_derived_batch_size = False

    if not options.child_processes:
        options.child_processes = port.host.environ.get(
            'WEBKIT_TEST_CHILD_PROCESSES', str(port.default_child_processes()))
    if not options.max_locked_shards:
        options.max_locked_shards = int(
            port.host.environ.get('WEBKIT_TEST_MAX_LOCKED_SHARDS',
                                  str(port.default_max_locked_shards())))

    if not options.configuration:
        options.configuration = port.get_option('configuration')

    if not options.target:
        options.target = port.get_option('target')

    if not options.timeout_ms:
        options.timeout_ms = str(port.timeout_ms())

    options.slow_timeout_ms = str(5 * int(options.timeout_ms))

    if options.additional_platform_directory:
        additional_platform_directories = []
        for path in options.additional_platform_directory:
            additional_platform_directories.append(
                port.host.filesystem.abspath(path))
        options.additional_platform_directory = additional_platform_directories

    if not args and not options.test_list and options.smoke is None:
        options.smoke = port.default_smoke_test_only()
    if options.smoke:
        if not args and not options.test_list and options.num_retries is None:
            # Retry failures 3 times if we're running a smoke test without
            # additional tests. SmokeTests is an explicit list of tests, so we
            # wouldn't retry by default without this special case.
            options.num_retries = 3

        if not options.test_list:
            options.test_list = []
        options.test_list.append(port.path_to_smoke_tests_file())
        if not options.skipped:
            options.skipped = 'always'

    if not options.skipped:
        options.skipped = 'default'

    if options.gtest_filter:
        args.extend(options.gtest_filter.split(':'))

    if not options.total_shards and 'GTEST_TOTAL_SHARDS' in port.host.environ:
        options.total_shards = int(port.host.environ['GTEST_TOTAL_SHARDS'])
    if not options.shard_index and 'GTEST_SHARD_INDEX' in port.host.environ:
        options.shard_index = int(port.host.environ['GTEST_SHARD_INDEX'])

    if not options.seed:
        options.seed = port.host.time()


def run(port, options, args, printer):
    _set_up_derived_options(port, options, args)
    manager = Manager(port, options, printer)
    printer.print_config(port)
    run_details = manager.run(args)
    _log.debug('')
    _log.debug('Testing completed. Exit status: %d', run_details.exit_code)
    printer.flush()
    return run_details
