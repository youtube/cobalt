# Copyright 2016 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Contains a contextmanager for a timer.

  Example usage:

    from timer import Timer

    with Timer('SomeTask') as t:
      # Do some time consuming task
      print('So far {} seconds have passed'.format(t.seconds_elapsed))

  print(t)  # This will print something like 'SomeTask took 1.2 seconds'
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import timeit


class Timer(object):
  """ContextManager for measuring time since an event."""

  def __init__(self, description):
    """Initializes the timer.

    Args:
      description: A string containing the description of the timer.
    """
    self.description = description
    self._timer = timeit.default_timer  # Choose best timer for the platform.
    self.start_time = None
    self._seconds_elapsed = None

  def __enter__(self):
    """Starts the timer.

    __enter__ allows this class to be used as a ContextManager.

    Returns:
      The object itself so it works with the |with| statement.
    """
    self.start_time = self._timer()
    return self

  def __exit__(self, unused_ex_type, unused_ex, unused_ex_trace):
    """Stops the timer and records the duration.

    __enter__ allows this class to be used as a ContextManager.

    Returns:
      False.  Any exception raised within the body of the contextmanager will
      propogate up the stack.
    """
    self._seconds_elapsed = self._timer() - self.start_time
    return False

  @property
  def seconds_elapsed(self):
    """Number of seconds elapsed since start until now or time when timer ended.

    This property will return the number of seconds since the timer was started,
    or the duration of the timer's context manager.

    Returns:
      A float containing the number of seconds elapsed.
    """
    if self._seconds_elapsed is None:
      return self._timer() - self.start_time

    return self._seconds_elapsed

  def __str__(self):
    """A magic method for generating a string representation of the object.

    Returns:
      A string containing a description and a human readable version of timer's
      value.
    """
    return '{} took {} seconds.'.format(self.description, self.seconds_elapsed)
