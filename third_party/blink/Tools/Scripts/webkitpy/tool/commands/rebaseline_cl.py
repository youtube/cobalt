# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command to fetch new baselines from try jobs for the current CL."""

import json
import logging
import optparse

from webkitpy.common.net.git_cl import GitCL
from webkitpy.layout_tests.models.test_expectations import BASELINE_SUFFIX_LIST
from webkitpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand
from webkitpy.w3c.wpt_manifest import WPTManifest

_log = logging.getLogger(__name__)


class RebaselineCL(AbstractParallelRebaselineCommand):
    name = 'rebaseline-cl'
    help_text = 'Fetches new baselines for a CL from test runs on try bots.'
    long_help = ('By default, this command will check the latest try job results '
                 'for all platforms, and start try jobs for platforms with no '
                 'try jobs. Then, new baselines are downloaded for any tests '
                 'that are being rebaselined. After downloading, the baselines '
                 'for different platforms will be optimized (consolidated).')
    show_in_main_help = True

    def __init__(self):
        super(RebaselineCL, self).__init__(options=[
            optparse.make_option(
                '--dry-run', action='store_true', default=False,
                help='Dry run mode; list actions that would be performed but do not do anything.'),
            optparse.make_option(
                '--only-changed-tests', action='store_true', default=False,
                help='Only download new baselines for tests that are changed in the CL.'),
            optparse.make_option(
                '--no-trigger-jobs', dest='trigger_jobs', action='store_false', default=True,
                help='Do not trigger any try jobs.'),
            self.no_optimize_option,
            self.results_directory_option,
        ])

    def execute(self, options, args, tool):
        self._tool = tool

        # TODO(qyearsley): Move this call to somewhere else.
        WPTManifest.ensure_manifest(tool)

        unstaged_baselines = self.unstaged_baselines()
        if unstaged_baselines:
            _log.error('Aborting: there are unstaged baselines:')
            for path in unstaged_baselines:
                _log.error('  %s', path)
            return 1

        issue_number = self._get_issue_number()
        if issue_number is None:
            _log.error('No issue number for current branch.')
            return 1
        _log.debug('Issue number for current branch: %s', issue_number)

        builds = self.git_cl().latest_try_jobs(self._try_bots())

        builders_with_pending_builds = self.builders_with_pending_builds(builds)
        if builders_with_pending_builds:
            _log.info('There are existing pending builds for:')
            for builder in sorted(builders_with_pending_builds):
                _log.info('  %s', builder)
        builders_with_no_results = self.builders_with_no_results(builds)

        if options.trigger_jobs and builders_with_no_results:
            self.trigger_builds(builders_with_no_results)
            _log.info('Please re-run webkit-patch rebaseline-cl once all pending try jobs have finished.')
            return 1

        if builders_with_no_results:
            # TODO(qyearsley): Support trying to continue as long as there are
            # some results from some builder; see http://crbug.com/673966.
            _log.error('The following builders have no results:')
            for builder in builders_with_no_results:
                _log.error('  %s', builder)
            return 1

        _log.debug('Getting results for issue %d.', issue_number)
        builds_to_results = self._fetch_results(builds)
        if builds_to_results is None:
            return 1

        test_prefix_list = {}
        if args:
            for test in args:
                test_prefix_list[test] = {b: BASELINE_SUFFIX_LIST for b in builds}
        else:
            test_prefix_list = self._test_prefix_list(
                builds_to_results,
                only_changed_tests=options.only_changed_tests)

        self._log_test_prefix_list(test_prefix_list)

        if not options.dry_run:
            self.rebaseline(options, test_prefix_list)
        return 0

    def _get_issue_number(self):
        """Returns the current CL issue number, or None."""
        issue = self.git_cl().get_issue_number()
        if not issue.isdigit():
            return None
        return int(issue)

    def git_cl(self):
        """Returns a GitCL instance. Can be overridden for tests."""
        return GitCL(self._tool)

    def trigger_builds(self, builders):
        _log.info('Triggering try jobs for:')
        for builder in sorted(builders):
            _log.info('  %s', builder)
        self.git_cl().trigger_try_jobs(builders)

    def builders_with_no_results(self, builds):
        """Returns the set of builders that don't have finished results."""
        builders_with_no_builds = set(self._try_bots()) - {b.builder_name for b in builds}
        return builders_with_no_builds | self.builders_with_pending_builds(builds)

    def builders_with_pending_builds(self, builds):
        """Returns the set of builders that have pending builds."""
        return {b.builder_name for b in builds if b.build_number is None}

    def _try_bots(self):
        """Returns a collection of try bot builders to fetch results for."""
        return self._tool.builders.all_try_builder_names()

    def _fetch_results(self, builds):
        """Fetches results for all of the given builds.

        There should be a one-to-one correspondence between Builds, supported
        platforms, and try bots. If not all of the builds can be fetched, then
        continuing with rebaselining may yield incorrect results, when the new
        baselines are deduped, an old baseline may be kept for the platform
        that's missing results.

        Args:
            builds: A list of Build objects.

        Returns:
            A dict mapping Build to LayoutTestResults, or None if any results
            were not available.
        """
        buildbot = self._tool.buildbot
        results = {}
        for build in builds:
            results_url = buildbot.results_url(build.builder_name, build.build_number)
            layout_test_results = buildbot.fetch_results(build)
            if layout_test_results is None:
                _log.error(
                    'Failed to fetch results from "%s".\n'
                    'Try starting a new job for %s by running :\n'
                    '  git cl try -b %s',
                    results_url, build.builder_name, build.builder_name)
                return None
            results[build] = layout_test_results
        return results

    def _test_prefix_list(self, builds_to_results, only_changed_tests):
        """Returns a dict which lists the set of baselines to fetch.

        The dict that is returned is a dict of tests to Build objects
        to baseline file extensions.

        Args:
            builds_to_results: A dict mapping Builds to LayoutTestResults.
            only_changed_tests: Whether to only include baselines for tests that
               are changed in this CL. If False, all new baselines for failing
               tests will be downloaded, even for tests that were not modified.

        Returns:
            A dict containing information about which new baselines to download.
        """
        builds_to_tests = {}
        for build, results in builds_to_results.iteritems():
            builds_to_tests[build] = self._tests_to_rebaseline(build, results)
        if only_changed_tests:
            files_in_cl = self._tool.git().changed_files(diff_filter='AM')
            # In the changed files list from Git, paths always use "/" as
            # the path separator, and they're always relative to repo root.
            # TODO(qyearsley): Do this without using a hard-coded constant.
            test_base = 'third_party/WebKit/LayoutTests/'
            tests_in_cl = [f[len(test_base):] for f in files_in_cl if f.startswith(test_base)]
        result = {}
        for build, tests in builds_to_tests.iteritems():
            for test in tests:
                if only_changed_tests and test not in tests_in_cl:
                    continue
                if test not in result:
                    result[test] = {}
                result[test][build] = BASELINE_SUFFIX_LIST
        return result

    def _tests_to_rebaseline(self, build, layout_test_results):
        """Fetches a list of tests that should be rebaselined for some build."""
        unexpected_results = layout_test_results.didnt_run_as_expected_results()
        tests = sorted(r.test_name() for r in unexpected_results
                       if r.is_missing_baseline() or r.has_mismatch_result())

        new_failures = self._fetch_tests_with_new_failures(build)
        if new_failures is None:
            _log.warning('No retry summary available for build %s.', build)
        else:
            tests = [t for t in tests if t in new_failures]
        return tests

    def _fetch_tests_with_new_failures(self, build):
        """For a given try job, lists tests that only occurred with the patch."""
        buildbot = self._tool.buildbot
        content = buildbot.fetch_retry_summary_json(build)
        if content is None:
            return None
        try:
            retry_summary = json.loads(content)
            return retry_summary['failures']
        except (ValueError, KeyError):
            _log.warning('Unexpected retry summary content:\n%s', content)
            return None

    @staticmethod
    def _log_test_prefix_list(test_prefix_list):
        """Logs the tests to download new baselines for."""
        if not test_prefix_list:
            _log.info('No tests to rebaseline; exiting.')
            return
        _log.debug('Tests to rebaseline:')
        for test, builds in test_prefix_list.iteritems():
            _log.debug('  %s:', test)
            for build in sorted(builds):
                _log.debug('    %s', build)
