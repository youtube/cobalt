#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Migrate local repository onto new default branch."""

import fix_encoding
import gerrit_util
import git_common
import metrics
import scm
import sys
import logging
from six.moves import urllib


def GetGerritProject(remote_url):
  """Returns Gerrit project name based on remote git URL."""
  if remote_url is None:
    raise RuntimeError('can\'t detect Gerrit project.')
  project = urllib.parse.urlparse(remote_url).path.strip('/')
  if project.endswith('.git'):
    project = project[:-len('.git')]
  # *.googlesource.com hosts ensure that Git/Gerrit projects don't start with
  # 'a/' prefix, because 'a/' prefix is used to force authentication in
  # gitiles/git-over-https protocol. E.g.,
  # https://chromium.googlesource.com/a/v8/v8 refers to the same repo/project
  # as
  # https://chromium.googlesource.com/v8/v8
  if project.startswith('a/'):
    project = project[len('a/'):]
  return project


def GetGerritHost(git_host):
  parts = git_host.split('.')
  parts[0] = parts[0] + '-review'
  return '.'.join(parts)


def main():
  remote = git_common.run('remote')
  # Use first remote as source of truth
  remote = remote.split("\n")[0]
  if not remote:
    raise RuntimeError('Could not find any remote')
  url = scm.GIT.GetConfig(git_common.repo_root(), 'remote.%s.url' % remote)
  host = urllib.parse.urlparse(url).netloc
  if not host:
    raise RuntimeError('Could not find remote host')

  project_head = gerrit_util.GetProjectHead(GetGerritHost(host),
                                            GetGerritProject(url))
  if project_head != 'refs/heads/main':
    raise RuntimeError("The repository is not migrated yet.")

  # User may have set to fetch only old default branch. Ensure fetch is tracking
  # main too.
  git_common.run('config', '--unset-all',
                 'remote.origin.fetch', 'refs/heads/*')
  git_common.run('config', '--add',
                 'remote.origin.fetch', '+refs/heads/*:refs/remotes/origin/*')
  logging.info("Running fetch...")
  git_common.run('fetch', remote)
  logging.info("Updating remote HEAD...")
  git_common.run('remote', 'set-head', '-a', remote)

  branches = git_common.get_branches_info(True)

  if 'master' in branches:
    logging.info("Migrating master branch...")
    if 'main' in branches:
      logging.info('You already have master and main branch, consider removing '
                   'master manually:\n'
                   ' $ git branch -d master\n')
    else:
      git_common.run('branch', '-m', 'master', 'main')
    branches = git_common.get_branches_info(True)

  for name in branches:
    branch = branches[name]
    if not branch:
      continue

    if 'master' in branch.upstream:
      logging.info("Migrating %s branch..." % name)
      new_upstream = branch.upstream.replace('master', 'main')
      git_common.run('branch', '--set-upstream-to', new_upstream, name)
      git_common.remove_merge_base(name)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  logging.basicConfig(level=logging.INFO)
  with metrics.collector.print_notice_and_exit():
    try:
      logging.info("Starting migration")
      main()
      logging.info("Migration completed")
    except RuntimeError as e:
      logging.error("Error %s" % str(e))
      sys.exit(1)
