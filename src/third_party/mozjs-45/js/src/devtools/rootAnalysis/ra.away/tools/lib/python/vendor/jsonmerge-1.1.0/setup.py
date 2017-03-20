#!/usr/bin/python
# vim:ts=4 sw=4 expandtab softtabstop=4

from setuptools import setup

setup(name='jsonmerge',
    version='1.1.0',
    description='Merge a series of JSON documents.',
    license='MIT',
    long_description=open("README.rst").read(),
    author='Tomaz Solc',
    author_email='tomaz.solc@tablix.org',
    packages = [ 'jsonmerge' ],
    install_requires = [ 'jsonschema' ],
    test_suite = 'tests',
    classifiers = [
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
        "Intended Audience :: Developers",
    ],
)
