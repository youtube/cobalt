Cobalt Webdriver-driven Benchmarks
---------------------

This directory contains a set of webdriver-driven benchmarks
for Cobalt.

Each file should contain a set of tests in Python "unittest" format.

All tests in all of the files included within "all.py" will be run on the
build system. Results can be recorded in the build results database.

To run an individual test, simply execute a script directly (or run
all of them via "all.py"). Platform configuration will be inferred from
the environment if set. Otherwise, it must be specified via commandline
parameters.

To make a new test:

 1. If appropriate, create a new file borrowing the boilerplate from
    an existing simple file, such as "browse_horizontal.py".

 2. Add the file name to the tests added within "all.py", causing it run
    when "all.py" is run.

 3. If this file contains internal names or details, consider adding it
    to the "EXCLUDE.FILES" list.

 4. Use the `record_test_result*` methods in `tv_testcase_util` where
    appropriate.

 5. Results must be added to the build results database schema. See
    the internal "README-Updating-Result-Schema.md" file
