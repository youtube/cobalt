#! /usr/bin/env python

from setuptools import setup, find_packages

setup(
    name="buildtools",
    version="1.0.6",
    description="Mozilla RelEng Toolkit",
    author="Release Engineers",
    author_email="release@mozilla.com",

    # python packages are under lib/python.  Note that there are several
    # top-level packages here -- not just a buildtools package

    packages=find_packages("lib/python"),
    package_dir={'': "lib/python"},

    test_suite='mozilla_buildtools.test',

    install_requires=[
        'sqlalchemy',
        'argparse',
        'twisted',
        'simplejson',
        'furl',
        'requests',
        'docopt',
        'python-dateutil',
        'jinja2',
        'redo',
    ],

    entry_points={
        'console_scripts': [
            'slavealloc = slavealloc.scripts.main:main'
        ],
    },

    scripts=["buildfarm/maintenance/reboot-idle-slaves.py"],

    # include files listed in MANIFEST.in
    include_package_data=True,
)
