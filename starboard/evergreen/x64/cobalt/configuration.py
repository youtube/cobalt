# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Cobalt Evergreen x64 configuration."""

import os

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter


class CobaltX64Configuration(cobalt_configuration.CobaltConfiguration):
  """Starboard Cobalt Evergreen x64 configuration."""

  def __init__(self, platform_configuration, application_name,
               application_directory):
    super(CobaltX64Configuration,
          self).__init__(platform_configuration, application_name,
                         application_directory)

  def GetPostIncludes(self):
    # If there isn't a configuration.gypi found in the usual place, we'll
    # supplement with our shared implementation.
    includes = super(CobaltX64Configuration, self).GetPostIncludes()
    for include in includes:
      if os.path.basename(include) == 'configuration.gypi':
        return includes

    shared_gypi_path = os.path.join(
        os.path.dirname(__file__), 'configuration.gypi')
    if os.path.isfile(shared_gypi_path):
      includes.append(shared_gypi_path)
    return includes

  def WebdriverBenchmarksEnabled(self):
    return True

  def GetTestFilters(self):
    filters = super(CobaltX64Configuration, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetWebPlatformTestFilters(self):
    filters = super(CobaltX64Configuration, self).GetWebPlatformTestFilters()
    filters.extend([
        # These tests are timing-sensitive, and are thus flaky on slower builds
        test_filter.TestFilter(
            'web_platform_tests',
            'xhr/WebPlatformTest.Run/XMLHttpRequest_send_timeout_events_htm',
            'debug'),
        test_filter.TestFilter(
            'web_platform_tests',
            'streams/WebPlatformTest.Run/streams_readable_streams_templated_html',
            'debug'),
        test_filter.TestFilter(
            'web_platform_tests',
            'cors/WebPlatformTest.Run/cors_preflight_failure_htm', 'devel'),
    ])
    return filters

  def GetTestEnvVariables(self):
    return {
        'base_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        },
        'crypto_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        },
        'net_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        }
    }

  __FILTERED_TESTS = {
      'base_unittests': [
          '*Death*',
          'BlockShutdown/TaskSchedulerTaskTrackerTest.DoublePendingFlushAsyncForTestingFails/0',
          'BlockShutdown/TaskSchedulerTaskTrackerTest.IOAllowed/0',
          'CheckedObserverListTest.*',
          'ContinueOnShutdown/TaskSchedulerTaskTrackerTest.DoublePendingFlushAsyncForTestingFails/0',
          'ContinueOnShutdown/TaskSchedulerTaskTrackerTest.IOAllowed/0',
          'ContinueOnShutdown/TaskSchedulerTaskTrackerTest.SingletonAllowed/0',
          'FileUtilTest.AppendToFile',
          'FileUtilTest.CopyFile',
          'IDMapTest.RemovedValueHandling',
          'Mock/RunLoopTest.DisallowRunningForTesting/0',
          'ObserverListTest/0.NonReentrantObserverList',
          'ObserverListTest/0.StdIteratorRemoveFront',
          'ObserverListTest/1.NonReentrantObserverList',
          'PostTaskTestWithExecutor.RegisterExecutorTwice',
          'Real/RunLoopTest.DisallowRunningForTesting/0',
          'SafeNumerics.SignedIntegerMath',
          'SafeNumerics.UnsignedIntegerMath',
          'ScopedBlockingCallDestructionOrderTest.InvalidDestructionOrder',
          'SequenceCheckerMacroTest.Macros',
          'SequenceManagerTest.SetTimeDomainForDisabledQueue/0',
          'SimpleThreadTest.NonJoinableStartAndDieOnJoin',
          'SkipOnShutdown/TaskSchedulerTaskTrackerTest.DoublePendingFlushAsyncForTestingFails/0',
          'SkipOnShutdown/TaskSchedulerTaskTrackerTest.IOAllowed/0',
          'TaskSchedulerImplTest.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated',
          'TaskSchedulerLock.AcquireMultipleLocksNoTransitivity',
          'TaskSchedulerLock.AcquireNonPredecessor',
          'TaskSchedulerLock.AcquirePredecessorWrongOrder',
          'TaskSchedulerLock.PredecessorCycle',
          'TaskSchedulerLock.PredecessorLongerCycle',
          'TaskSchedulerLock.SelfReferentialLock',
          'TaskSchedulerPriorityQueueTest.IllegalTwoTransactionsSameThread',
          'TaskSchedulerSequenceTest.PopNonEmptyFrontSlot',
          'TaskSchedulerSequenceTest.TakeEmptyFrontSlot',
          'TaskSchedulerSequenceTest.TakeEmptySequence',
          'TaskSchedulerSingleThreadTaskRunnerManagerTest.SharedWithBaseSyncPrimitivesDCHECKs',
          'TaskSchedulerTaskTrackerWaitAllowedTest.WaitAllowed',
          'TaskSchedulerWorkerStackTest.PushTwice',
          'TestMockTimeTaskRunnerTest.DefaultUnbound',
          'ThreadCheckerMacroTest.Macros',
          'ThreadRestrictionsTest.DisallowBaseSyncPrimitives',
          'ThreadRestrictionsTest.DisallowUnresponsiveTasks',
          'ThreadRestrictionsTest.ScopedAllowBaseSyncPrimitivesForTestingResetsState',
          'ThreadRestrictionsTest.ScopedAllowBaseSyncPrimitivesOutsideBlockingScopeResetsState',
          'ThreadRestrictionsTest.ScopedAllowBaseSyncPrimitivesResetsState',
          'ThreadRestrictionsTest.ScopedAllowBaseSyncPrimitivesWithBlockingDisallowed',
          'ThreadRestrictionsTest.ScopedAllowBlocking',
          'ThreadRestrictionsTest.ScopedAllowBlockingForTesting',
          'ThreadRestrictionsTest.ScopedDisallowBlocking',
          'ThreadTest.StartTwiceNonJoinableNotAllowed',
      ],
      'css_parser_test': ['LocaleNumeric/ScannerTest.*'],
      'dom_parser_test': [test_filter.FILTER_ALL],
      'layout_tests': [test_filter.FILTER_ALL],
      'nb_test': ['MemoryTrackerImplTest.MultiThreadedStress*Test'],
      'net_unittests': [test_filter.FILTER_ALL],
  }
