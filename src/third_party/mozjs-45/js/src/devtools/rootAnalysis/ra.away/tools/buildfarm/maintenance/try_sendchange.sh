#!/bin/sh
unset PYTHONHOME
/tools/buildbot-0.8.0/bin/python ~/tools/buildfarm/maintenance/try_sendchange.py "$@"