Framework for webdriver-driven benchmarks.

Please read youtube/tests/README.md, then return here.


# Automating Webdriver Benchmark tests

In addition to running the tests on the command line, we have also included a
way to automate them via integration with the Starboard porting layer.  The
entry point to this automation system is the WebdriverBenchmarkTest class in
"./test.py".  This class, when instantiated, can run webdriver benchmark tests
via its Run() method.  The TEST_RESULT lines that are printed to stdout,
mentioned in "tests/README.md", are all collected and printed at the end of the
test run, aggregating results in a convenient location.  To create an instance
of this class, provide the constructor with the following:

    1.) The test to run.  This is simply the script, including the relative
        path from this directory, that you would execute on the command line.
        Including the  ".py" is optional. For example, to run performance
        scripts for the main YouTube app, you would simply pass in
        "youtube/tests/performance".
    2.) The platform name.  This is the name of the Starboard platform that will
        be running the tests, e.g. "linux-x64x11"
    3.) The build configuration.  This is the build type that the tests will
        be run against, e.g. "devel"
    4.) The URL that the tests should be run against. This is optional and the
        test will use its default URL if it is not specified.
    5.) The device identifier.  This is the unique identifier that will be
        used to determine the specific device that tests will run on. Depending
        on the platform, this could be and IP address, plain string name, etc.
        This is optional and the test will rely upon the launcher to determine
        the default when it is not specified.
    6.) The sample size to use with the test. The supported values are
        'standard', 'reduced', and 'minimal'. This is optional and the test will
        use the platform's default sample size when it is not specified.


# Configuring Your Platform for Testing

In order to specify additional arguments to the webdriver benchmark tests, such
as the url and a sample size, each Starboard platform that runs the tests should
have its own configuration instructions.  These live in the platform's
PlatformConfig class, within its corresponding "gyp_configuration.py" file.  See
"../build/config/starboard.py" for an example of these methods.

The configuration instructions are spit up into 4 methods:

1.) WebdriverBenchmarksEnabled():  Determines if webdriver benchmarks are
    enabled for a platform.  This method should return True to enable them.
2.) GetDefaultSampleSize():  Determines what the sample size for a regular
    webdriver benchmark test run should be.  It should return a constant
    from the "SIZES" list in "../tools/webdriver_benchmark_config.py".
3.) GetWebdriverBenchmarksTargetParams(): This method should return a list of
    all command line parameters that you want to pass to the Cobalt executable.
    The members should be constants located within
    "../tools/webdriver_benchmark_config.py"
4.) GetWebdriverBenchmarksParams: This method should return a list of command
    line parameters to pass to the webdriver benchmark script.  The members
    should be constants from the same file as #2.

All of these methods have defaults, contained within the example file named
above ("starboard.py").  To implement them for a platform, override them in
the platform's PlatformConfig class.
