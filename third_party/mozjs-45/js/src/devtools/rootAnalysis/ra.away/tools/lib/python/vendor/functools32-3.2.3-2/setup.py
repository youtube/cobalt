#!/usr/bin/python

import sys
from distutils.core import setup


def main():
    if not (3,) > sys.version_info >= (2, 7):
        sys.stderr.write('This backport is for Python 2.7 only.\n')
        sys.exit(1)

    setup(
      name='functools32',
      version='3.2.3-2',
      description='Backport of the functools module from Python 3.2.3 for use on 2.7 and PyPy.',
      long_description="""
This is a backport of the functools standard library module from
Python 3.2.3 for use on Python 2.7 and PyPy. It includes
new features `lru_cache` (Least-recently-used cache decorator).""",
      license='PSF license',

      maintainer='ENDOH takanao',
      maintainer_email='djmchl@gmail.com',
      url='https://github.com/MiCHiLU/python-functools32',

      packages=['functools32'],
    )


if __name__ == '__main__':
    main()
