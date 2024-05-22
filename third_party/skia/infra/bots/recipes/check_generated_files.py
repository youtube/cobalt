# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Recipe for the Skia PerCommit Housekeeper.

PYTHON_VERSION_COMPATIBILITY = "PY3"

DEPS = [
  'build',
  'infra',
  'recipe_engine/context',
  'recipe_engine/file',
  'recipe_engine/path',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/raw_io',
  'recipe_engine/step',
  'checkout',
  'run',
  'vars',
]


def RunSteps(api):
  # Checkout, compile, etc.
  api.vars.setup()
  checkout_root = api.checkout.default_checkout_root
  api.checkout.bot_update(checkout_root=checkout_root)
  api.file.ensure_directory('makedirs tmp_dir', api.vars.tmp_dir)

  cwd = api.path['checkout']

  with api.context(cwd=cwd):
    # Get a baseline diff. This should be empty, but we want to be flexible for
    # cases where we have local diffs on purpose.
    diff1 = api.run(
        api.step,
        'git diff #1',
        cmd=['git', 'diff', '--no-ext-diff'],
        stdout=api.m.raw_io.output()).stdout.decode('utf-8')

    with api.context(env=api.infra.go_env):
      api.step('generate gl interfaces',
               cmd=['make', '-C', 'tools/gpu/gl/interface', 'generate'])

    # Run GN, regenerate the SKSL files, and make sure rewritten #includes work.
    api.build(checkout_root=checkout_root,
              out_dir=api.vars.build_dir.join('out', 'Release'))

    # Get a second diff. If this doesn't match the first, then there have been
    # modifications to the generated files.
    diff2 = api.run(
        api.step,
        'git diff #2',
        cmd=['git', 'diff', '--no-ext-diff'],
        stdout=api.m.raw_io.output()).stdout.decode('utf-8')

    api.run(
        api.python.inline,
        'compare diffs',
        program="""
diff1 = '''%s'''

diff2 = '''%s'''

if diff1 != diff2:
  print('Generated files have been edited!')
  exit(1)
""" % (diff1, diff2))


def GenTests(api):
  yield (
      api.test('Housekeeper-PerCommit-CheckGeneratedFiles') +
      api.properties(buildername='Housekeeper-PerCommit-CheckGeneratedFiles',
                     repository='https://skia.googlesource.com/skia.git',
                     revision='abc123',
                     path_config='kitchen',
                     swarm_out_dir='[SWARM_OUT_DIR]') +
      api.path.exists(api.path['start_dir'])
  )
