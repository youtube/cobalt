# Copyright 2017 Google Inc. All Rights Reserved.
{
  'variables': {
    'asm_target_arch%': '<(target_arch)',
    'boringssl_root%': '<(DEPTH)/third_party/boringssl/src',
  },

  'target_defaults': {
    # Turn off bad-function-cast warning, as BoringSSL violates this sometimes.
    'defines': [
      'PURIFY',
      'TERMIO',
      '_REENTRANT',
      # We do not use TLS over UDP on Chromium so far.
      'OPENSSL_NO_DTLS1',
    ],
    'cflags': [
      '-fPIC',
      '-fvisibility=hidden',
    ],
    'cflags_c': [
      '-std=gnu99',
      '-Wno-bad-function-cast',
    ],
    'cflags_c!': [
      '-Wbad-function-cast',
    ],

    'ldflags': [
      '-fPIC',
    ],

    'type': '<(library)',

    'conditions': [
      ['OS=="starboard"', {
        'variables': {
          'openssl_config_path': '<(boringssl_root)/config/starboard',
        },
        'dependencies': [
          '<(DEPTH)/starboard/client_porting/eztime/eztime.gyp:eztime',
          '<(DEPTH)/starboard/starboard_headers_only.gyp:starboard_headers_only',
        ],
        'sources': [
          '<(boringssl_root)/crypto/rand_extra/starboard.c',
          '<(boringssl_root)/crypto/cpu-starboard.c',
        ],
        'sources!': [
          '<(boringssl_root)/crypto/bio/connect.c',
          '<(boringssl_root)/crypto/bio/fd.c',
          '<(boringssl_root)/crypto/bio/socket.c',
          '<(boringssl_root)/crypto/bio/socket_helper.c',
          '<(boringssl_root)/crypto/rand_extra/deterministic.c',
          '<(boringssl_root)/crypto/rand_extra/fuchsia.c',
          '<(boringssl_root)/crypto/rand_extra/windows.c',
        ],
      }],
      ['OS=="lb_shell"', {
        'variables': {
          'openssl_config_path': 'config/lbshell',
        },
      }],
      ['OS=="lb_shell" or OS=="starboard"', {
        'defines': [
          'GETPID_IS_MEANINGLESS',
          'NO_SYS_PARAM_H',
          'NO_SYS_UN_H',
          'NO_SYSLOG',
          'NO_WINDOWS_BRAINDEATH',
          'OPENSSL_NO_BF',
          'OPENSSL_NO_BUF_FREELISTS',
          'OPENSSL_NO_CAMELLIA',
          'OPENSSL_NO_CAPIENG',
          'OPENSSL_NO_CAST',
          'OPENSSL_NO_CMS',
          'OPENSSL_NO_COMP',
          'OPENSSL_NO_DGRAM',  # Added by cobalt to remove unused UDP code.
          'OPENSSL_NO_DYNAMIC_ENGINE',
          'OPENSSL_NO_EC2M',
          'OPENSSL_NO_EC_NISTP_64_GCC_128',
          'OPENSSL_NO_ENGINE',
          'OPENSSL_NO_GMP',
          'OPENSSL_NO_GOST',
          'OPENSSL_NO_HEARTBEATS', # Workaround for CVE-2014-0160.
          'OPENSSL_NO_HW',
          'OPENSSL_NO_IDEA',
          'OPENSSL_NO_JPAKE',
          'OPENSSL_NO_KRB5',
          'OPENSSL_NO_MD2',
          'OPENSSL_NO_MD4',
          'OPENSSL_NO_MDC2',
          'OPENSSL_NO_OCB',
          'OPENSSL_NO_OCSP',
          'OPENSSL_NO_RC2',
          'OPENSSL_NO_RC4',
          'OPENSSL_NO_RC5',
          'OPENSSL_NO_RFC3779',
          'OPENSSL_NO_RIPEMD',
          'OPENSSL_NO_RMD160',
          'OPENSSL_NO_SCTP',
          'OPENSSL_NO_SEED',
          'OPENSSL_NO_SRP',
          'OPENSSL_NO_SSL2',
          'OPENSSL_NO_SSL3', # Address CVE-2014-3566
          'OPENSSL_NO_STATIC_ENGINE',
          'OPENSSL_NO_STORE',
          'OPENSSL_NO_SOCK',  # Added by Cobalt to remove unused socket code.
          'OPENSSL_NO_THREADS',  # Added by Cobalt to reduce overall threads count.
          'OPENSSL_NO_UI',  # Added by Cobalt to remove unused "UI" code.
          'OPENSSL_NO_UNIT_TEST',
          'OPENSSL_NO_WHIRLPOOL',
          'OPENSSL_USE_IPV6=0',
          'B_ENDIAN',
          'OPENSSL_NO_POSIX_IO',
        ],
        'sources!': [
          '<(boringssl_root)/crypto/bf/bf_cfb64.c',
          '<(boringssl_root)/crypto/bf/bf_ecb.c',
          '<(boringssl_root)/crypto/bf/bf_enc.c',
          '<(boringssl_root)/crypto/bf/bf_ofb64.c',
          '<(boringssl_root)/crypto/bf/bf_skey.c',
          '<(boringssl_root)/crypto/engine/eng_all.c',
          '<(boringssl_root)/crypto/engine/eng_cnf.c',
          '<(boringssl_root)/crypto/engine/eng_ctrl.c',
          '<(boringssl_root)/crypto/engine/eng_dyn.c',
          '<(boringssl_root)/crypto/engine/eng_err.c',
          '<(boringssl_root)/crypto/engine/eng_fat.c',
          '<(boringssl_root)/crypto/engine/eng_init.c',
          '<(boringssl_root)/crypto/engine/eng_lib.c',
          '<(boringssl_root)/crypto/engine/eng_list.c',
          '<(boringssl_root)/crypto/engine/eng_pkey.c',
          '<(boringssl_root)/crypto/engine/eng_table.c',
          '<(boringssl_root)/crypto/engine/tb_asnmth.c',
          '<(boringssl_root)/crypto/engine/tb_cipher.c',
          '<(boringssl_root)/crypto/engine/tb_dh.c',
          '<(boringssl_root)/crypto/engine/tb_digest.c',
          '<(boringssl_root)/crypto/engine/tb_dsa.c',
          '<(boringssl_root)/crypto/engine/tb_ecdh.c',
          '<(boringssl_root)/crypto/engine/tb_ecdsa.c',
          '<(boringssl_root)/crypto/engine/tb_pkmeth.c',
          '<(boringssl_root)/crypto/engine/tb_rand.c',
          '<(boringssl_root)/crypto/engine/tb_rsa.c',
          '<(boringssl_root)/crypto/engine/tb_store.c',
          '<(boringssl_root)/crypto/rc2/rc2_cbc.c',
          '<(boringssl_root)/crypto/rc2/rc2_ecb.c',
          '<(boringssl_root)/crypto/rc2/rc2_skey.c',
          '<(boringssl_root)/crypto/rc2/rc2cfb64.c',
          '<(boringssl_root)/crypto/rc2/rc2ofb64.c',
          '<(boringssl_root)/crypto/rc4/rc4_enc.c',
          '<(boringssl_root)/crypto/rc4/rc4_skey.c',
          '<(boringssl_root)/crypto/rc4/rc4_utl.c',
          '<(boringssl_root)/crypto/ripemd/rmd_dgst.c',
          '<(boringssl_root)/crypto/ripemd/rmd_one.c',
          # Removed during Starboard port, found or made to be unneeded at
          # link time.
          '<(boringssl_root)/crypto/bio/b_sock.c',
          '<(boringssl_root)/crypto/bio/bss_acpt.c',
          '<(boringssl_root)/crypto/bio/bss_conn.c',
          '<(boringssl_root)/crypto/bio/bss_dgram.c',
          '<(boringssl_root)/crypto/bio/bss_fd.c',
          '<(boringssl_root)/crypto/bio/bss_log.c',
          '<(boringssl_root)/crypto/bio/bss_sock.c',
          '<(boringssl_root)/crypto/comp/c_rle.c',
          '<(boringssl_root)/crypto/comp/c_zlib.c',
          '<(boringssl_root)/crypto/comp/comp_err.c',
          '<(boringssl_root)/crypto/comp/comp_lib.c',
          '<(boringssl_root)/crypto/des/read2pwd.c',
          '<(boringssl_root)/crypto/dso/dso_dl.c',
          '<(boringssl_root)/crypto/dso/dso_dlfcn.c',
          '<(boringssl_root)/crypto/dso/dso_err.c',
          '<(boringssl_root)/crypto/dso/dso_lib.c',
          '<(boringssl_root)/crypto/dso/dso_null.c',
          '<(boringssl_root)/crypto/dso/dso_openssl.c',
          '<(boringssl_root)/crypto/ebcdic.c',
          '<(boringssl_root)/crypto/evp/e_bf.c',
          '<(boringssl_root)/crypto/evp/e_old.c',
          '<(boringssl_root)/crypto/evp/e_rc2.c',
          '<(boringssl_root)/crypto/evp/e_rc4.c',
          '<(boringssl_root)/crypto/evp/e_rc4_hmac_md5.c',
          '<(boringssl_root)/crypto/evp/e_rc5.c',
          '<(boringssl_root)/crypto/evp/e_rc5.c',
          '<(boringssl_root)/crypto/evp/m_md4.c',
          '<(boringssl_root)/crypto/evp/m_mdc2.c',
          '<(boringssl_root)/crypto/evp/m_ripemd.c',
          '<(boringssl_root)/crypto/evp/m_wp.c',
          '<(boringssl_root)/crypto/krb5/krb5_asn.c',
          '<(boringssl_root)/crypto/md4/md4_dgst.c',
          '<(boringssl_root)/crypto/md4/md4_one.c',
          '<(boringssl_root)/crypto/o_str.c',
          '<(boringssl_root)/crypto/pqueue/pqueue.c',
          '<(boringssl_root)/crypto/rand/rand_egd.c',
          '<(boringssl_root)/crypto/rand/randfile.c',
          '<(boringssl_root)/crypto/srp/srp_lib.c',
          '<(boringssl_root)/crypto/srp/srp_vfy.c',
          '<(boringssl_root)/crypto/ui/ui_compat.c',
          '<(boringssl_root)/crypto/ui/ui_err.c',
          '<(boringssl_root)/crypto/ui/ui_lib.c',
          '<(boringssl_root)/crypto/ui/ui_openssl.c',
          '<(boringssl_root)/crypto/ui/ui_util.c',
          '<(boringssl_root)/ssl/d1_both.c',
          '<(boringssl_root)/ssl/d1_clnt.c',
          '<(boringssl_root)/ssl/d1_enc.c',
          '<(boringssl_root)/ssl/d1_lib.c',
          '<(boringssl_root)/ssl/d1_meth.c',
          '<(boringssl_root)/ssl/d1_pkt.c',
          '<(boringssl_root)/ssl/kssl.c',
          '<(boringssl_root)/ssl/d1_srvr.c',
        ],
      }],  # OS == "lb_shell" or OS == "starboard"
    ],
    'include_dirs': [
      '.',
      '<@(openssl_config_path)',
    ],
    'all_dependent_settings': {
      'include_dirs': [
        '<@(openssl_config_path)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'crypto',

      # Include the source file list generated by BoringSSL
      'includes': ['boringssl.gypi',],

      'sources': [
        '<@(boringssl_crypto_files)',
        '<@(boringssl_ssl_files)',
      ],
      'include_dirs': [
        '<(boringssl_root)/include',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(boringssl_root)/include',
        ],
      },

      'conditions': [
        # This massive, nested conditional block will select the correct batch
        # of assembly language files for the current OS and CPU architecture, or
        # it will turn off assembly language files entirely if the
        # |asm_target_arch| has been set to "none".
        ['target_os not in ["linux", "android", "tvos"] or asm_target_arch not in ["x86", "x64", "arm", "arm64"]', {
          # please read comments for |target_os=="win"| condition below
          'defines': [
            'OPENSSL_NO_ASM',
          ],
        }, {  # target_os in ["linux", "android", "tvos"] and asm_target_arch in ["none", "mips", "ia32"]
          'conditions': [
            ['target_os=="linux" or target_os=="android"', {
              'conditions': [
                ['asm_target_arch=="x86"', {
                  'sources': [
                    '<@(boringssl_linux_x86_files)',
                  ],
                }],
                ['asm_target_arch=="x64"', {
                  'sources': [
                    '<@(boringssl_linux_x86_64_files)',
                  ],
                }],
                ['asm_target_arch=="arm"', {
                  'sources': [
                    '<@(boringssl_linux_arm_files)',
                  ],
                }],
                ['asm_target_arch=="arm64"', {
                  'sources': [
                    '<@(boringssl_linux_aarch64_files)',
                  ],
                }],
              ],
            }],
            ['target_os=="win"', {

            # For |target_os=="win"| an ASM rule uses ml.exe (aka MS Macro Assembler)
            # which we can't use with GNU format. As soon as ASM rule will be fixed
            # for using GNU Assembler, to accelerate "crypto" with assembler 
            # implementations just remove target_os=="win" from abowe condition 
            # |'asm_target_arch=="none" or target_os=="win"'| which currently
            # sets OPENSSL_NO_ASM for "win" too

              'conditions': [
                ['asm_target_arch=="x86"', {
                  'sources': [
                    '<@(boringssl_win_x86_files)',
                  ],
                }],
                ['asm_target_arch=="x64"', {
                  'sources': [
                    '<@(boringssl_win_x86_64_files)',
                  ],
                }],
              ],
            }],
            ['target_os=="tvos"', {
              'conditions': [
                ['asm_target_arch=="arm64"', {
                  'sources': [
                    '<@(boringssl_ios_aarch64_files)',
                  ],
                }],
                # Simulator
                ['asm_target_arch=="x64"', {
                  'defines': [
                    'OPENSSL_NO_ASM',
                  ],
                }],
              ],
            }]
          ],
        }],
      ],
    },
  ],
}
