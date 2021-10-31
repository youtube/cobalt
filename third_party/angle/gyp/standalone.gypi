# Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'variables':
    {
        # Chrome OS chroot builds need a special pkg-config, so make it possible to change.
        'pkg-config%': 'pkg-config',

        # Use a nested variable trick to get use_x11 evaluated more
        # eagerly than other conditional variables.
        'variables':
        {
            'variables':
            {
                'use_ozone%': 0,
            },

            # Copy conditionally-set variables out one scope.
            'use_ozone%': '<(use_ozone)',
        },
    },
}
