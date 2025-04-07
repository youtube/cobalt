#!/usr/bin/env vpython3
""" A script to convert depot_tools DEPS managed dependency to
   a squashed Git subtree.
"""
import os
import sys
import argparse
import logging
import collections
import tempfile

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
        'depot_tools'))
# pylint: disable=wrong-import-position,import-error
import gclient_utils
import gclient_eval
import gclient_scm


def parse_depfile(deps_path, package):
  with open(deps_path, 'r', encoding='utf-8') as f:
    deps_content = f.read()

  depfile = gclient_eval.Parse(deps_content, deps_path)
  dep_deps = depfile['deps']
  select = dep_deps.get(package, None) or dep_deps.get('src/' + package, None)
  if not select:
    raise RuntimeError(f'Package {package} not found in {deps_path}')
  dep_type = select['dep_type']
  if dep_type != 'git':
    raise RuntimeError(
        f'Package {package} is not a git dependency ( type: {dep_type})')
  remote, rev = gclient_utils.SplitUrlRevision(select['url'])
  return remote, rev


def clone_dep(dest_dir, url, rev):
  Opts = collections.namedtuple('Options', [
      'verbose', 'revision', 'force', 'break_repo_locks', 'upstream',
      'delete_unversioned_trees', 'reset', 'merge'
  ])
  opts = Opts(
      verbose=True,
      revision=rev,
      force=True,
      break_repo_locks=True,
      upstream=None,
      delete_unversioned_trees=True,
      merge=False,
      reset=True)
  wrapper = gclient_scm.GitWrapper(url, root_dir=dest_dir, relpath='tmp')
  dict_files = []
  wrapper.update(opts, [], dict_files)
  # pylint: disable=protected-access
  wrapper._Run(['checkout', '-b', 'squash_insert'], opts)
  return wrapper.GetCheckoutRoot()


def main():
  temp_dir = os.path.join(tempfile.gettempdir(), 'cobalt_squash')
  parser = argparse.ArgumentParser(
      description='Convert DEPS managed dependency to a squashed subtree.')
  parser.add_argument(
      '--deps_file', default='DEPS', help='Path to the DEPS file to parse.')
  parser.add_argument(
      'package', help='Relative path to the package, e.g. `third_party/icu`.')
  parser.add_argument(
      '--staging_dir',
      default=temp_dir,
      help='Temporary directory for cloning the dependency.')
  args = parser.parse_args()
  if args.package.startswith('src/'):
    raise RuntimeError('Please remove src/ prefix')
  logging.basicConfig(stream=sys.stdout, level=logging.INFO)
  remote, rev = parse_depfile(args.deps_file, args.package)
  if not remote or not rev:
    raise RuntimeError('Could not parse dependency URL')
  cloned_repo = clone_dep(args.staging_dir, remote, rev)
  print(f"""
    # Remove the directory:
    rm -rf {args.package}
    # Add the squashed subtree, this creates a merge commit
    git subtree add --prefix {args.package} {cloned_repo} squash_insert --squash
    NB: Remember to edit DEPS file:
      - Add a comment above the original entry: # Cobalt: imported
      - Comment out the original entry for '{args.package}'
      - commit --amend this into the squash commit
    """)


if __name__ == '__main__':
  main()
