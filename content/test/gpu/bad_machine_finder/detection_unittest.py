# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import decimal
import unittest

from bad_machine_finder import detection
from bad_machine_finder import tasks


class BadMachineListUnittest(unittest.TestCase):

  def testBasic(self):
    """Tests basic functionality of the class."""
    first_list = detection.BadMachineList()
    first_list.AddBadMachine('bot-1', 'reason-1')
    first_list.AddBadMachine('bot-2', 'reason-2')

    second_list = detection.BadMachineList()
    second_list.AddBadMachine('bot-2', 'reason-3')
    second_list.AddBadMachine('bot-2', 'reason-4')

    first_list.Merge(second_list)
    expected_bad_machines = {
        'bot-1': [
            'reason-1',
        ],
        'bot-2': [
            'reason-2',
            'reason-3',
            'reason-4',
        ],
    }

    self.assertEqual(first_list.bad_machines, expected_bad_machines)

  def testRemoveLowConfidenceMachines(self):
    """Tests that low confidence machines are correctly removed."""
    bad_machine_list = detection.BadMachineList()
    bad_machine_list.AddBadMachine('bot-1', 'reason-1')
    bad_machine_list.AddBadMachine('bot-2', 'reason-2')
    bad_machine_list.AddBadMachine('bot-2', 'reason-3')

    expected_bad_machines = {
        'bot-1': ['reason-1'],
        'bot-2': [
            'reason-2',
            'reason-3',
        ],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

    bad_machine_list.RemoveLowConfidenceMachines(2)
    expected_bad_machines = {
        'bot-2': [
            'reason-2',
            'reason-3',
        ],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testIterMarkdown(self):
    """Tests that Markdown output is correct."""
    bad_machine_list = detection.BadMachineList()
    bad_machine_list.AddBadMachine('bot-2', 'reason-2')
    bad_machine_list.AddBadMachine('bot-2', 'reason-3')
    bad_machine_list.AddBadMachine('bot-1', 'reason-1')

    bot_1_expected_markdown = """\
  * bot-1
    * reason-1"""
    bot_2_expected_markdown = """\
  * bot-2
    * reason-2
    * reason-3"""

    expected_pairs = [
        ('bot-1', bot_1_expected_markdown),
        ('bot-2', bot_2_expected_markdown),
    ]
    self.assertEqual(list(bad_machine_list.IterMarkdown()), expected_pairs)


class MixinGroupedBadMachinesUnittest(unittest.TestCase):

  def testInputValidation(self):
    """Tests that invalid inputs are properly caught."""
    bad_machine_list = detection.BadMachineList()
    mgbm = detection.MixinGroupedBadMachines()
    mgbm.AddMixinData('mixin_name', bad_machine_list)
    with self.assertRaisesRegex(
        ValueError, 'Bad machines for mixin mixin_name were already added'):
      mgbm.AddMixinData('mixin_name', bad_machine_list)

  def testGetAllBadMachineNames(self):
    """Tests that all bad machine names are properly returned."""
    first_bad_machine_list = detection.BadMachineList()
    first_bad_machine_list.AddBadMachine('bot-1', 'reason-1')
    first_bad_machine_list.AddBadMachine('bot-2', 'reason-2')

    second_bad_machine_list = detection.BadMachineList()
    second_bad_machine_list.AddBadMachine('bot-2', 'reason-3')
    second_bad_machine_list.AddBadMachine('bot-3', 'reason-4')

    mgbm = detection.MixinGroupedBadMachines()
    mgbm.AddMixinData('first', first_bad_machine_list)
    mgbm.AddMixinData('second', second_bad_machine_list)

    self.assertEqual(mgbm.GetAllBadMachineNames(), {'bot-1', 'bot-2', 'bot-3'})

  def testGenerateMarkdown(self):
    """Tests basic Markdown generation."""
    first_bad_machine_list = detection.BadMachineList()
    first_bad_machine_list.AddBadMachine('bot-1', 'reason-1a')
    first_bad_machine_list.AddBadMachine('bot-1', 'reason-1b')
    first_bad_machine_list.AddBadMachine('bot-2', 'reason-2')

    second_bad_machine_list = detection.BadMachineList()
    second_bad_machine_list.AddBadMachine('bot-3', 'reason-3')
    second_bad_machine_list.AddBadMachine('bot-4', 'reason-4')

    mgbm = detection.MixinGroupedBadMachines()
    mgbm.AddMixinData('mixin-b', second_bad_machine_list)
    mgbm.AddMixinData('mixin-a', first_bad_machine_list)

    expected_markdown = """\
Bad machines for mixin-a
  * bot-1
    * reason-1a
    * reason-1b
  * bot-2
    * reason-2

Bad machines for mixin-b
  * bot-3
    * reason-3
  * bot-4
    * reason-4"""
    self.assertEqual(mgbm.GenerateMarkdown(), expected_markdown)

  def testGenerateMarkdownWithSomeBotsSkipped(self):
    """Tests Markdown generation when some bots should be skipped."""
    first_bad_machine_list = detection.BadMachineList()
    first_bad_machine_list.AddBadMachine('bot-1', 'reason-1a')
    first_bad_machine_list.AddBadMachine('bot-1', 'reason-1b')
    first_bad_machine_list.AddBadMachine('bot-2', 'reason-2')

    second_bad_machine_list = detection.BadMachineList()
    second_bad_machine_list.AddBadMachine('bot-3', 'reason-3')
    second_bad_machine_list.AddBadMachine('bot-4', 'reason-4')

    mgbm = detection.MixinGroupedBadMachines()
    mgbm.AddMixinData('mixin-b', second_bad_machine_list)
    mgbm.AddMixinData('mixin-a', first_bad_machine_list)

    expected_markdown = """\
Bad machines for mixin-a
  * bot-2
    * reason-2"""
    self.assertEqual(
        mgbm.GenerateMarkdown(bots_to_skip={'bot-1', 'bot-3', 'bot-4'}),
        expected_markdown)

  def testGenerateMarkdownWithAllBotsSkipped(self):
    """Tests Markdown generation when all bots should be skipped."""
    first_bad_machine_list = detection.BadMachineList()
    first_bad_machine_list.AddBadMachine('bot-1', 'reason-1a')
    first_bad_machine_list.AddBadMachine('bot-1', 'reason-1b')
    first_bad_machine_list.AddBadMachine('bot-2', 'reason-2')

    second_bad_machine_list = detection.BadMachineList()
    second_bad_machine_list.AddBadMachine('bot-3', 'reason-3')
    second_bad_machine_list.AddBadMachine('bot-4', 'reason-4')

    mgbm = detection.MixinGroupedBadMachines()
    mgbm.AddMixinData('mixin-b', second_bad_machine_list)
    mgbm.AddMixinData('mixin-a', first_bad_machine_list)

    self.assertEqual(
        mgbm.GenerateMarkdown(
            bots_to_skip={'bot-1', 'bot-2', 'bot-3', 'bot-4'}), '')


class DetectViaStdDevOutlierUnittest(unittest.TestCase):

  def testInputChecking(self):
    """Tests that invalid inputs are checked."""
    # No tasks.
    mixin_stats = tasks.MixinStats()
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaStdDevOutlier(mixin_stats, 2, 0)

    # Negative threshold
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bot', 'suite', 1, 0)
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaStdDevOutlier(mixin_stats, -1, 0)

  def testSmallGoodFleet(self):
    mixin_stats = tasks.MixinStats()
    for i in range(10):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, i % 5)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaStdDevOutlier(mixin_stats, 2, 0)
    self.assertEqual(bad_machine_list.bad_machines, {})

  def testOneClearlyBadMachineSmallFleet(self):
    """Tests behavior when there is a single clearly bad machine."""
    # We need enough samples that the mean is sufficiently skewed towards the
    # good bots' failure rate for this detection method to work.
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 99)
    for i in range(10):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, 1)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaStdDevOutlier(mixin_stats, 2, 0)
    expected_bad_machines = {
        'bad-bot': [('Had a failure rate of 0.99 despite a fleet-wide average '
                     'of 0.09909090909090909 and a standard deviation of '
                     '0.2817301915422738.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSeveralBadMachinesLargeFleet(self):
    """Tests behavior when there are several bad machines in a large fleet."""
    mixin_stats = tasks.MixinStats()
    for i in range(98):
      mixin_stats.AddStatsForBotAndSuite(f'bot-{i}', 'suite', 100, 1)
    mixin_stats.AddStatsForBotAndSuite('bot-98', 'suite', 100, 15)
    mixin_stats.AddStatsForBotAndSuite('bot-99', 'suite', 100, 20)
    mixin_stats.AddStatsForBotAndSuite('bot-100', 'suite', 100, 50)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaStdDevOutlier(mixin_stats, 2, 0)
    expected_bad_machines = {
        'bot-98': [('Had a failure rate of 0.15 despite a fleet-wide average '
                    'of 0.018118811881188118 and a standard deviation of '
                    '0.05350511905346074.')],
        'bot-99': [('Had a failure rate of 0.2 despite a fleet-wide average '
                    'of 0.018118811881188118 and a standard deviation of '
                    '0.05350511905346074.')],
        'bot-100': [('Had a failure rate of 0.5 despite a fleet-wide average '
                     'of 0.018118811881188118 and a standard deviation of '
                     '0.05350511905346074.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSmallFlakyFleet(self):
    """Tests behavior when there's a bad machine in a small, flaky fleet."""
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 50)
    for i in range(9):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, 25)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaStdDevOutlier(mixin_stats, 2, 0)
    expected_bad_machines = {
        'bad-bot': [('Had a failure rate of 0.5 despite a fleet-wide average '
                     'of 0.275 and a standard deviation of 0.075.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testLowFailedTaskMachinesSkipped(self):
    """Tests that machines w/ low failed task counts are skipped."""
    mixin_stats = tasks.MixinStats()
    for i in range(98):
      mixin_stats.AddStatsForBotAndSuite(f'bot-{i}', 'suite', 100, 1)
    mixin_stats.AddStatsForBotAndSuite('bot-98', 'suite', 10, 3)
    mixin_stats.AddStatsForBotAndSuite('bot-99', 'suite', 100, 50)
    mixin_stats.Freeze()

    with self.assertLogs(level='DEBUG') as log_manager:
      bad_machine_list = detection.DetectViaStdDevOutlier(mixin_stats, 2, 5)
      for line in log_manager.output:
        if ('Bot bot-98 skipped in DetectViaStdDevOutlier due to only having 3 '
            'failed tasks') in line:
          break
      else:
        self.fail('Did not find expected log line')

    expected_bad_machines = {
        'bot-99': [('Had a failure rate of 0.5 despite a fleet-wide average '
                    'of 0.0178 and a standard deviation of '
                    '0.05640177302177654.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)


class DetectViaRandomChanceUnittest(unittest.TestCase):

  def testInputChecking(self):
    """Tests that invalid inputs are checked."""
    # No tasks.
    mixin_stats = tasks.MixinStats()
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaRandomChance(mixin_stats, 0.005)

    # Non-positive probability.
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bot', 'suite', 1, 0)
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaRandomChance(mixin_stats, 0)

    # >1 probability
    with self.assertRaises(ValueError):
      detection.DetectViaRandomChance(mixin_stats, 1.1)

  def testSmallGoodFleet(self):
    """Tests behavior when there are no bad machines."""
    mixin_stats = tasks.MixinStats()
    for i in range(10):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, i % 5)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaRandomChance(mixin_stats, 0.005)
    self.assertEqual(bad_machine_list.bad_machines, {})

  def testOneClearlyBadMachineSmallFleet(self):
    """Tests behavior when there is a single clearly bad machine."""
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 99)
    mixin_stats.AddStatsForBotAndSuite('good-bot', 'suite', 100, 1)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaRandomChance(mixin_stats, 0.005)
    expected_bad_machines = {
        'bad-bot': [('99 of 100 tasks failed despite a fleet-wide average '
                     'failed task rate of 0.5. The probability of this '
                     'happening randomly is 7.967495142732219e-29.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSeveralBadMachinesLargeFleet(self):
    """Tests behavior when there are several bad machines in a large fleet."""
    mixin_stats = tasks.MixinStats()
    for i in range(98):
      mixin_stats.AddStatsForBotAndSuite(f'bot-{i}', 'suite', 100, 1)
    mixin_stats.AddStatsForBotAndSuite('bot-98', 'suite', 100, 15)
    mixin_stats.AddStatsForBotAndSuite('bot-99', 'suite', 100, 20)
    mixin_stats.AddStatsForBotAndSuite('bot-100', 'suite', 100, 50)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaRandomChance(mixin_stats, 0.005)
    expected_bad_machines = {
        'bot-98': [('15 of 100 tasks failed despite a fleet-wide average '
                    'failed task rate of 0.01811881188118811881188118812. '
                    'The probability of this happening randomly is '
                    '4.41689373707857e-10.')],
        'bot-99': [('20 of 100 tasks failed despite a fleet-wide average '
                    'failed task rate of 0.01811881188118811881188118812. The '
                    'probability of this happening randomly is '
                    '1.9407812867119233e-15.')],
        'bot-100': [('50 of 100 tasks failed despite a fleet-wide average '
                     'failed task rate of 0.01811881188118811881188118812. The '
                     'probability of this happening randomly is '
                     '3.3205488374477226e-59.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSmallFlakyFleet(self):
    """Tests behavior when there's a bad machine in a small, flaky fleet."""
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 50)
    for i in range(9):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, 25)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaRandomChance(mixin_stats, 0.005)
    expected_bad_machines = {
        'bad-bot': [('50 of 100 tasks failed despite a fleet-wide average '
                     'failed task rate of 0.275. The probability of this '
                     'happening randomly is 1.5273539960703075e-06.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)


class DetectViaInterquartileRangeUnittest(unittest.TestCase):

  def testInputChecking(self):
    """Tests that invalid inputs are checked."""
    # No tasks.
    mixin_stats = tasks.MixinStats()
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaInterquartileRange(mixin_stats, 'mixin_name', 1.5, 0)

    # Non-positive iqr_multiplier.
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bot', 'suite', 1, 0)
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaInterquartileRange(mixin_stats, 'mixin_name', 0, 0)

    # Less than the number of samples needed for quartiles to be meaningful.
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bot-1', 'suite', 99, 0)
    mixin_stats.AddStatsForBotAndSuite('bot-2', 'suite', 1, 0)
    mixin_stats.AddStatsForBotAndSuite('bot-3', 'suite', 1, 0)
    mixin_stats.AddStatsForBotAndSuite('bot-4', 'suite', 1, 0)
    mixin_stats.Freeze()

    with self.assertLogs(level='INFO') as log_manager:
      bad_machine_list = detection.DetectViaInterquartileRange(
          mixin_stats, 'mixin_name', 1.5, 0)
      for line in log_manager.output:
        if ('Quartiles require at least 5 samples to be meaningful. Mixin '
            'mixin_name only provided 4 samples.') in line:
          break
      else:
        self.fail('Did not find expected log line')
    self.assertEqual(bad_machine_list.bad_machines, {})

    # IQR ends up being 0.
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bot-1', 'suite', 99, 0)
    mixin_stats.AddStatsForBotAndSuite('bot-2', 'suite', 1, 0)
    mixin_stats.AddStatsForBotAndSuite('bot-3', 'suite', 1, 0)
    mixin_stats.AddStatsForBotAndSuite('bot-4', 'suite', 1, 0)
    mixin_stats.AddStatsForBotAndSuite('bot-5', 'suite', 1, 0)
    mixin_stats.Freeze()

    with self.assertLogs(level='INFO') as log_manager:
      bad_machine_list = detection.DetectViaInterquartileRange(
          mixin_stats, 'mixin_name', 1.5, 0)
      for line in log_manager.output:
        if ('Mixin mixin_name resulted in an IQR of 0, which is not useful for '
            'detecting outliers.') in line:
          break
      else:
        self.fail('Did not find expected log line')
    self.assertEqual(bad_machine_list.bad_machines, {})

  def testSmallGoodFleet(self):
    """Tests behavior when there are no bad machines."""
    mixin_stats = tasks.MixinStats()
    for i in range(10):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, i % 5)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaInterquartileRange(
        mixin_stats, 'mixin_name', 1.5, 0)
    self.assertEqual(bad_machine_list.bad_machines, {})

  def testOneClearlyBadMachineSmallFleet(self):
    """Tests behavior when there is a single clearly bad machine."""
    mixin_stats = tasks.MixinStats()
    # We need > 4 samples in order for quartiles to be meaningful.
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 99)
    for i in range(10):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, i % 5)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaInterquartileRange(
        mixin_stats, 'mixin_name', 1.5, 0)
    expected_bad_machines = {
        'bad-bot': [('Failure rate of 0.99 is above the IQR-based upper bound '
                     'of 0.07250000000000001.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSeveralBadMachinesLargeFleet(self):
    """Tests behavior when there are several bad machines in a large fleet."""
    mixin_stats = tasks.MixinStats()
    for i in range(98):
      mixin_stats.AddStatsForBotAndSuite(f'bot-{i}', 'suite', 100, i % 5)
    mixin_stats.AddStatsForBotAndSuite('bot-98', 'suite', 100, 15)
    mixin_stats.AddStatsForBotAndSuite('bot-99', 'suite', 100, 20)
    mixin_stats.AddStatsForBotAndSuite('bot-100', 'suite', 100, 50)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaInterquartileRange(
        mixin_stats, 'mixin_name', 1.5, 0)
    expected_bad_machines = {
        'bot-98': [('Failure rate of 0.15 is above the IQR-based upper bound '
                    'of 0.06.')],
        'bot-99': [('Failure rate of 0.2 is above the IQR-based upper bound '
                    'of 0.06.')],
        'bot-100': [('Failure rate of 0.5 is above the IQR-based upper bound '
                     'of 0.06.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSmallFlakyFleet(self):
    """Tests behavior when there's a bad machine in a small, flaky fleet."""
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 50)
    for i in range(9):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100,
                                         25 + i % 5)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaInterquartileRange(
        mixin_stats, 'mixin_name', 1.5, 0)
    expected_bad_machines = {
        'bad-bot': [('Failure rate of 0.5 is above the IQR-based upper bound '
                     'of 0.31000000000000005.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testLowFailedTaskMachinesSkipped(self):
    """Tests that machines w/ low failed task counts are skipped."""
    mixin_stats = tasks.MixinStats()
    for i in range(98):
      mixin_stats.AddStatsForBotAndSuite(f'bot-{i}', 'suite', 100, i % 3)
    mixin_stats.AddStatsForBotAndSuite('bot-98', 'suite', 10, 4)
    mixin_stats.AddStatsForBotAndSuite('bot-99', 'suite', 100, 50)
    mixin_stats.Freeze()

    with self.assertLogs(level='DEBUG') as log_manager:
      bad_machine_list = detection.DetectViaInterquartileRange(
          mixin_stats, 'mixin_name', 1.5, 5)
      for line in log_manager.output:
        if ('Bot bot-98 skipped in DetectViaInterquartileRange due to only '
            'having 4 failed tasks') in line:
          break
      else:
        self.fail('Did not find expected log line')

    expected_bad_machines = {
        'bot-99': [('Failure rate of 0.5 is above the IQR-based upper bound of '
                    '0.05.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)


class IndependentEventHelpersUnittest(unittest.TestCase):

  def testChanceOfExactlyNIndependentEvents(self):
    """Tests behavior of the N independent events helper."""
    # pylint: disable=protected-access
    func = detection._ChanceOfExactlyNIndependentEvents
    # pylint: enable=protected-access

    # Equivalent to flipping a coin and getting heads.
    self.assertEqual(func(decimal.Decimal(0.5), 1, 1), decimal.Decimal(0.5))

    # Equivalent to flipping two coins and getting zero heads.
    self.assertEqual(func(decimal.Decimal(0.5), 2, 0), decimal.Decimal(0.25))

    # Make sure that this is special-cased since Decimal(0)**0 is an error.
    self.assertEqual(func(decimal.Decimal(0), 2, 1), decimal.Decimal(0))
    self.assertEqual(func(decimal.Decimal(0), 2, 0), decimal.Decimal(1))

  def testChanceOfNOrMoreIndependentEvents(self):
    """Tests behavior of the N+ independent events helper."""
    # pylint: disable=protected-access
    func = detection._ChanceOfNOrMoreIndependentEvents
    # pylint: enable=protected-access

    # Probability of getting 0 or more should always be 1.
    self.assertEqual(func(decimal.Decimal(0.5), 10, 0), 1)

    # Equivalent to flipping two coins and getting at least one heads.
    self.assertEqual(func(decimal.Decimal(0.5), 2, 1), 0.75)
