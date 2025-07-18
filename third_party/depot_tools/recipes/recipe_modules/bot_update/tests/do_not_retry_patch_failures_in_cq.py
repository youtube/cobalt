# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import post_process

PYTHON_VERSION_COMPATIBILITY = 'PY2+3'

DEPS = [
    'bot_update',
    'gclient',
    'recipe_engine/buildbucket',
    'recipe_engine/cq',
    'recipe_engine/properties',
    'recipe_engine/step',
]


def RunSteps(api):
  src_cfg = api.gclient.make_config()
  soln = src_cfg.solutions.add()
  soln.name = 'src'
  soln.url = 'https://chromium.googlesource.com/chromium/src.git'
  try:
    bot_update_step = api.bot_update.ensure_checkout(
        patch=True, gclient_config=src_cfg)
  except api.step.StepFailure:
    api.step(
        name='cq will not retry this'
        if api.cq.do_not_retry_build else 'will retry',
        cmd=None)


def GenTests(api):

  yield (api.test('works as intended') + api.buildbucket.try_build(
      'chromium/src',
      'try',
      'linux',
      git_repo='https://chromium.googlesource.com/chromium/src') +
         api.properties(fail_patch='apply') + api.step_data(
             'bot_update', retcode=88) + api.post_check(
                 lambda check, steps: check('cq will not retry this' in steps))
         + api.post_process(post_process.DropExpectation))
