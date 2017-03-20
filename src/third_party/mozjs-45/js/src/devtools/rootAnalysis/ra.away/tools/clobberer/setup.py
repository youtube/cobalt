#!/usr/bin/env python

from distutils.core import setup

setup(name='clobberer',
      version='2.0',
      description='Build machine cleanup utility',
      author='Mozilla Release Engineering',
      author_email='release@mozilla.com',
      scripts=['clobberer.py'],
      )
