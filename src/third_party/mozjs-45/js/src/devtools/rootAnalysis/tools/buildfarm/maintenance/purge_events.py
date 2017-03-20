#!/usr/bin/python
"""Run in a buildbot master directory once the master is shut down to
purge old events from the builder files.

NB: The master must be shut down for this to work!"""
import cPickle
import os
import shutil

for f in os.listdir("."):
    builder_file = os.path.join(f, "builder")
    if os.path.isdir(f) and os.path.exists(builder_file):
        builder = cPickle.load(open(builder_file))
        if builder.category == 'release':
            print "Skipping", builder_file
            continue
        print "Backing up", builder_file
        shutil.copyfile(builder_file, builder_file + ".bak")

        # Set some dummy attributes that get deleted by __getstate__
        builder.currentBigState = None
        builder.basedir = None
        builder.status = None
        builder.nextBuildNumber = None

        # Truncate to 500 events
        builder.events = builder.events[-500:]
        print "Writing", builder_file
        cPickle.dump(builder, open(builder_file, "w"))
