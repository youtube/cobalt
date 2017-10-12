Framework for webdriver-driven benchmarks.

Please read tests/README.md, then return here.


# Automating Webdriver Benchmark tests

In addition to running the tests on the command line, we have also included a
way to automate them via integration with the Starboard porting layer.  The
entry point to this automation system is the WebdriverBenchmarkTest class in
"./test.py".  This class, when instantiated, can run webdriver benchmark tests via
its Run() method.  The TEST_RESULT lines that are printed to stdout, mentioned
in "tests/README.md", are all collected and printed at the end of the test run,
aggregating results in a convenient location.  To create an instance of this
class, provide the constructor with the following:

    1.) The test's name.  This is simply the name of the script that you
        would execute on the command line, with the ".py" stripped out.
        So to execute "tests/performance.py", you would simply pass in
        "performance"
    2.  The URL that the tests should be run against.
    3.) The platform name.  This is the name of the Starboard platform that will
        be running the tests, e.g. "linux-x64x11"
    4.) The build configuration.  This is the build type that the tests will
        be run against, e.g. "devel"
    5.) The device identifier.  This is the unique identifier that will be
        used to determine the specific device that tests will run on. Depending
        on the platform, this could be and IP address, plain string name, etc.
    6.) The type of webdriver benchmark test to run.  These can be found in
        "src/cobalt/tools/webdriver_benchmark_config.py".  The options are:
        a.) SANITY
        b.) DEFAULT

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
2.) GetWebdriverBenchmarksTargetParams(): This method should return a list of
    all command line parameters that you want to pass to the Cobalt executable.
    The members should be constants located within
    "../tools/webdriver_benchmark_config.py"
3.) GetWebdriverBenchmarksParams: This method should return a list of command
    line parameters to pass to the webdriver benchmark script.  The members
    should be constants from the same file as #2.

All of these methods have defaults, contained within the example file named
above ("starboard.py").  To implement them for a platform, override them in
the platform's PlatformConfig class.