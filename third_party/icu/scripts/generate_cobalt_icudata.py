#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Generates Cobalt's filtered ICU data bundle (icudtl.dat) at build time."""

import argparse
import concurrent.futures
import os
import pathlib
import platform
import shutil
import subprocess
import sys

_ICU_TOOLS = (
    'gencnval',
    'gencfu',
    'makeconv',
    'genbrk',
    'gensprep',
    'gendict',
    'icupkg',
    'genrb',
    'gencmn',
)


def _get_host_toolchain(icu_root: pathlib.Path):
  """Returns (cc, cxx, ar, cflags, ldflags) for building host ICU CLI tools."""
  src_root = icu_root.resolve().parents[1]
  llvm_bin = src_root / 'third_party' / 'llvm-build' / 'Release+Asserts' / 'bin'
  clang_bin = llvm_bin / 'clang'
  clangxx_bin = llvm_bin / 'clang++'
  llvm_ar_bin = llvm_bin / 'llvm-ar'

  if platform.system() == 'Darwin':
    cc = subprocess.check_output(
        ['xcrun', '--sdk', 'macosx', '--find', 'clang'], text=True).strip()
    cxx = subprocess.check_output(
        ['xcrun', '--sdk', 'macosx', '--find', 'clang++'], text=True).strip()
    ar = subprocess.check_output(['xcrun', '--sdk', 'macosx', '--find', 'ar'],
                                 text=True).strip()
    sdk_path = subprocess.check_output(
        ['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()
    flags = ['-isysroot', sdk_path]
    return cc, cxx, ar, flags, flags

  sysroot = src_root / 'build' / 'linux' / 'debian_bullseye_amd64-sysroot'
  if clangxx_bin.is_file() and sysroot.is_dir():
    ar = str(llvm_ar_bin) if llvm_ar_bin.is_file() else 'ar'
    cflags = [f'--sysroot={sysroot}']
    ldflags = [f'--sysroot={sysroot}', '-fuse-ld=lld']
    return str(clang_bin), str(clangxx_bin), ar, cflags, ldflags

  cc = shutil.which('clang') or shutil.which('gcc') or 'cc'
  cxx = shutil.which('clang++') or shutil.which('g++') or 'c++'
  ar = shutil.which('llvm-ar') or shutil.which('ar') or 'ar'
  return cc, cxx, ar, [], []


def _build_host_tools(icu_src: pathlib.Path, build_dir: pathlib.Path,
                      num_cores: int) -> pathlib.Path:
  """Compiles and links the 9 ICU host CLI tools into build_dir/bin."""
  bin_dir = build_dir / 'bin'
  if all((bin_dir / tool).is_file() for tool in _ICU_TOOLS):
    return bin_dir

  obj_dir = build_dir / 'obj'
  obj_dir.mkdir(parents=True, exist_ok=True)
  bin_dir.mkdir(parents=True, exist_ok=True)

  cc, cxx, ar, host_cflags, host_ldflags = _get_host_toolchain(icu_src.parent)
  common_flags = host_cflags + [
      '-O2',
      '-w',
      '-DU_STATIC_IMPLEMENTATION',
      '-DU_ATTRIBUTE_DEPRECATED=',
      '-DU_COMMON_IMPLEMENTATION',
      '-DU_I18N_IMPLEMENTATION',
      '-DU_TOOLUTIL_IMPLEMENTATION',
      f"-I{icu_src / 'common'}",
      f"-I{icu_src / 'i18n'}",
      f"-I{icu_src / 'tools' / 'toolutil'}",
  ]

  lib_sources = []
  for sub in ('stubdata', 'common', 'i18n', 'tools/toolutil'):
    sub_dir = icu_src / sub
    lib_sources.extend(sorted(sub_dir.glob('*.c')))
    lib_sources.extend(sorted(sub_dir.glob('*.cpp')))

  def compile_source(src_path: pathlib.Path, extra_includes=()):
    rel = '_'.join(src_path.relative_to(icu_src).parts)
    obj_path = obj_dir / f'{rel}.o'
    if obj_path.is_file():
      return str(obj_path)
    is_cpp = src_path.suffix == '.cpp'
    lang_flags = (['-std=c++17']
                  if is_cpp else ['-std=gnu11', '-Dchar16_t=uint16_t'])
    cmd = ([cxx if is_cpp else cc] + common_flags + list(extra_includes) +
           lang_flags +
           ['-c', str(src_path), '-o', str(obj_path)])
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if res.returncode != 0:
      raise RuntimeError(f'Failed to compile {src_path}:\n{res.stderr}')
    return str(obj_path)

  with concurrent.futures.ThreadPoolExecutor(max_workers=num_cores) as executor:
    lib_objs = list(executor.map(compile_source, lib_sources))

  lib_a = build_dir / 'libicu_host.a'
  lib_a.unlink(missing_ok=True)
  subprocess.run([ar, 'rcs', str(lib_a)] + lib_objs, check=True)

  def build_tool(tool: str):
    tool_dir = icu_src / 'tools' / tool
    tool_srcs = [
        s for s in sorted(
            list(tool_dir.glob('*.c')) + list(tool_dir.glob('*.cpp')))
        if s.name != 'derb.cpp'
    ]
    tool_objs = [
        compile_source(s, extra_includes=(f'-I{tool_dir}',)) for s in tool_srcs
    ]
    out_bin = bin_dir / tool
    link_cmd = ([cxx] + host_ldflags + ['-o', str(out_bin)] + tool_objs +
                [str(lib_a), '-ldl', '-lpthread', '-lm'])
    res = subprocess.run(link_cmd, capture_output=True, text=True, check=False)
    if res.returncode != 0:
      raise RuntimeError(f'Failed to link {tool}:\n{res.stderr}')
    return out_bin

  with concurrent.futures.ThreadPoolExecutor(
      max_workers=len(_ICU_TOOLS)) as executor:
    list(executor.map(build_tool, _ICU_TOOLS))

  return bin_dir


def _build_filtered_icudata(icu_src: pathlib.Path, filter_file: pathlib.Path,
                            bin_dir: pathlib.Path,
                            build_dir: pathlib.Path) -> pathlib.Path:
  """Runs icutools.databuilder and gencmn to generate filtered icudt74l.dat."""
  out_build_dir = build_dir / 'data_out' / 'icudt74l'
  tmp_dir = build_dir / 'data_tmp'
  shutil.rmtree(out_build_dir, ignore_errors=True)
  shutil.rmtree(tmp_dir, ignore_errors=True)
  out_build_dir.mkdir(parents=True, exist_ok=True)
  tmp_dir.mkdir(parents=True, exist_ok=True)

  env = os.environ.copy()
  env['PYTHONPATH'] = os.pathsep.join([
      str(icu_src / 'python'),
      str(icu_src / 'data'),
  ])
  subprocess.run(
      [
          sys.executable,
          '-m',
          'icutools.databuilder',
          '--mode=unix-exec',
          f"--src_dir={icu_src / 'data'}",
          f'--filter_file={filter_file}',
          f'--out_dir={out_build_dir}',
          f'--tmp_dir={tmp_dir}',
          f'--tool_dir={bin_dir}',
      ],
      env=env,
      check=True,
  )
  subprocess.run(
      [
          str(bin_dir / 'gencmn'),
          '-c',
          '-e',
          'icudt74',
          '-n',
          'icudt74l',
          '-s',
          str(out_build_dir),
          '-t',
          'dat',
          '-d',
          str(tmp_dir),
          '0',
          str(tmp_dir / 'icudata.lst'),
      ],
      check=True,
  )
  return tmp_dir / 'icudt74l.dat'


def main():
  parser = argparse.ArgumentParser(
      description='Generate Cobalt icudtl.dat from filters/cobalt.json.')
  parser.add_argument(
      '--filter-file',
      required=True,
      help='Path to the ICU JSON filter file (e.g. filters/cobalt.json).')
  parser.add_argument('--out-file',
                      required=True,
                      help='Output path for generated icudtl.dat.')
  parser.add_argument(
      '--build-dir',
      required=True,
      help='Intermediate directory for caching ICU host tools and data build.')
  args = parser.parse_args()

  icu_root = pathlib.Path(__file__).resolve().parents[1]
  icu_src = icu_root / 'source'
  filter_file = pathlib.Path(args.filter_file).resolve()
  out_file = pathlib.Path(args.out_file).resolve()
  build_dir = pathlib.Path(args.build_dir).resolve()

  build_dir.mkdir(parents=True, exist_ok=True)
  out_file.parent.mkdir(parents=True, exist_ok=True)

  num_cores = os.cpu_count() or 4
  bin_dir = _build_host_tools(icu_src, build_dir, num_cores)
  generated_dat = _build_filtered_icudata(icu_src, filter_file, bin_dir,
                                          build_dir)

  tmp_out_file = out_file.with_name(f'{out_file.name}.tmp.{os.getpid()}')
  shutil.copyfile(generated_dat, tmp_out_file)
  tmp_out_file.replace(out_file)
  return 0


if __name__ == '__main__':
  sys.exit(main())
