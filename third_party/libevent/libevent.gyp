# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_libevent%': 0,
  },
  'conditions': [
    ['use_system_libevent==0', {
      'targets': [
        {
          'target_name': 'libevent',
          'product_name': 'event',
          'type': 'static_library',
          'toolsets': ['host', 'target'],
          'sources': [
            'buffer.c',
            'epoll.c',
            'evbuffer.c',
            'evdns.c',
            'event.c',
            'event_tagging.c',
            'evrpc.c',
            'evutil.c',
            'http.c',
            'kqueue.c',
            'log.c',
            'poll.c',
            'select.c',
            'signal.c',
            'strlcpy.c',
          ],
          'defines': [
            'HAVE_CONFIG_H',
          ],
          'conditions': [
            # Starboard could be any other underlying platform, so we use
            # target_arch to tell the difference. Libevent is itself a platform
            # abstraction library, so Starboard depends on it to implement its
            # SbSocketWaiter API. This means the Libevent is below the Starboard
            # platform abstraction line.
            [ 'OS == "starboard"', {
              'sources!': [
                'buffer.c',
                'evbuffer.c',
                'evdns.c',
                'event_tagging.c',
                'evrpc.c',
                'http.c',
                'select.c',
                'signal.c',
                'strlcpy.c',
              ],
              'include_dirs': [ 'starboard' ],
              'conditions': [
                [ 'sb_libevent_method == "epoll"', {
                  'sources!': [
                    'kqueue.c',
                    'poll.c',
                  ],
                }],
                [ 'sb_libevent_method == "poll"', {
                  'sources!': [
                    'epoll.c',
                    'kqueue.c',
                  ],
                }],
                [ 'sb_libevent_method == "kqueue"', {
                  'sources!': [
                    'epoll.c',
                    'poll.c',
                  ],
                }],
                [ 'target_os == "linux"', {
                  'sources': [ 'epoll_sub.c' ],
                  'include_dirs': [ 'starboard/linux' ],
                  'link_settings': {
                    'libraries': [
                      '-lrt',
                    ],
                  },
                }],
                # TODO: Make this android specific, not a linux copy.
                [ 'target_os == "android"', {
                  'sources': [ 'epoll_sub.c' ],
                  'include_dirs': [ 'starboard/linux' ],
                  }
                ],
              ],
            }],
            # libevent has platform-specific implementation files.  Since its
            # native build uses autoconf, platform-specific config.h files are
            # provided and live in platform-specific directories.
            [ 'OS == "linux" or (OS == "android" and _toolset == "host")', {
              'sources': [ 'epoll.c', 'epoll_sub.c' ],
              'include_dirs': [ 'linux' ],
              'link_settings': {
                'libraries': [
                  # We need rt for clock_gettime().
                  # TODO(port) Maybe on FreeBSD as well?
                  '-lrt',
                ],
              },
            }],
            [ '(OS == "android" or (OS == "lb_shell" and target_arch == "android")) and _toolset == "target"', {
              # On android, epoll_create(), epoll_ctl(), epoll_wait() and
              # clock_gettime() are all in libc.so, so no need to add
              # epoll_sub.c and link librt.
              'sources': [ 'epoll.c' ],
              'include_dirs': [ 'android' ],
            }],
            [ 'OS == "mac" or OS == "ios" or os_bsd==1', {
              'sources': [ 'kqueue.c' ],
              'include_dirs': [ 'mac' ]
            }],
            [ 'OS == "solaris"', {
              'sources': [ 'devpoll.c', 'evport.c' ],
              'include_dirs': [ 'solaris' ]
            }],
          ],
        },
      ],
    }, {  # use_system_libevent != 0
      'targets': [
        {
          'target_name': 'libevent',
          'type': 'none',
          'toolsets': ['host', 'target'],
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_LIBEVENT',
            ],
          },
          'link_settings': {
            'libraries': [
              '-levent',
            ],
          },
        }
      ],
    }],
  ],
}
