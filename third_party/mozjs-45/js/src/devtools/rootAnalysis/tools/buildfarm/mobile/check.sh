#!/bin/sh
cd /builds

if [ -z $1 ] ; then
  python sut_tools/check.py
else
  python sut_tools/check.py $*
fi

