#!/usr/bin/env python3
# Copyright 2026 Google LLC
# TAG=agy
"""Black-box integration test suite for shepherd_pr.py."""

# pylint: disable=bad-indentation,inconsistent-quotes,line-too-long,consider-using-with,broad-exception-caught

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
                              check=False)

    def test_cli_run_once_green(self):
        """Verifies CLI run-once mode outputs green status JSON."""
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
            ['--repo', 'org/repo', '--pr', '101', '--run-once'],
            handler,
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        output = json.loads(res.stdout)
        self.assertEqual(len(output), 1)
        self.assertEqual(output[0]['pr'], 101)
        self.assertTrue(output[0]['is_green'])
        self.assertEqual(output[0]['mergeable'], 'MERGEABLE')

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
            ['--repo', 'org/repo', '--pr', '102', '--run-once', '--ci-report'],
            handler,
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        output = json.loads(res.stdout)
        self.assertEqual(len(output), 1)
        self.assertEqual(output[0]['pr'], 102)
        self.assertFalse(output[0]['is_green'])

        ci_reports = output[0]['ci_shepherd_reports']
        self.assertEqual(len(ci_reports), 1)
        report = ci_reports[0]
        self.assertEqual(report['platform'], 'linux-modular')
        self.assertEqual(report['workflow'], 'linux')
        self.assertTrue(report['has_failures'])
        self.assertIn('on-host-test', report['failing_jobs'])
        self.assertEqual(len(report['test_failures']), 1)
        self.assertEqual(report['test_failures'][0]['name'],
                         'OzoneTest.CursorCheck')
        self.assertIn('Expected 1, got 0',
                      report['test_failures'][0]['message'])

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
            ['--repo', 'org/repo', '--pr', '102', '--ci-report'],
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
            ['--repo', 'org/repo', '--pr', '400', '--run-once'],
            handler,
        )
        self.assertEqual(res.returncode, 0)
        output = json.loads(res.stdout)
        self.assertEqual(len(output), 1)
        self.assertEqual(output[0]['pr'], 400)
        self.assertFalse(output[0]['is_green'])
        self.assertIn('GraphQL error: Not found', output[0]['error'])


if __name__ == '__main__':
    unittest.main()
