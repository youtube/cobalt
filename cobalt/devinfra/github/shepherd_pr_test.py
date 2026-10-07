#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Black-box integration test suite for shepherd_pr.py."""

# pylint: disable=bad-indentation,inconsistent-quotes,line-too-long,consider-using-with,broad-exception-caught,unused-import

import json
import os
import subprocess
import sys
import tempfile
import unittest

_scripts_dir = os.path.dirname(os.path.abspath(__file__))
_SCRIPT_PATH = os.path.join(_scripts_dir, 'shepherd_pr.py')


class TestPRShepherdCLI(unittest.TestCase):
    """Black-box command-line integration test suite for shepherd_pr.py."""

    def setUp(self):
        super().setUp()
        self.test_dir = tempfile.TemporaryDirectory()
        self.bin_dir = os.path.join(self.test_dir.name, 'bin')
        self.mock_gh_script = os.path.join(self.bin_dir, 'gh')
        os.makedirs(self.bin_dir, exist_ok=True)

        # Create mock gh executable that executes python code from MOCK_GH_HANDLER
        gh_content = (
            '#!/usr/bin/env python3\n'
            'import json, os, sys\n'
            'handler_path = os.environ.get("MOCK_GH_HANDLER")\n'
            'if not handler_path or not os.path.exists(handler_path):\n'
            '  sys.stderr.write("MOCK_GH_HANDLER not found\\n")\n'
            '  sys.exit(1)\n'
            'with open(handler_path, "r", encoding="utf-8") as f:\n'
            '  code = f.read()\n'
            'scope = {"argv": sys.argv[1:], "os": os, "sys": sys, "json": json}\n'
            'exec(code, scope)\n')
        with open(self.mock_gh_script, 'w', encoding='utf-8') as f:
            f.write(gh_content)
        os.chmod(self.mock_gh_script, 0o755)

    def tearDown(self):
        self.test_dir.cleanup()
        super().tearDown()

    def run_cli(self, args, handler_code):
        """Executes shepherd_pr.py via CLI with custom mock gh handler."""
        handler_path = os.path.join(self.test_dir.name, 'handler.py')
        with open(handler_path, 'w', encoding='utf-8') as f:
            f.write(handler_code)

        env = os.environ.copy()
        env['PATH'] = f'{self.bin_dir}:{env.get("PATH", "")}'
        env['MOCK_GH_HANDLER'] = handler_path

        cmd = [sys.executable, _SCRIPT_PATH] + args
        return subprocess.run(cmd,
                              capture_output=True,
                              text=True,
                              env=env,
                              cwd=self.test_dir.name,
                              check=False)

    def test_cli_single_run_green(self):
        """Verifies CLI single run execution outputs green status."""
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 101,
                  'title': 'Feature A',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/101',
                  'headRefOid': 'sha_green_101',
                  'headRefName': 'feat-a',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'CLEAN',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'build',
                              'status': 'COMPLETED',
                              'conclusion': 'SUCCESS',
                              'detailsUrl': 'https://url/build'
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)
sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            ['--repo', 'org/repo', '--pr', '101'],
            handler,
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        self.assertIn('[PR #101] Tracking started.', res.stdout)
        self.assertIn('SUPER GREEN! Multipass authorized.', res.stdout)

    def test_cli_exit_zero_on_failure(self):
        """Verifies --exit-zero exits with 0 even when PR checks fail."""
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 102,
                  'title': 'Feature B',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'CHANGES_REQUESTED',
                  'url': 'https://github.com/org/repo/pull/102',
                  'headRefOid': 'sha_fail_102',
                  'headRefName': 'feat-b',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'CLEAN',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'test_job',
                              'status': 'COMPLETED',
                              'conclusion': 'FAILURE',
                              'detailsUrl': 'https://url/fail'
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)
if argv[:2] == ['run', 'list']:
  sys.stdout.write('[]')
  sys.exit(0)
if 'view' in argv and '--job' in argv:
  sys.stdout.write('Failing step log content')
  sys.exit(0)
sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            ['--repo', 'org/repo', '--pr', '102', '--exit-zero'],
            handler,
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        self.assertIn('[PR #102] Tracking started.', res.stdout)
        self.assertIn("Failed check 'test_job' is FAILURE", res.stdout)
        self.assertNotIn('SUPER GREEN!', res.stdout)

    def test_cli_ci_shepherd_report_download_and_triage(self):
        """Verifies downloading CI Shepherd JSON reports and extracting failure telemetry."""
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 102,
                  'title': 'Feature B',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/102',
                  'headRefOid': 'sha_fail_102',
                  'headRefName': 'feat-b',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'BLOCKED',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'validate-result (linux-modular)',
                              'status': 'COMPLETED',
                              'conclusion': 'FAILURE',
                              'detailsUrl': 'https://url/check'
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)

if argv[:2] == ['run', 'list']:
  runs = [{
      'databaseId': 501,
      'workflowName': 'linux',
      'status': 'completed',
      'conclusion': 'failure'
  }]
  sys.stdout.write(json.dumps(runs))
  sys.exit(0)

if argv[:2] == ['run', 'download']:
  dest_dir = argv[argv.index('-D') + 1]
  report_dir = os.path.join(dest_dir, 'shepherd-report-linux-modular')
  os.makedirs(report_dir, exist_ok=True)
  report_file = os.path.join(report_dir, 'shepherd_report.json')
  with open(report_file, 'w', encoding='utf-8') as f:
    json.dump({
        'platform': 'linux-modular',
        'platform_name': 'x64',
        'workflow': 'linux',
        'run_id': '501',
        'run_url': 'https://github.com/org/repo/actions/runs/501',
        'completed_at': '2026-09-15T22:00:00Z',
        'checks': {
            'initialize': 'success',
            'build': 'success',
            'on-host-test': 'failure'
        },
        'test_failures': {
            'failing_tests': {
                'results/test_results.xml': [{
                    'name': 'OzoneTest.CursorCheck',
                    'message': 'Expected 1, got 0\\nStack trace line'
                }]
            }
        }
    }, f)
  sys.exit(0)

sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            ['--repo', 'org/repo', '--pr', '102'],
            handler,
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn('[PR #102] Tracking started.', res.stdout)
        self.assertIn(
            "Failed check 'validate-result (linux-modular)' is FAILURE",
            res.stdout)
        self.assertIn('CI Shepherd Report [linux-modular] (linux)', res.stdout)
        self.assertIn('Failing jobs: on-host-test', res.stdout)
        self.assertIn('OzoneTest.CursorCheck', res.stdout)

    def test_cli_summary_display_with_ci_report(self):
        """Verifies CLI interactive output displays CI Shepherd report findings."""
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 102,
                  'title': 'Feature B',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/102',
                  'headRefOid': 'sha_fail_102',
                  'headRefName': 'feat-b',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'BLOCKED',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'validate-result (linux-modular)',
                              'status': 'COMPLETED',
                              'conclusion': 'FAILURE',
                              'detailsUrl': 'https://url/check'
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)

if argv[:2] == ['run', 'list']:
  runs = [{
      'databaseId': 501,
      'workflowName': 'linux',
      'status': 'completed',
      'conclusion': 'failure'
  }]
  sys.stdout.write(json.dumps(runs))
  sys.exit(0)

if argv[:2] == ['run', 'download']:
  dest_dir = argv[argv.index('-D') + 1]
  report_dir = os.path.join(dest_dir, 'shepherd-report-linux-modular')
  os.makedirs(report_dir, exist_ok=True)
  report_file = os.path.join(report_dir, 'shepherd_report.json')
  with open(report_file, 'w', encoding='utf-8') as f:
    json.dump({
        'platform': 'linux-modular',
        'workflow': 'linux',
        'run_id': '501',
        'run_url': 'https://github.com/org/repo/actions/runs/501',
        'checks': {'build': 'failure'},
        'test_failures': {}
    }, f)
  sys.exit(0)

sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            ['--repo', 'org/repo', '--pr', '102'],
            handler,
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn('[PR #102] Tracking started.', res.stdout)
        self.assertIn(
            "Failed check 'validate-result (linux-modular)' is FAILURE",
            res.stdout)
        self.assertIn('CI Shepherd Report [linux-modular] (linux)', res.stdout)
        self.assertIn('Failing jobs: build', res.stdout)

    def test_cli_super_green_multipass(self):
        """Verifies CLI outputs SUPER GREEN on successful completion."""
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 200,
                  'title': 'Green PR',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/200',
                  'headRefOid': 'sha_200',
                  'headRefName': 'main',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'CLEAN',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'build',
                              'status': 'COMPLETED',
                              'conclusion': 'SUCCESS',
                              'detailsUrl': ''
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)
sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            ['--repo', 'org/repo', '--pr', '200'],
            handler,
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        self.assertIn('SUPER GREEN! Multipass authorized.', res.stdout)

    def test_cli_merge_conflict_warning(self):
        """Verifies CLI warns when merge conflicts are detected."""
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 300,
                  'title': 'Conflict PR',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/300',
                  'headRefOid': 'sha_300',
                  'headRefName': 'branch-c',
                  'mergeable': 'CONFLICTING',
                  'mergeStateStatus': 'DIRTY',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'build',
                              'status': 'COMPLETED',
                              'conclusion': 'SUCCESS',
                              'detailsUrl': ''
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)
sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            ['--repo', 'org/repo', '--pr', '300'],
            handler,
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn('WARNING: PR has merge conflicts! (State: DIRTY)',
                      res.stdout)

    def test_cli_gh_error_handling(self):
        """Verifies graceful handling of gh CLI API errors."""
        handler = """
sys.stderr.write("GraphQL error: Not found\\n")
sys.exit(1)
"""
        res = self.run_cli(
            ['--repo', 'org/repo', '--pr', '400'],
            handler,
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn('[PR #400] Error: GraphQL error: Not found', res.stderr)

    def test_cli_watch_breaks_on_initial_failed_job(self):
        """Verifies watch mode breaks execution upon discovering a failed job on initial poll."""
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 500,
                  'title': 'Failing PR',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/500',
                  'headRefOid': 'sha_fail_500',
                  'headRefName': 'feat-fail',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'BLOCKED',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'test-linux',
                              'status': 'COMPLETED',
                              'conclusion': 'FAILURE',
                              'detailsUrl': 'https://url/fail'
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)

if argv[:2] == ['run', 'list']:
  sys.stdout.write(json.dumps([]))
  sys.exit(0)

sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            [
                '--repo', 'org/repo', '--pr', '500', '--watch', '--interval',
                '1'
            ],
            handler,
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn('[PR #500] Tracking started.', res.stdout)
        self.assertIn("Failed check 'test-linux' is FAILURE", res.stdout)
        self.assertIn('ACTION REQUIRED: Triage failure immediately!',
                      res.stdout)

    def test_cli_watch_breaks_on_subsequent_failed_job(self):
        """Verifies watch mode continues while pending but breaks when a job fails on subsequent poll."""
        state_file = os.path.join(self.test_dir.name, 'poll_count_fail.txt')
        handler = f"""
cmd = ' '.join(argv)
state_file = r"{state_file}"
count = 0
if os.path.exists(state_file):
  with open(state_file, 'r', encoding='utf-8') as f:
    count = int(f.read().strip() or '0')
with open(state_file, 'w', encoding='utf-8') as f:
  f.write(str(count + 1))

if argv[:2] == ['api', 'graphql']:
  conclusion = 'FAILURE' if count >= 1 else 'PENDING'
  status = 'COMPLETED' if count >= 1 else 'IN_PROGRESS'
  data = {{
      'data': {{
          'repository': {{
              'pullRequest': {{
                  'number': 501,
                  'title': 'Dynamic PR',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/501',
                  'headRefOid': 'sha_dyn_501',
                  'headRefName': 'feat-dyn',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'BLOCKED' if count >= 1 else 'CLEAN',
                  'statusCheckRollup': {{
                      'contexts': {{
                          'nodes': [{{
                              '__typename': 'CheckRun',
                              'name': 'build-job',
                              'status': status,
                              'conclusion': conclusion,
                              'detailsUrl': 'https://url/build'
                          }}]
                      }}
                  }},
                  'reviewThreads': {{'nodes': []}}
              }}
          }}
      }}
  }}
  sys.stdout.write(json.dumps(data))
  sys.exit(0)

if argv[:2] == ['run', 'list']:
  sys.stdout.write(json.dumps([]))
  sys.exit(0)

sys.stderr.write(f"Unexpected command: {{argv}}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            [
                '--repo', 'org/repo', '--pr', '501', '--watch', '--interval',
                '1'
            ],
            handler,
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn('[PR #501] Tracking started.', res.stdout)
        self.assertIn("Check 'build-job' changed IN_PROGRESS -> FAILURE",
                      res.stdout)
        self.assertIn('ACTION REQUIRED: Triage failure immediately!',
                      res.stdout)

    def test_cli_watch_continues_pending_until_green(self):
        """Verifies watch mode polls while checks are pending and succeeds when all become green."""
        state_file = os.path.join(self.test_dir.name, 'poll_count_green.txt')
        handler = f"""
