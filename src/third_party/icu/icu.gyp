# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_icu%': 0,
    'icu_use_data_file_flag%': 0,
    'conditions': [
      # On Windows MSVS's library archive tool won't create an empty
      # library given zero object input. Therefore we set target type as
      # none. On other platforms use static_library.
      ['actual_target_arch=="win"', {
        'icudata_target_type': 'none',
      }, {
        'icudata_target_type': 'static_library',
      }],
    ],
  },
  'target_defaults': {
    'direct_dependent_settings': {
      'defines': [
        # Tell ICU to not insert |using namespace icu;| into its headers,
        # so that chrome's source explicitly has to use |icu::|.
        'U_USING_ICU_NAMESPACE=0',
      ],
    },
  },
  'conditions': [
    ['use_system_icu==0', {
      'target_defaults': {
        'defines': [
          'U_USING_ICU_NAMESPACE=0',
        ],
        'conditions': [
          ['component=="static_library"', {
            'defines': [
              'U_STATIC_IMPLEMENTATION',
            ],
          }],
        ],
        # TODO(mark): Looks like this is causing the "public" include
        # directories to show up twice in some targets in this file.  Fix.
        'include_dirs': [
          'public/common',
          'public/i18n',
          'source/common',
          'source/i18n',
        ],
        'msvs_disabled_warnings': [4005, 4068, 4355, 4996],
      },
      'targets': [
        {
          'target_name': 'icudata',
          'type': '<(icudata_target_type)',
          'sources': [
             # These are hand-generated, but will do for now.  The linux
             # version is an identical copy of the (mac) icudt46l_dat.S file,
             # modulo removal of the .private_extern and .const directives and
             # with no leading underscore on the icudt46_dat symbol.
             'android/icudt46l_dat.S',
             'linux/icudt46l_dat.S',
             'mac/icudt46l_dat.S',
          ],
          'conditions': [
            [ 'OS == "win"', {
              'type': 'none',
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)',
                  'files': [
                    'windows/icudt.dll',
                  ],
                },
              ],
            }],
            [ 'OS == "win" or OS == "mac" or OS == "ios" or OS == "android"', {
              'sources!': ['linux/icudt46l_dat.S'],
            }],
            [ 'OS != "android"', {
              'sources!': ['android/icudt46l_dat.S'],
            }],
            [ 'OS != "mac" and OS != "ios"', {
              'sources!': ['android/icudt46l_dat.S'],
            }],
            [ 'OS=="lb_shell" or OS=="starboard"', {
              # we use the ICU data files instead of any precompiled data
              'sources/': [['exclude', 'icudt46l_dat']],
            }],
            [ 'OS != "win" and icu_use_data_file_flag', {
              # Remove any assembly data file.
              'sources/': [['exclude', 'icudt46l_dat']],
              # Compile in the stub data symbol.
              'sources': ['source/stubdata/stubdata.c'],
              # Make sure any binary depending on this gets the data file.
              'link_settings': {
                'target_conditions': [
                  ['(OS == "mac" and _mac_bundle) or OS=="ios"', {
                    'mac_bundle_resources': [
                      'source/data/in/icudt46l.dat',
                    ],
                  }, {
                    'copies': [{
                      'destination': '<(PRODUCT_DIR)',
                      'files': [
                        'source/data/in/icudt46l.dat',
                      ],
                    }],
                  }],
                ],  # target_conditions
              },  # link_settings
            }],
            [ 'cobalt==1', {
              # TODO: ICU data handling should be unified with
              # the Chromium code.
              'includes': [
                '../../cobalt/build/copy_icu_data.gypi',
              ],
            }],
            [ '_type != "shared_library"', {
              'defines': [
                'U_HIDE_DATA_SYMBOL',
              ],
            }],
          ],
        },
        {
          'target_name': 'icui18n',
          'type': '<(component)',
          'sources': [
            'source/i18n/anytrans.cpp',
            'source/i18n/astro.cpp',
            'source/i18n/basictz.cpp',
            'source/i18n/bms.cpp',
            'source/i18n/bmsearch.cpp',
            'source/i18n/bocsu.c',
            'source/i18n/brktrans.cpp',
            'source/i18n/buddhcal.cpp',
            'source/i18n/calendar.cpp',
            'source/i18n/casetrn.cpp',
            'source/i18n/cecal.cpp',
            'source/i18n/chnsecal.cpp',
            'source/i18n/choicfmt.cpp',
            'source/i18n/coleitr.cpp',
            'source/i18n/coll.cpp',
            'source/i18n/colldata.cpp',
            'source/i18n/coptccal.cpp',
            'source/i18n/cpdtrans.cpp',
            'source/i18n/csdetect.cpp',
            'source/i18n/csmatch.cpp',
            'source/i18n/csr2022.cpp',
            'source/i18n/csrecog.cpp',
            'source/i18n/csrmbcs.cpp',
            'source/i18n/csrsbcs.cpp',
            'source/i18n/csrucode.cpp',
            'source/i18n/csrutf8.cpp',
            'source/i18n/curramt.cpp',
            'source/i18n/currfmt.cpp',
            'source/i18n/currpinf.cpp',
            'source/i18n/currunit.cpp',
            'source/i18n/datefmt.cpp',
            'source/i18n/dcfmtsym.cpp',
            'source/i18n/decContext.c',
            'source/i18n/decNumber.c',
            'source/i18n/decimfmt.cpp',
            'source/i18n/digitlst.cpp',
            'source/i18n/dtfmtsym.cpp',
            'source/i18n/dtitvfmt.cpp',
            'source/i18n/dtitvinf.cpp',
            'source/i18n/dtptngen.cpp',
            'source/i18n/dtrule.cpp',
            'source/i18n/esctrn.cpp',
            'source/i18n/ethpccal.cpp',
            'source/i18n/fmtable.cpp',
            'source/i18n/fmtable_cnv.cpp',
            'source/i18n/format.cpp',
            'source/i18n/fphdlimp.cpp',
            'source/i18n/fpositer.cpp',
            'source/i18n/funcrepl.cpp',
            'source/i18n/gregocal.cpp',
            'source/i18n/gregoimp.cpp',
            'source/i18n/hebrwcal.cpp',
            'source/i18n/indiancal.cpp',
            'source/i18n/inputext.cpp',
            'source/i18n/islamcal.cpp',
            'source/i18n/japancal.cpp',
            'source/i18n/locdspnm.cpp',
            'source/i18n/measfmt.cpp',
            'source/i18n/measure.cpp',
            'source/i18n/msgfmt.cpp',
            'source/i18n/name2uni.cpp',
            'source/i18n/nfrs.cpp',
            'source/i18n/nfrule.cpp',
            'source/i18n/nfsubs.cpp',
            'source/i18n/nortrans.cpp',
            'source/i18n/nultrans.cpp',
            'source/i18n/numfmt.cpp',
            'source/i18n/numsys.cpp',
            'source/i18n/olsontz.cpp',
            'source/i18n/persncal.cpp',
            'source/i18n/plurfmt.cpp',
            'source/i18n/plurrule.cpp',
            'source/i18n/quant.cpp',
            'source/i18n/rbnf.cpp',
            'source/i18n/rbt.cpp',
            'source/i18n/rbt_data.cpp',
            'source/i18n/rbt_pars.cpp',
            'source/i18n/rbt_rule.cpp',
            'source/i18n/rbt_set.cpp',
            'source/i18n/rbtz.cpp',
            'source/i18n/regexcmp.cpp',
            'source/i18n/regexst.cpp',
            'source/i18n/regextxt.cpp',
            'source/i18n/reldtfmt.cpp',
            'source/i18n/rematch.cpp',
            'source/i18n/remtrans.cpp',
            'source/i18n/repattrn.cpp',
            'source/i18n/search.cpp',
            'source/i18n/selfmt.cpp',
            'source/i18n/simpletz.cpp',
            'source/i18n/smpdtfmt.cpp',
            'source/i18n/sortkey.cpp',
            'source/i18n/strmatch.cpp',
            'source/i18n/strrepl.cpp',
            'source/i18n/stsearch.cpp',
            'source/i18n/taiwncal.cpp',
            'source/i18n/tblcoll.cpp',
            'source/i18n/timezone.cpp',
            'source/i18n/titletrn.cpp',
            'source/i18n/tmunit.cpp',
            'source/i18n/tmutamt.cpp',
            'source/i18n/tmutfmt.cpp',
            'source/i18n/tolowtrn.cpp',
            'source/i18n/toupptrn.cpp',
            'source/i18n/translit.cpp',
            'source/i18n/transreg.cpp',
            'source/i18n/tridpars.cpp',
            'source/i18n/tzrule.cpp',
            'source/i18n/tztrans.cpp',
            'source/i18n/ucal.cpp',
            'source/i18n/ucln_in.c',
            'source/i18n/ucol.cpp',
            'source/i18n/ucol_bld.cpp',
            'source/i18n/ucol_cnt.cpp',
            'source/i18n/ucol_elm.cpp',
            'source/i18n/ucol_res.cpp',
            'source/i18n/ucol_sit.cpp',
            'source/i18n/ucol_tok.cpp',
            'source/i18n/ucol_wgt.cpp',
            'source/i18n/ucoleitr.cpp',
            'source/i18n/ucsdet.cpp',
            'source/i18n/ucurr.cpp',
            'source/i18n/udat.cpp',
            'source/i18n/udatpg.cpp',
            'source/i18n/ulocdata.c',
            'source/i18n/umsg.cpp',
            'source/i18n/unesctrn.cpp',
            'source/i18n/uni2name.cpp',
            'source/i18n/unum.cpp',
            'source/i18n/uregex.cpp',
            'source/i18n/uregexc.cpp',
            'source/i18n/usearch.cpp',
            'source/i18n/uspoof.cpp',
            'source/i18n/uspoof_build.cpp',
            'source/i18n/uspoof_conf.cpp',
            'source/i18n/uspoof_impl.cpp',
            'source/i18n/uspoof_wsconf.cpp',
            'source/i18n/utmscale.c',
            'source/i18n/utrans.cpp',
            'source/i18n/vtzone.cpp',
            'source/i18n/vzone.cpp',
            'source/i18n/windtfmt.cpp',
            'source/i18n/winnmfmt.cpp',
            'source/i18n/wintzimpl.cpp',
            'source/i18n/zonemeta.cpp',
            'source/i18n/zrule.cpp',
            'source/i18n/zstrfmt.cpp',
            'source/i18n/ztrans.cpp',
          ],
          'defines': [
            'U_I18N_IMPLEMENTATION',
          ],
          'dependencies': [
            'icuuc',
          ],
          'direct_dependent_settings': {
            # Use prepend (+) because the WebKit build needs to pick up these
            # ICU headers instead of the ones in ../WebKit/JavaScriptCore/wtf,
            # also in WebKit's include path.
            # TODO(mark): The triple + is a bug.  It should be a double +.  It
            # seems that a + is being chopped off when the "target" section is
            # merged into "target_defaults".
            'include_dirs+++': [
              'public/i18n',
            ],
          },
          'conditions': [
            [ 'os_posix == 1 and OS != "mac" and OS != "ios" and OS != "lb_shell"', {
              # Since ICU wants to internally use its own deprecated APIs, don't
              # complain about it.
              'cflags': [
                '-Wno-deprecated-declarations',
              ],
              'cflags_cc': [
                '-frtti',
              ],
            }],
            ['OS == "mac" or OS == "ios"', {
              'xcode_settings': {
                'GCC_ENABLE_CPP_RTTI': 'YES',       # -frtti
              },
            }],
            ['OS == "win"', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'RuntimeTypeInfo': 'true',
                },
              }
            }],
            ['(OS == "lb_shell" or OS=="starboard") and target_arch == "ps3"', {
              'cflags_cc': [
                '-Xc+=rtti',
              ],
            }],
            ['clang==1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  # ICU uses its own deprecated functions.
                  '-Wno-deprecated-declarations',
                  # ICU prefers `a && b || c` over `(a && b) || c`.
                  '-Wno-logical-op-parentheses',
                  # ICU has some `unsigned < 0` checks.
                  '-Wno-tautological-compare',
                  # uspoof.h has a U_NAMESPACE_USE macro. That's a bug,
                  # the header should use U_NAMESPACE_BEGIN instead.
                  # http://bugs.icu-project.org/trac/ticket/9054
                  '-Wno-header-hygiene',
                  # Looks like a real issue, see http://crbug.com/114660
                  '-Wno-return-type-c-linkage',
                ],
              },
              'cflags': [
                '-Wno-deprecated-declarations',
                '-Wno-logical-op-parentheses',
                '-Wno-tautological-compare',
                '-Wno-header-hygiene',
                '-Wno-return-type-c-linkage',
              ],
            }],
            ['OS == "android" and use_system_stlport == 1', {
              # ICU requires RTTI, which is not present in the system's stlport,
              # so we have to include gabi++.
              'include_dirs': [
                '<(android_src)/abi/cpp/include',
              ],
              'link_settings': {
                'libraries': [
                  '-lgabi++',
                ],
              },
            }],
            ['OS=="lb_shell"', {
              'dependencies': [
                '<(lbshell_root)/build/projects/posix_emulation.gyp:posix_emulation',
              ],
            }],
            ['OS=="starboard"', {
              'dependencies': [
                '<(DEPTH)/starboard/starboard.gyp:starboard',
               ],
            }],
            ['(OS=="lb_shell" or OS=="starboard") and target_arch=="android"', {
              'cflags_cc': [
                '-frtti',
              ],
              'link_settings': {
                'libraries': [
                  '-lgabi++_static',
                ],
              },
            }],
            ['(OS=="lb_shell" or OS=="starboard") and (target_os=="linux" or clang==1)', {
              'cflags_cc': [
                '-frtti',
              ],
              'cflags_cc!': [
                '-fno-rtti',
              ],
            }],
          ],
        },
        {
          'target_name': 'icuuc',
          'type': '<(component)',
          'sources': [
            'source/common/bmpset.cpp',
            'source/common/brkeng.cpp',
            'source/common/brkiter.cpp',
            'source/common/bytestream.cpp',
            'source/common/caniter.cpp',
            'source/common/chariter.cpp',
            'source/common/charstr.cpp',
            'source/common/cmemory.c',
            'source/common/cstring.c',
            'source/common/cwchar.c',
            'source/common/dictbe.cpp',
            'source/common/dtintrv.cpp',
            'source/common/errorcode.cpp',
            'source/common/filterednormalizer2.cpp',
            'source/common/icudataver.c',
            'source/common/icuplug.c',
            'source/common/locavailable.cpp',
            'source/common/locbased.cpp',
            'source/common/locdispnames.cpp',
            'source/common/locid.cpp',
            'source/common/loclikely.cpp',
            'source/common/locmap.c',
            'source/common/locresdata.cpp',
            'source/common/locutil.cpp',
            'source/common/mutex.cpp',
            'source/common/normalizer2.cpp',
            'source/common/normalizer2impl.cpp',
            'source/common/normlzr.cpp',
            'source/common/parsepos.cpp',
            'source/common/propname.cpp',
            'source/common/propsvec.c',
            'source/common/punycode.c',
            'source/common/putil.c',
            'source/common/rbbi.cpp',
            'source/common/rbbidata.cpp',
            'source/common/rbbinode.cpp',
            'source/common/rbbirb.cpp',
            'source/common/rbbiscan.cpp',
            'source/common/rbbisetb.cpp',
            'source/common/rbbistbl.cpp',
            'source/common/rbbitblb.cpp',
            'source/common/resbund.cpp',
            'source/common/resbund_cnv.cpp',
            'source/common/ruleiter.cpp',
            'source/common/schriter.cpp',
            'source/common/serv.cpp',
            'source/common/servlk.cpp',
            'source/common/servlkf.cpp',
            'source/common/servls.cpp',
            'source/common/servnotf.cpp',
            'source/common/servrbf.cpp',
            'source/common/servslkf.cpp',
            'source/common/stringpiece.cpp',
            'source/common/triedict.cpp',
            'source/common/uarrsort.c',
            'source/common/ubidi.c',
            'source/common/ubidi_props.c',
            'source/common/ubidiln.c',
            'source/common/ubidiwrt.c',
            'source/common/ubrk.cpp',
            'source/common/ucase.c',
            'source/common/ucasemap.c',
            'source/common/ucat.c',
            'source/common/uchar.c',
            'source/common/uchriter.cpp',
            'source/common/ucln_cmn.c',
            'source/common/ucmndata.c',
            'source/common/ucnv.c',
            'source/common/ucnv2022.c',
            'source/common/ucnv_bld.c',
            'source/common/ucnv_cb.c',
            'source/common/ucnv_cnv.c',
            'source/common/ucnv_err.c',
            'source/common/ucnv_ext.c',
            'source/common/ucnv_io.c',
            'source/common/ucnv_lmb.c',
            'source/common/ucnv_set.c',
            'source/common/ucnv_u16.c',
            'source/common/ucnv_u32.c',
            'source/common/ucnv_u7.c',
            'source/common/ucnv_u8.c',
            'source/common/ucnvbocu.c',
            'source/common/ucnvdisp.c',
            'source/common/ucnvhz.c',
            'source/common/ucnvisci.c',
            'source/common/ucnvlat1.c',
            'source/common/ucnvmbcs.c',
            'source/common/ucnvscsu.c',
            'source/common/ucnvsel.cpp',
            'source/common/ucol_swp.cpp',
            'source/common/udata.cpp',
            'source/common/udatamem.c',
            'source/common/udataswp.c',
            'source/common/uenum.c',
            'source/common/uhash.c',
            'source/common/uhash_us.cpp',
            'source/common/uidna.cpp',
            'source/common/uinit.c',
            'source/common/uinvchar.c',
            'source/common/uiter.cpp',
            'source/common/ulist.c',
            'source/common/uloc.c',
            'source/common/uloc_tag.c',
            'source/common/umapfile.c',
            'source/common/umath.c',
            'source/common/umutex.c',
            'source/common/unames.c',
            'source/common/unifilt.cpp',
            'source/common/unifunct.cpp',
            'source/common/uniset.cpp',
            'source/common/uniset_props.cpp',
            'source/common/unisetspan.cpp',
            'source/common/unistr.cpp',
            'source/common/unistr_case.cpp',
            'source/common/unistr_cnv.cpp',
            'source/common/unistr_props.cpp',
            'source/common/unorm.cpp',
            'source/common/unorm_it.c',
            'source/common/unormcmp.cpp',
            'source/common/uobject.cpp',
            'source/common/uprops.cpp',
            'source/common/ures_cnv.c',
            'source/common/uresbund.c',
            'source/common/uresdata.c',
            'source/common/usc_impl.c',
            'source/common/uscript.c',
            'source/common/uset.cpp',
            'source/common/uset_props.cpp',
            'source/common/usetiter.cpp',
            'source/common/ushape.c',
            'source/common/usprep.cpp',
            'source/common/ustack.cpp',
            'source/common/ustr_cnv.c',
            'source/common/ustr_wcs.c',
            'source/common/ustrcase.c',
            'source/common/ustrenum.cpp',
            'source/common/ustrfmt.c',
            'source/common/ustring.c',
            'source/common/ustrtrns.c',
            'source/common/utext.cpp',
            'source/common/utf_impl.c',
            'source/common/util.cpp',
            'source/common/util_props.cpp',
            'source/common/utrace.c',
            'source/common/utrie.c',
            'source/common/utrie2.cpp',
            'source/common/utrie2_builder.c',
            'source/common/uts46.cpp',
            'source/common/utypes.c',
            'source/common/uvector.cpp',
            'source/common/uvectr32.cpp',
            'source/common/uvectr64.cpp',
            'source/common/wintz.c',
          ],
          'defines': [
            'U_COMMON_IMPLEMENTATION',
          ],
          'dependencies': [
            'icudata',
          ],
          'direct_dependent_settings': {
            # Use prepend (+) because the WebKit build needs to pick up these
            # ICU headers instead of the ones in ../WebKit/JavaScriptCore/wtf,
            # also in WebKit's include path.
            # TODO(mark): The triple + is a bug.  It should be a double +.  It
            # seems that a + is being chopped off when the "target" section is
            # merged into "target_defaults".
            'include_dirs+++': [
              'public/common',
            ],
            'conditions': [
              [ 'component=="static_library"', {
                'defines': [
                  'U_STATIC_IMPLEMENTATION',
                ],
              }],
            ],
          },
          'conditions': [
            [ 'OS == "win"', {
              'sources': [
                'source/stubdata/stubdata.c',
              ],
            }],
            ['OS=="lb_shell" or OS=="starboard"', {
              'defines': [
                'U_HAVE_NL_LANGINFO_CODESET=0',
                'U_HAVE_NL_LANGINFO=0'
              ],
              'sources': [
                'source/stubdata/stubdata.c',
              ],
            }],
            ['OS=="lb_shell"', {
              'dependencies': [
                '<(lbshell_root)/build/projects/posix_emulation.gyp:posix_emulation',
              ],
            }],
            ['OS=="starboard"', {
              'dependencies': [
                '<(DEPTH)/starboard/starboard.gyp:starboard',
               ],
            }],
            [ 'os_posix == 1 and OS != "mac" and OS != "ios" and OS != "lb_shell"', {
              'cflags': [
                # Since ICU wants to internally use its own deprecated APIs,
                # don't complain about it.
                '-Wno-deprecated-declarations',
                '-Wno-unused-function',
              ],
              'cflags_cc': [
                '-frtti',
              ],
            }],
            ['OS == "mac" or OS == "ios"', {
              'xcode_settings': {
                'GCC_ENABLE_CPP_RTTI': 'YES',       # -frtti
              },
            }],
            ['OS == "win"', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'RuntimeTypeInfo': 'true',
                },
              },
            }],
            ['(OS == "lb_shell" or OS=="starboard") and target_arch == "ps3"', {
              'cflags_cc': [
                '-Xc+=rtti',
              ],
            }],
            ['(OS=="lb_shell" or OS=="starboard") and target_arch=="android"', {
              'cflags_cc': [
                '-frtti',
              ],
              'link_settings': {
                'libraries': [
                  '-lgabi++_static',
                ],
              },
            }],
            ['(OS=="lb_shell" or OS=="starboard") and (target_os=="linux" or clang==1)', {
              'cflags_cc': [
                '-frtti',
              ],
              'cflags_cc!': [
                '-fno-rtti',
              ],
            }],
            ['OS == "android" and use_system_stlport == 1', {
              # ICU requires RTTI, which is not present in the system's stlport,
              # so we have to include gabi++.
              'include_dirs': [
                '<(android_src)/abi/cpp/include',
              ],
              'link_settings': {
                'libraries': [
                  '-lgabi++',
                ],
              },
            }],
            ['clang==1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  # ICU uses its own deprecated functions.
                  '-Wno-deprecated-declarations',
                  # ICU prefers `a && b || c` over `(a && b) || c`.
                  '-Wno-logical-op-parentheses',
                  # ICU has some `unsigned < 0` checks.
                  '-Wno-tautological-compare',
                  # uresdata.c has switch(RES_GET_TYPE(x)) code. The
                  # RES_GET_TYPE macro returns an UResType enum, but some switch
                  # statement contains case values that aren't part of that
                  # enum (e.g. URES_TABLE32 which is in UResInternalType). This
                  # is on purpose.
                  '-Wno-switch',
                ],
              },
              'cflags': [
                '-Wno-deprecated-declarations',
                '-Wno-logical-op-parentheses',
                '-Wno-tautological-compare',
                '-Wno-switch',
              ],
            }],
          ],
        },
      ],
    }, { # use_system_icu != 0
      'targets': [
        {
          'target_name': 'system_icu',
          'type': 'none',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_ICU',
            ],
          },
          'conditions': [
            ['OS=="android"', {
              'direct_dependent_settings': {
                'include_dirs': [
                  '<(android_src)/external/icu4c/common',
                  '<(android_src)/external/icu4c/i18n',
                ],
              },
              'link_settings': {
                'libraries': [
                  '-licui18n',
                  '-licuuc',
                ],
              },
            },{ # OS!="android"
              'link_settings': {
                'ldflags': [
                  '<!@(icu-config --ldflags)',
                ],
                'libraries': [
                  '<!@(icu-config --ldflags-libsonly)',
                ],
              },
            }],
          ],
        },
        {
          'target_name': 'icudata',
          'type': 'none',
          'dependencies': ['system_icu'],
          'export_dependent_settings': ['system_icu'],
        },
        {
          'target_name': 'icui18n',
          'type': 'none',
          'dependencies': ['system_icu'],
          'export_dependent_settings': ['system_icu'],
        },
        {
          'target_name': 'icuuc',
          'type': 'none',
          'dependencies': ['system_icu'],
          'export_dependent_settings': ['system_icu'],
        },
      ],
    }],
  ],
}
