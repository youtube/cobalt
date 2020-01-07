"""A redirect for new build code to find the old port_symlink.py location."""

import os

execfile(os.path.join(os.path.dirname(__file__), os.pardir, 'build',
                      'port_symlink.py'))
