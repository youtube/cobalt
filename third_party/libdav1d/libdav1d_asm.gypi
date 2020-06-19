# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

{
  'conditions': [
    ['target_arch == "x64" or target_arch == "x86"', {
      'variables': {
        'DAV1D_IS_PIC': '1',
      },
      'conditions': [
        ['target_arch == "x64"', {
          'variables': {
            'DAV1D_ARCH_X86_32': '0',
            'DAV1D_ARCH_X86_64': '1',
          }
        }, {
          'variables': {
            'DAV1D_ARCH_X86_32': '1',
            'DAV1D_ARCH_X86_64': '0',
          }
        }]
      ],
    }],
    ['target_os == "linux"', {
      'variables': {
        'DAV1D_STACK_ALIGNMENT': '32',
      },
    }],
    ['target_os == "win"', {
      'conditions': [
        ['target_arch == "x64"', {
          'variables': {
            'DAV1D_STACK_ALIGNMENT': '16',
          }
        }, {
          'variables': {
            'DAV1D_STACK_ALIGNMENT': '4',
          }
        }]
      ],
    }],
  ],
  'rules': [{
    'rule_name': 'assemble',
    'extension': 'asm',
    'inputs': [],
    'outputs': [
      '<(PRODUCT_DIR)/obj/third_party/libdav1d/<(RULE_INPUT_ROOT).asm.o',
    ],

    'conditions': [
      ['target_os == "win"', {
        'variables': {
          'NASM_EXECUTABLE': 'nasm.exe',
          'NASM_OUTPUT_FORMAT' : 'win',
        },
      }],
      ['target_os == "linux"', {
        'variables': {
          'NASM_EXECUTABLE': '/usr/bin/nasm',
          'NASM_OUTPUT_FORMAT': 'elf',
        },
      }],
      ['target_arch == "x86"', {
        'variables': {
          'NASM_OUTPUT_SIZE': '32'
        },
      }, {
        'variables': {
          'NASM_OUTPUT_SIZE': '64'
        },
      }],
    ],

    'action': [
      '<(NASM_EXECUTABLE)',
      '-f<(NASM_OUTPUT_FORMAT)<(NASM_OUTPUT_SIZE)',
      '-DSTACK_ALIGNMENT=<(DAV1D_STACK_ALIGNMENT)',
      '-DARCH_X86_32=<(DAV1D_ARCH_X86_32)',
      '-DARCH_X86_64=<(DAV1D_ARCH_X86_64)',
      '-DPIC=<(DAV1D_IS_PIC)',
      '-I<(DEPTH)/third_party/libdav1d/include',
      '-I<(DEPTH)/third_party/libdav1d/src',
      '-MQ<(PRODUCT_DIR)/$out',
      '-MF<(PRODUCT_DIR)/$out.ndep',
      '-o<(PRODUCT_DIR)/$out',
      '<(RULE_INPUT_PATH)',
    ],
    'process_outputs_as_sources': 1,
    'message': 'Building <(RULE_INPUT_ROOT).asm.o',
  }],
}
