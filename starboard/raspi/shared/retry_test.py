#
# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Tests for `retry` module"""

import unittest
from starboard.shared import retry
import argparse
import logging
import sys
import time
from enum import IntEnum


class Behavior(IntEnum):
  OK = 0
  OS_ERROR = 1
  RUNTIME_ERROR = 2
  OTHER_ERROR = 3


def _problem(param: int, caller=str):
  logging.info('%s: param=%d', caller, param)
  if param == Behavior.OS_ERROR:
    raise OSError('OS made an oops')
  if param == Behavior.RUNTIME_ERROR:
    raise RuntimeError('Runtime oops')
  if param == Behavior.OTHER_ERROR:
    raise MemoryError('Download more RAM')
  return 100 + param * 3


def problem(param: int):
  return _problem(param, 'undecorated problem')


@retry.retry(exceptions=(RuntimeError,), retries=1)
def decorated_runtimeerror(param: int):
  return _problem(param, 'decorated with runtimeerror')


@retry.retry(exceptions=(OSError,), retries=1)
def decorated_oserror(param: int):
  return _problem(param, 'decorated with oserror')


@retry.retry(exceptions=(OSError, RuntimeError), retries=1)
def decorated_both(param: int):
  return _problem(param, 'decorated with oserror+runtimeerror')


@retry.retry(
    exceptions=(OSError,),
    retries=2,
    backoff=lambda: (logging.info('sleeping 0.2'), time.sleep(0.2)))
def decorated_oserror_backoff_2(param: int):
  return _problem(param, 'decorated with oserror, 2 retries and sleep backoff')


