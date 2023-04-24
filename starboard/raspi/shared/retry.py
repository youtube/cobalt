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
"""General retry wrapper module

Allows retrying a function call either with a decorator or inline call.
This is a substitute for more comprehensive Python retry wrapper packages like
`tenacity`, `retry`, `backoff` and others.
The only reason this exists is that Python package deployment for on-device
tests cannot currently dynamically include dependencies.
TODO(b/279249837): Remove this and use an off the shelf package.
"""

from typing import Sequence, Callable
import functools
import logging


class RetriesExceeded(RuntimeError):
  """Exception recording retry failure conditions"""

  def __init__(self, retries: int, function: Callable, *args, **kwargs) -> None:
    super().__init__(*args, **kwargs)
    self.retries = retries
    self.function = function

  def __str__(self) -> str:
    callable_str = getattr(self.function, '__name__', repr(self.function))
    return (f'Retries exceeded while calling {callable_str}'
            f' with max {self.retries}') + super().__str__()


def _retry_function(function: Callable, exceptions: Sequence, retries: int,
                    backoff: Callable, wrap_exceptions: bool):
  current_retry = 0
  while current_retry <= retries:
    try:
      return function()
    except exceptions as inner:
      current_retry += 1
      logging.debug('Exception running %s, retry %d/%d', function, retry,
                    retries)
      if current_retry > retries:
        # If 0 retries were attempted, pass up original exception
        if not retries or not wrap_exceptions:
          raise
        raise RetriesExceeded(retries, function) from inner
      if backoff:
        if backoff():
          raise StopIteration() from inner

  raise RuntimeError('Bug: we should never get here')


def with_retry(function: Callable,
               args: tuple = (),
               kwargs: dict = None,
               exceptions: Sequence = (Exception,),
               retries: int = 0,
               backoff: Callable = None,
               wrap_exceptions: bool = True):
  """Call a function with retry on exception

    :param args: Called function positional args.
    :param kwargs: Called function named args.
    :param exceptions: Sequence of exception types that will be retried.
    :param retries: Max retries attempted.
    :param backoff: Optional backoff callable. Truthy return from callable
        terminates the loop.
    :param wrap_exceptions: If true ( default ) wrap underlying exceptions in
        RetriesExceeded exception type
    :
  """
  return _retry_function(
      functools.partial(function, *args, **(kwargs if kwargs else {})),
      exceptions=exceptions,
      retries=retries,
      backoff=backoff,
      wrap_exceptions=wrap_exceptions,
  )


def retry(exceptions: Sequence = (Exception,),
          retries: int = 0,
          backoff: Callable = None,
          wrap_exceptions: bool = True):
  """Decorator for self-retrying function on thrown exception

    :param exceptions: Sequence of exception types that will be retried.
    :param retries: Max retries attempted.
    :param backoff: Optional backoff callable. Truthy return from callable
        terminates the loop.
    :param wrap_exceptions: If true ( default ) wrap underlying exceptions in
        RetriesExceeded exception type
    """

  def decorator(function):

    @functools.wraps(function)
    def wrapper(*args, **kwargs):
      return _retry_function(
          functools.partial(function, *args, **kwargs),
          exceptions=exceptions,
          retries=retries,
          backoff=backoff,
          wrap_exceptions=wrap_exceptions)

    return wrapper

  return decorator
