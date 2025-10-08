#!/usr/bin/env python3

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to generate build.gradle from template and run fetch_all.py

More specifically, to generate build.gradle:
  - It downloads the index page for the latest androidx snapshot from
    https://androidx.dev/snapshots/latest/artifacts
  - It replaces {{androidx_repository_url}} with the URL for the latest snapshot
  - For each dependency, it looks up the version in the index page's HTML and
    substitutes the version into {{androidx_dependency_version}}.

Extra args after -- are passed directly to fetch_all.py.
"""

import argparse
import contextlib
import json
import logging
import os
import pathlib
import re
import shutil
import sys
import subprocess
import tempfile
from urllib import request
import zipfile

_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]
_ANDROIDX_PATH = _SRC_PATH / 'third_party/androidx'
_CIPD_PATH = _ANDROIDX_PATH / 'cipd'
_BOM_NAME = 'bill_of_materials.json'
_EXTRACT_SCRIPT_PATH = _ANDROIDX_PATH / 'extract_and_commit_extras.py'

sys.path.insert(1, str(_SRC_PATH / 'build/autoroll'))
import fetch_util

# URL to artifacts in latest androidx snapshot.
_ANDROIDX_LATEST_SNAPSHOT_ARTIFACTS_URL = 'https://androidx.dev/snapshots/latest/artifacts'

# When androidx roller is breaking, and a fix is not imminent, use this to pin a
# broken library to an old known-working version.
# * Find working versions from prior androidx roll commit descriptions.
# * The first element of each tuple is the path to the artifact of the latest
#   version of the library. It could change if the version is rev'ed in a new
#   snapshot.
# * The second element is a URL to replace the file with. Find the URL for older
#   versions of libraries by looking through the artifacts for the older version
#   (e.g.: https://androidx.dev/snapshots/builds/8545498/artifacts)
_OVERRIDES = [
    # Example:
    #('androidx_core_core/core-1.9.0-SNAPSHOT.aar',
    # 'https://androidx.dev/snapshots/builds/8545498/artifacts/repository/'
    # 'androidx/core/core/1.8.0-SNAPSHOT/core-1.8.0-20220505.122105-1.aar'),
<<<<<<< HEAD
]

# Set this to the build_id to pin all libraries to a given version.
# Useful when pinning a single library would cause issues, but you do not want
# to pause the auto-roller because other teams want to add / remove libraries.
# Example: '8545498'
_LATEST_VERSION_OVERRIDE = ''


_FILES_TO_COMMIT = [
    'additional_readme_paths.json',
    'bill_of_materials.json',
    'BUILD.gn',
    'build.gradle',
]


def _get_latest_androidx_version():
    androidx_artifacts_response = request.urlopen(
        _ANDROIDX_LATEST_SNAPSHOT_ARTIFACTS_URL)
    # Get the versioned url from the redirect destination.
    androidx_artifacts_url = androidx_artifacts_response.url
    androidx_artifacts_response.close()
    logging.info('URL for the latest build info: %s', androidx_artifacts_url)
    # Strip '/repository' from pattern.
    resolved_snapshot_repository_url_pattern = (
        fetch_util.make_androidx_maven_url('([0-9]*)').rsplit('/', 1)[0])
    match = re.match(resolved_snapshot_repository_url_pattern,
                     androidx_artifacts_url)
    assert match is not None
    version = match.group(1)
    logging.info('Resolved latest androidx version to %s', version)
    return version
=======
    ('androidx_recyclerview_recyclerview/recyclerview-1.4.0-SNAPSHOT.aar',
     'https://androidx.dev/snapshots/builds/9668027/artifacts/repository/'
     'androidx/recyclerview/recyclerview/1.4.0-SNAPSHOT/'
     'recyclerview-1.4.0-20230228.234124-1.aar'),
]


def _build_snapshot_repository_url(version):
    return _SNAPSHOT_REPOSITORY_URL.replace('{{version}}', version)


def _delete_readonly_files(paths):
    for path in paths:
        if os.path.exists(path):
            os.chmod(path, stat.S_IRUSR | stat.S_IRGRP | stat.S_IWUSR)
            os.remove(path)


def _parse_dir_list(dir_list):
    """Computes 'library_group:library_name'->library_version mapping.

    Args:
      dir_list: List of androidx library directories.
    """
    dependency_version_map = dict()
    for dir_entry in dir_list:
        stripped_dir = dir_entry.strip()
        if not stripped_dir.startswith('repository/androidx/'):
            continue
        dir_components = stripped_dir.split('/')
        # Expected format:
        # "repository/androidx/library_group/library_name/library_version/pom_or_jar"
        if len(dir_components) < 6:
            continue
        dependency_package = 'androidx.' + '.'.join(dir_components[2:-3])
        dependency_module = '{}:{}'.format(dependency_package,
                                           dir_components[-3])
        if dependency_module not in dependency_version_map:
            dependency_version_map[dependency_module] = dir_components[-2]
    return dependency_version_map


def _compute_replacement(dependency_version_map, androidx_repository_url,
                         line):
    """Computes output line for build.gradle from build.gradle.template line.

    Replaces {{android_repository_url}}, {{androidx_dependency_version}} and
      {{version_overrides}}.

    Args:
      dependency_version_map: An "dependency_group:dependency_name"->dependency_version mapping.
      androidx_repository_url: URL of the maven repository.
      line: Input line from the build.gradle.template.
    """
    line = line.replace('{{androidx_repository_url}}', androidx_repository_url)

    if line.strip() == '{{version_overrides}}':
        lines = ['versionOverrideMap = [:]']
        for dependency, version in sorted(dependency_version_map.items()):
            lines.append(f"versionOverrideMap['{dependency}'] = '{version}'")
        return '\n'.join(lines)

    match = re.search(r'\'(\S+):{{androidx_dependency_version}}\'', line)
    if not match:
        return line

    dependency = match.group(1)
    version = dependency_version_map.get(dependency)
    if not version:
        raise Exception(f'Version for {dependency} not found.')

    return line.replace('{{androidx_dependency_version}}', version)


@contextlib.contextmanager
def _build_dir():
    dirname = tempfile.mkdtemp()
    try:
        yield dirname
    finally:
        shutil.rmtree(dirname)


def _download_and_parse_build_info():
    """Downloads and parses BUILD_INFO file."""
    with _build_dir() as build_dir:
        androidx_build_info_response = request.urlopen(
            _ANDROIDX_LATEST_SNAPSHOT_BUILD_INFO_URL)
        androidx_build_info_url = androidx_build_info_response.geturl()
        logging.info('URL for the latest build info: %s',
                     androidx_build_info_url)
        androidx_build_info_path = os.path.join(build_dir, 'BUILD_INFO')
        with open(androidx_build_info_path, 'w') as f:
            f.write(androidx_build_info_response.read().decode('utf-8'))

        # Strip '/repository' from pattern.
        resolved_snapshot_repository_url_pattern = (
            _build_snapshot_repository_url('([0-9]*)').rsplit('/', 1)[0])

        version = re.match(resolved_snapshot_repository_url_pattern,
                           androidx_build_info_url).group(1)

        with open(androidx_build_info_path, 'r') as f:
            build_info_dict = json.loads(f.read())
        dir_list = build_info_dict['target']['dir_list']

        return dir_list, version


def _create_local_dir_list(repo_path):
    repo_path = repo_path.rstrip('/')
    prefix_len = len(repo_path) + 1
    ret = []
    for dirpath, _, filenames in os.walk(repo_path):
        for name in filenames:
            ret.append(os.path.join('repository', dirpath[prefix_len:], name))
    return ret


def _process_build_gradle(dependency_version_map, androidx_repository_url):
    """Generates build.gradle from template.

    Args:
      dependency_version_map: An "dependency_group:dependency_name"->dependency_version mapping.
      androidx_repository_url: URL of the maven repository.
    """
    build_gradle_template_path = os.path.join(_ANDROIDX_PATH,
                                              'build.gradle.template')
    build_gradle_out_path = os.path.join(_ANDROIDX_PATH, 'build.gradle')
    # |build_gradle_out_path| is not deleted after script has finished running. The file is in
    # .gitignore and thus will be excluded from uploaded CLs.
    with open(build_gradle_template_path, 'r') as template_f, \
        open(build_gradle_out_path, 'w') as out:
        for template_line in template_f:
            replacement = _compute_replacement(dependency_version_map,
                                               androidx_repository_url,
                                               template_line)
            out.write(replacement)


def _write_cipd_yaml(libs_dir, version, cipd_yaml_path, experimental=False):
    """Writes cipd.yaml file at the passed-in path."""

    lib_dirs = os.listdir(libs_dir)
    if not lib_dirs:
        raise Exception('No generated libraries in {}'.format(libs_dir))

    data_files = [
        'BUILD.gn', 'VERSION.txt', 'additional_readme_paths.json',
        'build.gradle'
    ]
    for lib_dir in lib_dirs:
        abs_lib_dir = os.path.join(libs_dir, lib_dir)
        androidx_rel_lib_dir = os.path.relpath(abs_lib_dir, _ANDROIDX_PATH)
        if not os.path.isdir(abs_lib_dir):
            continue
        lib_files = os.listdir(abs_lib_dir)
        if not 'cipd.yaml' in lib_files:
            continue

        for lib_file in lib_files:
            if lib_file == 'cipd.yaml' or lib_file == 'OWNERS':
                continue
            data_files.append(os.path.join(androidx_rel_lib_dir, lib_file))

    if experimental:
        package = 'experimental/google.com/' + os.getlogin() + '/androidx'
    else:
        package = 'chromium/third_party/androidx'
    contents = [
        '# Copyright 2021 The Chromium Authors',
        '# Use of this source code is governed by a BSD-style license that can be',
        '# found in the LICENSE file.',
        '# version: ' + version,
        'package: ' + package,
        'description: androidx',
        'data:',
    ]
    contents.extend('- file: ' + f for f in data_files)

    with open(cipd_yaml_path, 'w') as out:
        out.write('\n'.join(contents))
>>>>>>> e6037d01020 (Add ExoPlayer media3 libraries to //third_party/androidx (#7074))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-v',
                        '--verbose',
                        dest='verbose_count',
                        default=0,
                        action='count',
                        help='Verbose level (multiple times for more)')
    parser.add_argument('--local-repo',
                        help='Path to a locally androidx maven repo to use '
                        'instead of fetching the latest.')
    parser.add_argument(
        '--local',
        action='store_true',
        help='If passed then we will run the extract_and_commit_extras.py '
        'script and will not try rolling to the latest snapshot but reprocess '
        'the project at the current androidx.dev snapshot.')
    parser.add_argument(
        '--use-bom',
        action='store_true',
        help='If passed then we will use the existing bill_of_materials.json '
        'instead of resolving the latest androidx (faster but might resolve '
        'incorrect versions if deps are added/removed).')
    args, extra_args = parser.parse_known_args()

    logging.basicConfig(
        level=logging.WARNING - 10 * args.verbose_count,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')

    if args.local_repo:
        version = 'local'
        androidx_snapshot_repository_url = ('file://' +
                                            os.path.abspath(args.local_repo))
    else:
        if _LATEST_VERSION_OVERRIDE:
            version = _LATEST_VERSION_OVERRIDE
        elif args.local:
            version = fetch_util.get_current_androidx_version()
        else:
            version = _get_latest_androidx_version()

        androidx_snapshot_repository_url = (
            fetch_util.make_androidx_maven_url(version))
        # Prepend '0' to version to avoid conflicts with previous version format.
        version = 'cr-0' + version

    if args.use_bom:
        version_map_str = fetch_util.generate_version_map_str(_ANDROIDX_PATH /
                                                              _BOM_NAME)
    else:
        version_map_str = ''

    fetch_util.fill_template(
        _ANDROIDX_PATH / 'build.gradle.template',
        _ANDROIDX_PATH / 'build.gradle',
        version_overrides=version_map_str,
        androidx_repository_url=androidx_snapshot_repository_url)

    # Overrides do not work with local snapshots since the repository_url is
    # different.
    if not args.local_repo:
        for subpath, url in _OVERRIDES:
            extra_args += ['--override-artifact', f'{subpath}:{url}']

    os.makedirs(_CIPD_PATH, exist_ok=True)
    # gclient/cipd extract files as read only.
    subprocess.run(['chmod', '-R', '+w', _CIPD_PATH])

    fetch_util.run_fetch_all(android_deps_dir=_ANDROIDX_PATH,
                             output_subdir='cipd',
                             extra_args=extra_args,
                             verbose_count=args.verbose_count)

    version_map_str = fetch_util.generate_version_map_str(_CIPD_PATH /
                                                          _BOM_NAME)

    # Regenerate the build.gradle file filling in the the version map so that
    # runs of the main project do not have to revalutate androidx versions.
    fetch_util.fill_template(
        _ANDROIDX_PATH / 'build.gradle.template',
        _CIPD_PATH / 'build.gradle',
        version_overrides=version_map_str,
        androidx_repository_url=androidx_snapshot_repository_url)

    version_txt_path = os.path.join(_CIPD_PATH, 'VERSION.txt')
    with open(version_txt_path, 'w') as f:
        f.write(version)

    to_commit_zip_path = _CIPD_PATH / 'to_commit.zip'
    file_map = {f: f'third_party/androidx/{f}' for f in _FILES_TO_COMMIT}
    fetch_util.create_to_commit_zip(output_path=to_commit_zip_path,
                                    package_root=_CIPD_PATH,
                                    dirnames=['libs'],
                                    absolute_file_map=file_map)
    if args.local:
        subprocess.run([
            _EXTRACT_SCRIPT_PATH, '--cipd-package-path', _CIPD_PATH,
            '--no-git-add'
        ],
                       check=True)

    fetch_util.write_cipd_yaml(package_root=_CIPD_PATH,
                               package_name=fetch_util.ANDROIDX_CIPD_PACKAGE,
                               version=version,
                               output_path=_CIPD_PATH / 'cipd.yaml')

if __name__ == '__main__':
    main()
