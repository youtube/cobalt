# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import struct
from recipe_engine import recipe_test_api


class BotUpdateTestApi(recipe_test_api.RecipeTestApi):
  def output_json(self, root, first_sln, revision_mapping, fail_patch=False,
                  fixed_revisions=None, commit_positions=True):
    """Deterministically synthesize json.output test data for gclient's
    --output-json option.
    """
    output = {
        'did_run': True,
        'patch_failure': False
    }

    revisions = {
        project_name: self.gen_revision(project_name)
        for project_name in set(revision_mapping.values())
    }
    if fixed_revisions:
      for project_name, revision in fixed_revisions.items():
        if revision == 'HEAD':
          revision = self.gen_revision(project_name)
        elif revision.startswith('refs/') or revision.startswith('origin/'):
          revision = self.gen_revision('{}@{}'.format(project_name, revision))
        revisions[project_name] = revision

    properties = {
        property_name: revisions[project_name]
        for property_name, project_name in revision_mapping.items()
    }
    properties.setdefault('got_revision', self.gen_revision(first_sln))
    if commit_positions:

      def get_ref(project_name):
        revision = fixed_revisions.get(project_name, 'HEAD')
        if revision.startswith('origin/'):
          return revision.replace('origin/', 'refs/heads/', 1)
        if revision.startswith('refs/'):
          return revision
        return 'refs/heads/main'

      properties.update({
          '%s_cp' % property_name:
          ('%s@{#%s}' %
           (get_ref(project_name), self.gen_commit_position(project_name)))
          for property_name, project_name in revision_mapping.items()
      })

    output.update({
        'patch_root': root or first_sln,
        'root': first_sln,
        'properties': properties,
        'step_text': 'Some step text'
    })

    output.update({
        'manifest': {
            project_name: {
                'repository': 'https://fake.org/%s.git' % project_name,
                'revision': revision,
            }
            for project_name, revision in revisions.items()
        }
    })

    output.update({
        'source_manifest': {
            'version': 0,
            'directories': {
                project_name: {
                    'git_checkout': {
                        'repo_url': 'https://fake.org/%s.git' % project_name,
                        'revision': revision
                    }
                }
                for project_name, revision in revisions.items()
            }
        }
    })

    if fixed_revisions:
      output['fixed_revisions'] = fixed_revisions

    if fail_patch:
      output['patch_failure'] = True
      output['failed_patch_body'] = '\n'.join([
        'Downloading patch...',
        'Applying the patch...',
        'Patch: foo/bar.py',
        'Index: foo/bar.py',
        'diff --git a/foo/bar.py b/foo/bar.py',
        'index HASH..HASH MODE',
        '--- a/foo/bar.py',
        '+++ b/foo/bar.py',
        'context',
        '+something',
        '-something',
        'more context',
      ])
      output['patch_apply_return_code'] = 1
      if fail_patch == 'download':
        output['patch_apply_return_code'] = 3
    return self.m.json.output(output)

  @staticmethod
  def gen_revision(project):
    """Hash project to bogus deterministic git hash values."""
    h = hashlib.sha1(project.encode('utf-8'))
    return h.hexdigest()

  @staticmethod
  def gen_commit_position(project):
    """Hash project to bogus deterministic Cr-Commit-Position values."""
    h = hashlib.sha1(project.encode('utf-8'))
    return struct.unpack('!I', h.digest()[:4])[0] % 300000