cmd = ' '.join(argv)
state_file = r"{state_file}"
count = 0
if os.path.exists(state_file):
  with open(state_file, 'r', encoding='utf-8') as f:
    count = int(f.read().strip() or '0')
with open(state_file, 'w', encoding='utf-8') as f:
  f.write(str(count + 1))

if argv[:2] == ['api', 'graphql']:
  conclusion = 'SUCCESS' if count >= 1 else 'PENDING'
  status = 'COMPLETED' if count >= 1 else 'IN_PROGRESS'
  data = {{
      'data': {{
          'repository': {{
              'pullRequest': {{
                  'number': 502,
                  'title': 'Green PR',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/502',
                  'headRefOid': 'sha_green_502',
                  'headRefName': 'feat-green',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'CLEAN',
                  'statusCheckRollup': {{
                      'contexts': {{
                          'nodes': [{{
                              '__typename': 'CheckRun',
                              'name': 'build-job',
                              'status': status,
                              'conclusion': conclusion,
                              'detailsUrl': 'https://url/build'
                          }}]
                      }}
                  }},
                  'reviewThreads': {{'nodes': []}}
              }}
          }}
      }}
  }}
  sys.stdout.write(json.dumps(data))
  sys.exit(0)

if argv[:2] == ['run', 'list']:
  sys.stdout.write(json.dumps([]))
  sys.exit(0)

sys.stderr.write(f"Unexpected command: {{argv}}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            [
                '--repo', 'org/repo', '--pr', '502', '--watch', '--interval',
                '1'
            ],
            handler,
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        self.assertIn('[PR #502] Tracking started.', res.stdout)
        self.assertIn("Check 'build-job' changed IN_PROGRESS -> SUCCESS",
                      res.stdout)
        self.assertIn('SUPER GREEN! Multipass authorized.', res.stdout)

    def test_cli_downloads_failure_artifacts_and_logs(self):
        """Verifies downloading test results and logs for failed jobs to the artifacts directory."""
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 601,
                  'title': 'Test Download PR',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/601',
                  'headRefOid': 'sha_fail_601',
                  'headRefName': 'feat-fail-download',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'BLOCKED',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'test-linux-modular',
                              'status': 'COMPLETED',
                              'conclusion': 'FAILURE',
                              'databaseId': 1234567,
                              'detailsUrl': 'https://github.com/org/repo/actions/runs/801/job/1234567'
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)

if argv[:2] == ['run', 'list']:
  runs = [{
      'databaseId': 801,
      'workflowName': 'linux',
      'status': 'completed',
      'conclusion': 'failure'
  }]
  sys.stdout.write(json.dumps(runs))
  sys.exit(0)

if argv[:2] == ['run', 'download']:
  dest_dir = argv[argv.index('-D') + 1]
  report_dir = os.path.join(dest_dir, 'shepherd-report-linux-modular')
  os.makedirs(report_dir, exist_ok=True)
  report_file = os.path.join(report_dir, 'shepherd_report.json')
  with open(report_file, 'w', encoding='utf-8') as f:
    json.dump({
        'platform': 'linux-modular',
        'workflow': 'linux',
        'run_id': '801',
        'run_url': 'https://github.com/org/repo/actions/runs/801',
        'checks': {'build': 'success', 'on-host-test': 'failure'},
        'test_failures': {
            'failing_tests': {
                'results/test_results.xml': [{
                    'name': 'DemoTest.AssertionFailure',
                    'message': 'Expected true, got false'
                }]
            }
        }
    }, f)
  sys.exit(0)

if argv[:2] == ['run', 'view']:
  if '--job' in argv:
    sys.stdout.write("FAILED STEP LOG FOR JOB 1234567\\nAssertionError: DemoTest.AssertionFailure\\n")
    sys.exit(0)
  if '--json' in argv and 'jobs' in argv:
    sys.stdout.write(json.dumps({'jobs': [{'name': 'on-host-test', 'databaseId': 9999, 'conclusion': 'failure'}]}))
    sys.exit(0)
  sys.stdout.write("RUN LOG FOR 801\\n")
  sys.exit(0)

sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            ['--repo', 'org/repo', '--pr', '601'],
            handler,
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn('[PR #601] Tracking started.', res.stdout)
        self.assertIn("Failed check 'test-linux-modular' is FAILURE",
                      res.stdout)
        self.assertIn(
            "Downloaded test results and logs for failed jobs to 'artifacts'",
            res.stdout)

        # Verify logs were downloaded to artifacts/logs
        log_file = os.path.join(self.test_dir.name, 'artifacts', 'logs',
                                'test-linux-modular.log')
        self.assertTrue(os.path.exists(log_file),
                        f"Log file missing: {log_file}")
        with open(log_file, 'r', encoding='utf-8') as f:
            log_content = f.read()
        self.assertIn('FAILED STEP LOG FOR JOB 1234567', log_content)

        # Verify test results report exists in artifacts/test_results
        report_file = os.path.join(self.test_dir.name, 'artifacts',
                                   'test_results',
                                   'shepherd-report-linux-modular',
                                   'shepherd_report.json')
        self.assertTrue(os.path.exists(report_file),
                        f"Report file missing: {report_file}")
        with open(report_file, 'r', encoding='utf-8') as f:
            report_data = json.load(f)
        self.assertEqual(report_data['platform'], 'linux-modular')

    def test_cli_downloads_to_custom_artifacts_dir(self):
        """Verifies CLI respects custom --artifacts-dir / --download-dir option."""
        custom_dir = os.path.join(self.test_dir.name, 'custom_triage_dir')
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 602,
                  'title': 'Custom Dir PR',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/602',
                  'headRefOid': 'sha_fail_602',
                  'headRefName': 'feat-custom-dir',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'BLOCKED',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'build-check',
                              'status': 'COMPLETED',
                              'conclusion': 'FAILURE',
                              'databaseId': 7654321,
                              'detailsUrl': 'https://github.com/org/repo/actions/runs/802/job/7654321'
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)

if argv[:2] == ['run', 'list']:
  sys.stdout.write(json.dumps([]))
  sys.exit(0)

if argv[:2] == ['run', 'view']:
  sys.stdout.write("COMPILATION ERROR: file.cc:10: error\\n")
  sys.exit(0)

sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            [
                '--repo', 'org/repo', '--pr', '602', '--artifacts-dir',
                custom_dir
            ],
            handler,
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn(
            f"Downloaded test results and logs for failed jobs to '{custom_dir}'",
            res.stdout)
        log_file = os.path.join(custom_dir, 'logs', 'build-check.log')
        self.assertTrue(os.path.exists(log_file),
                        f"Custom log file missing: {log_file}")
        with open(log_file, 'r', encoding='utf-8') as f:
            self.assertIn('COMPILATION ERROR', f.read())

    def test_cli_no_download_flag(self):
        """Verifies --no-download flag disables automatic downloading."""
        handler = """
cmd = ' '.join(argv)
if argv[:2] == ['api', 'graphql']:
  data = {
      'data': {
          'repository': {
              'pullRequest': {
                  'number': 603,
                  'title': 'No Download PR',
                  'state': 'OPEN',
                  'isDraft': False,
                  'reviewDecision': 'APPROVED',
                  'url': 'https://github.com/org/repo/pull/603',
                  'headRefOid': 'sha_fail_603',
                  'headRefName': 'feat-no-download',
                  'mergeable': 'MERGEABLE',
                  'mergeStateStatus': 'BLOCKED',
                  'statusCheckRollup': {
                      'contexts': {
                          'nodes': [{
                              '__typename': 'CheckRun',
                              'name': 'failing-job',
                              'status': 'COMPLETED',
                              'conclusion': 'FAILURE',
                              'databaseId': 11111,
                              'detailsUrl': 'https://github.com/org/repo/actions/runs/803/job/11111'
                          }]
                      }
                  },
                  'reviewThreads': {'nodes': []}
              }
          }
      }
  }
  sys.stdout.write(json.dumps(data))
  sys.exit(0)

if argv[:2] == ['run', 'list']:
  sys.stdout.write(json.dumps([]))
  sys.exit(0)

sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""
        res = self.run_cli(
            ['--repo', 'org/repo', '--pr', '603', '--no-download'],
            handler,
        )
        self.assertEqual(res.returncode, 1)
        self.assertNotIn("Downloaded test results and logs", res.stdout)
        default_artifacts = os.path.join(self.test_dir.name, 'artifacts')
        self.assertFalse(os.path.exists(default_artifacts))


if __name__ == '__main__':
    unittest.main()
