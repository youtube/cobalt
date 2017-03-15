# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An interface to git-cl.

The git-cl tool is responsible for communicating with Rietveld, Gerrit,
and Buildbucket to manage changelists and try jobs associated with them.
"""

import json
import logging
import re

from webkitpy.common.net.buildbot import Build, filter_latest_builds

_log = logging.getLogger(__name__)

# A refresh token may be needed for some commands, such as git cl try,
# in order to authenticate with buildbucket.
_COMMANDS_THAT_TAKE_REFRESH_TOKEN = ('try',)


class GitCL(object):

    def __init__(self, host, auth_refresh_token_json=None, cwd=None):
        self._host = host
        self._auth_refresh_token_json = auth_refresh_token_json
        self._cwd = cwd

    def run(self, args):
        """Runs git-cl with the given arguments and returns the output."""
        command = ['git', 'cl'] + args
        if self._auth_refresh_token_json and args[0] in _COMMANDS_THAT_TAKE_REFRESH_TOKEN:
            command += ['--auth-refresh-token-json', self._auth_refresh_token_json]
        return self._host.executive.run_command(command, cwd=self._cwd)

    def trigger_try_jobs(self, builders=None):
        builders = builders or self._host.builders.all_try_builder_names()
        if 'android_blink_rel' in builders:
            self.run(['try', '-b', 'android_blink_rel'])
            builders.remove('android_blink_rel')
        # TODO(qyearsley): Stop explicitly adding the master name when
        # git cl try can get the master name; see http://crbug.com/700523.
        command = ['try', '-m', 'tryserver.blink']
        for builder in sorted(builders):
            command.extend(['-b', builder])
        self.run(command)

    def get_issue_number(self):
        return self.run(['issue']).split()[2]

    def wait_for_try_jobs(self, poll_delay_seconds=10 * 60, timeout_seconds=120 * 60):
        """Waits until all try jobs are finished.

        Args:
            poll_delay_seconds: Time to wait between fetching results.
            timeout_seconds: Time to wait before aborting.

        Returns:
            A list of try job result dicts, or None if a timeout occurred.
        """
        start = self._host.time()
        self._host.print_('Waiting for try jobs (timeout: %d seconds).' % timeout_seconds)
        while self._host.time() - start < timeout_seconds:
            self._host.sleep(poll_delay_seconds)
            try_results = self.fetch_try_results()
            _log.debug('Fetched try results: %s', try_results)
            if self.all_jobs_finished(try_results):
                self._host.print_('All jobs finished.')
                return try_results
            self._host.print_('Waiting. %d seconds passed.' % (self._host.time() - start))
            self._host.sleep(poll_delay_seconds)
        self._host.print_('Timed out waiting for try results.')
        return None

    def latest_try_jobs(self, builder_names=None):
        """Returns a list of Builds for the latest jobs for the given builders."""
        try_results = self.fetch_try_results()
        if builder_names:
            try_results = [r for r in try_results if r['builder_name'] in builder_names]
        return filter_latest_builds(self._try_result_to_build(r) for r in try_results)

    def fetch_try_results(self):
        """Requests results f try jobs for the current CL."""
        with self._host.filesystem.mkdtemp() as temp_directory:
            results_path = self._host.filesystem.join(temp_directory, 'try-results.json')
            self.run(['try-results', '--json', results_path])
            contents = self._host.filesystem.read_text_file(results_path)
            _log.debug('Fetched try results to file "%s".', results_path)
            self._host.filesystem.remove(results_path)
        return json.loads(contents)

    @staticmethod
    def _try_result_to_build(try_result):
        """Converts a parsed try result dict to a Build object."""
        builder_name = try_result['builder_name']
        url = try_result['url']
        if url is None:
            return Build(builder_name, None)
        match = re.match(r'.*/builds/(\d+)?$', url)
        build_number = match.group(1)
        assert build_number and build_number.isdigit()
        return Build(builder_name, int(build_number))

    @staticmethod
    def all_jobs_finished(try_results):
        return all(r.get('status') == 'COMPLETED' for r in try_results)

    @staticmethod
    def has_failing_try_results(try_results):
        return any(r.get('result') == 'FAILURE' for r in try_results)