class RetryTest(unittest.TestCase):

  def setUp(self) -> None:
    self.actual_calls = 0
    self.call_counter = 0
    return super().setUp()

  def problem(self, param):
    self.actual_calls += 1
    return _problem(param, 'undecorated problem method')

  @retry.retry(exceptions=(OSError,), retries=1)
  def decorated_os_problem(self, param):
    self.actual_calls += 1
    return _problem(param, 'decorated problem method')

  @retry.retry(exceptions=(OSError,), retries=1, wrap_exceptions=False)
  def decorated_os_problem_nowrap(self, param):
    self.actual_calls += 1
    return _problem(param, 'decorated problem method, pass-through exceptions')

  @retry.retry(exceptions=(OSError,), retries=5)
  def decorated_os_problem_3(self, param):
    self.actual_calls += 1
    self.call_counter += 1
    if self.call_counter == 3:
      return 200
    return _problem(param, 'decorated problem that succeeds on 3rd try')

  def test_ok_call_undecorated(self):
    self.assertEqual(100, retry.with_retry(problem, (Behavior.OK,)))
    self.assertEqual(100, retry.with_retry(self.problem, (Behavior.OK,)))
    self.assertEqual(self.actual_calls, 1)

  def test_ok_call_decorated(self):
    self.assertEqual(100, decorated_both(Behavior.OK))
    self.assertEqual(100, self.decorated_os_problem(Behavior.OK))
    self.assertEqual(self.actual_calls, 1)

  def test_retry_exceeds(self):
    with self.assertRaises(OSError):
      retry.with_retry(problem, (Behavior.OS_ERROR,), retries=0)
    with self.assertRaises(retry.RetriesExceeded):
      retry.with_retry(problem, (Behavior.OS_ERROR,), retries=1)
    with self.assertRaises(retry.RetriesExceeded):
      retry.with_retry(problem, (Behavior.OS_ERROR,), retries=50)
    with self.assertRaises(retry.RetriesExceeded):
      retry.with_retry(self.problem, (Behavior.OS_ERROR,), retries=1)
    self.assertEqual(self.actual_calls, 2)

  def test_retry_exceeds_decorated(self):
    with self.assertRaises(retry.RetriesExceeded):
      decorated_oserror(Behavior.OS_ERROR)
    with self.assertRaises(retry.RetriesExceeded):
      self.decorated_os_problem(Behavior.OS_ERROR)
    self.assertEqual(self.actual_calls, 2)
    with self.assertRaises(retry.RetriesExceeded):
      decorated_runtimeerror(Behavior.RUNTIME_ERROR)

  def test_other_exceptions_propagate(self):
    with self.assertRaises(RuntimeError):
      retry.with_retry(
          problem, (Behavior.RUNTIME_ERROR,),
          exceptions=(OSError, MemoryError),
          retries=0)
    with self.assertRaises(RuntimeError):
      retry.with_retry(
          problem, (Behavior.RUNTIME_ERROR,),
          exceptions=(OSError, MemoryError),
          retries=4)
    with self.assertRaises(RuntimeError):
      retry.with_retry(
          self.problem, (Behavior.RUNTIME_ERROR,),
          exceptions=(OSError, MemoryError),
          retries=0)
    self.assertEqual(self.actual_calls, 1)
    with self.assertRaises(RuntimeError):
      retry.with_retry(
          self.problem, (Behavior.RUNTIME_ERROR,),
          exceptions=(OSError, MemoryError),
          retries=50)
    self.assertEqual(self.actual_calls, 2)

  def test_original_exceptions(self):
    with self.assertRaises(OSError):
      retry.with_retry(
          problem, (Behavior.OS_ERROR,), retries=0, wrap_exceptions=False)
    with self.assertRaises(OSError):
      retry.with_retry(
          problem, (Behavior.OS_ERROR,), retries=1, wrap_exceptions=False)
    with self.assertRaises(OSError):
      retry.with_retry(
          problem, (Behavior.OS_ERROR,), retries=50, wrap_exceptions=False)
    with self.assertRaises(OSError):
      self.decorated_os_problem_nowrap(Behavior.OS_ERROR)

  def test_call_can_succeed_1(self):
    self.assertEqual(100, self.decorated_os_problem_3(Behavior.OK))
    self.assertEqual(self.actual_calls, 1)

  def test_call_can_succeed_2(self):
    self.assertEqual(200, self.decorated_os_problem_3(Behavior.OS_ERROR))
    self.assertEqual(self.actual_calls, 3)
    with self.assertRaises(RuntimeError):  # ensure other errors still throw
      self.decorated_os_problem_3(Behavior.RUNTIME_ERROR)

  def test_backoff_gets_called(self):
    backoff_calls = 0

    def inrcement():
      nonlocal backoff_calls
      backoff_calls += 1

    with self.assertRaises(RuntimeError):
      retry.with_retry(
          self.problem, (Behavior.RUNTIME_ERROR,),
          exceptions=(OSError),
          retries=2,
          backoff=inrcement)
    self.assertEqual(backoff_calls, 0)
    with self.assertRaises(RuntimeError):
      retry.with_retry(
          self.problem, (Behavior.RUNTIME_ERROR,),
          exceptions=(RuntimeError),
          retries=2,
          backoff=inrcement)
    self.assertEqual(backoff_calls, 2)

  def test_backoff_terminates_loop(self):
    with self.assertRaises(StopIteration):
      retry.with_retry(
          self.problem, (Behavior.RUNTIME_ERROR,),
          exceptions=(RuntimeError),
          retries=2,
          backoff=lambda: True)

  def test_exception_has_details(self):
    with self.assertRaises(retry.RetriesExceeded) as context:
      retry.with_retry(problem, (Behavior.OS_ERROR,), retries=50)
    self.assertIn('50', str(context.exception))
    self.assertIn('problem', str(context.exception))


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--verbose', '-v', action='store_true')
  parser.add_argument('func_behavior', type=int)
  parser.add_argument('--oserror', action='store_true')
  parser.add_argument('--runtimeerror', action='store_true')
  parser.add_argument('--retries', type=int, default=1)
  parser.add_argument('--backoff', action='store_true')
  parser.add_argument('--decorated', action='store_true')
  args = parser.parse_args()
  logging.basicConfig(
      stream=sys.stdout, level=logging.DEBUG if args.verbose else logging.INFO)
  exceptions = []
  if args.oserror:
    exceptions.append(OSError)
  if args.runtimeerror:
    exceptions.append(RuntimeError)
  backoff = None if not args.backoff else lambda: (print('Backoff'),
                                                   time.sleep(1))
  if not args.decorated:
    if exceptions:
      print(
          retry.with_retry(
              problem, (args.func_behavior,),
              retries=args.retries,
              exceptions=tuple(exceptions),
              backoff=backoff))
    else:  # default, accept all exceptions
      print(
          retry.with_retry(
              problem, (args.func_behavior,),
              retries=args.retries,
              backoff=backoff))
  else:
    if args.backoff:
      print(decorated_oserror_backoff_2(args.func_behavior))
    elif args.oserror and args.runtimeerror:
      print(decorated_both(args.func_behavior))
    elif args.oserror:
      print(decorated_oserror(args.func_behavior))
    elif args.runtimeerror:
      print(decorated_runtimeerror(args.func_behavior))
    else:
      raise NotImplementedError('No test implemented for these args')
