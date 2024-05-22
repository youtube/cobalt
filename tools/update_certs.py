#!/usr/bin/env python3
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""Tool for updating Cobalt SSL Certificates.

Usage:
  update_certs.py -r <cobalt_repo> -b <openssl_bin> -p <pem_files>

Args:
    cobalt_repo: The path to the Cobalt repository root.
    openssl_bin: The path to the OpenSSL binary, defaults to "/usr/bin/openssl"
      when not specified.
    pem_files: The paths to the source pem files.
"""

import argparse
import os
import shutil
import subprocess

from typing import List
from typing import Set

_CERTS_PATH = 'cobalt/content/ssl/certs'
_BUILD_PATH = 'cobalt/network/certs.gni'


def SplitCerts(concatenated_cert_file: str) -> List[str]:
  """Split concatenated .pem file into a list of individual certs."""
  begin_anchor = '-----BEGIN CERTIFICATE-----'
  end_anchor = '-----END CERTIFICATE-----'

  cert_list = []
  current_cert = ''
  in_cert = False

  for line in concatenated_cert_file:
    if not in_cert and line.find(begin_anchor) != -1:
      in_cert = True
      current_cert += line
    elif in_cert and line.find(end_anchor) != -1:
      in_cert = False
      current_cert += line
      cert_list.append(current_cert)
      current_cert = ''
    elif in_cert:
      current_cert += line

  return cert_list


def ComputeCertFilename(openssl_bin: str, cert: str) -> str:
  """Use x509 to get the hashed filename for the individual cert."""
  command_args = [openssl_bin, 'x509', '-subject_hash', '-noout']
  output = subprocess.check_output(command_args, input=cert.encode('utf-8'))

  return output.decode().rstrip() + '.0'


def UpdateBuild(build_file: str, cert_filepaths: Set[str]) -> None:
  cert_filepaths = list(cert_filepaths)
  cert_filepaths.sort()

  # Adds build file
  cert_lines = ['network_certs = [\n']
  cert_lines += [f'  "//{path}",\n' for path in cert_filepaths]
  cert_lines += [']\n']
  with open(build_file, 'w', encoding='utf-8') as f:
    f.writelines(cert_lines)


def UpdateCerts(certs_repo: str, openssl_bin: str, pem_files: str) -> None:
  certs_path = os.path.join(certs_repo, _CERTS_PATH)

  # Removes existing certs
  if os.path.exists(certs_path):
    shutil.rmtree(certs_path)
  os.mkdir(certs_path)

  # Adds certs
  relative_filepaths = set()
  for pem_file in pem_files:
    cert_list = []
    with open(pem_file, encoding='utf-8') as p:
      cert_list = SplitCerts(p)
    for cert in cert_list:
      filename = ComputeCertFilename(openssl_bin, cert)
      filepath = os.path.join(certs_path, filename)
      relative_filepath = os.path.join(_CERTS_PATH, filename)
      relative_filepaths.add(relative_filepath)
      with open(filepath, 'w', encoding='utf-8') as f:
        f.write(cert)

  # Updates certs.gni with updated certs
  build_file = os.path.join(certs_repo, _BUILD_PATH)
  UpdateBuild(build_file, relative_filepaths)


def main() -> None:
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-r',
      '--cobalt_repo',
      type=str,
      required=True,
      help='The path to the Cobalt repository root.')
  parser.add_argument(
      '-b',
      '--openssl_bin',
      type=str,
      required=False,
      default='/usr/bin/openssl',
      help='The path to the OpenSSL binary.')
  parser.add_argument(
      '-p',
      '--pem_files',
      type=str,
      nargs='+',
      required=True,
      help='The paths to the source pem files.')
  args = parser.parse_args()

  UpdateCerts(args.cobalt_repo, args.openssl_bin, args.pem_files)


if __name__ == '__main__':
  main()
