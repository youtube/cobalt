The files in this directory are inputs to filter_fuzz_stub.  They represent
serialized Skia filters that have been found to cause bugs/security issues
in the past.  Each test input file has the format, "repro-XXX.fil" where
XXX is a bug ID associated with the test case, and can be looked up by
navigating to https://bugs.chromium.org/p/chromium/issues/detail?id=XXX .

Note that some tests are pre-pended with "oom-" (such as "oom-repro-448423.fil"
and "oom-repro-465455.fil").  These tests are known to crash (in a controlled
manner) from lack of memory while trying to make huge allocations.
