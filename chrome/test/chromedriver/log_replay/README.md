# ChromeDriver Log-Replay component
This directory contains a component of ChromeDriver that allows developers to
"replay" ChromeDriver sessions using a ChromeDriver log file.

## Sample Usage
For full usage information, run `./client_replay.py --help`.

In general, this component is used by generating a ChromeDriver log from a
session of interest (or getting the log from an bug report, etc.), and then
replaying that log using this component. For example, to replay ChromeDriver
tests from chromedriver/test/run_py_tests.py:
```
# Generate the log
test/run_py_tests.py --log-path=<desired path to log> --replayable=true
# Replay the log
./client_replay.py <chromedriver binary> <path to log> --devtools-replay=true
```
For more usage information, run `./client_replay.py --help`.

## Project Information
Design: https://docs.google.com/a/google.com/document/d/e/2PACX-1vRsp_DhC797flAmzLFaX6Ly4Ro5I9wL-Xk_HH0Zibl6abQ3XTvIypZgdMcBn3hw1FhEoizOhK1RqJpA/pub

## Architecture
The code has two main components: the client-side replay written in Python and
the DevTools-side replay written in C++.

### Client-side Replay
The client-side replay module is written in Python and exists separately from
the ChromeDriver binary. It re-uses the same logic to launch the ChromeDriver
binary as test/run_py_tests.py (this code is in the chromedriver/server
directory).

The entry point for the client-side replay is client_replay.py. This
file contains code that parses client-side commands out of the log file and
interprets them back into the corresponding WebDriver requests. When there are
session, window, or element IDs in the log, the CommandSequence class
substitutes them to match the new session so that the logged commands have
valid references to sessions, windows, and elements.

This entry point also launches ChromeDriver (which in turn launches Chrome),
and has a programmatic interface for retrieving commands from a log file using
the CommandSequence class. The command-line interface uses the Replayer class
to interface with a CommandSequence.

### DevTools-side Replay
The DevTools-side replay module is written in C++ and is built as part of
ChromeDriver binary, enabled by running ChromeDriver (or client_replay.py) with
the `--devtools-replay=true` flag.

DevToolsLogReader (in devtools_log_reader.h) parses out DevTools commands,
responses, and events out of the log file. Depending on the DevTools command
type (HTTP or WebSocket), the responses are read directly into ChromeDriver by
either the ReplayHTTPClient or the LogReplaySocket classes.

ReplayHTTPClient subclasses DevToolsHTTPClient, which is responsible for all of
the HTTP communication between ChromeDriver and DevTools, and it overrides one
method to communicate with the DevToolsLogReader instead of the actual Chrome
instance. LogReplaySocket subclasses SyncWebSocket (see chromedriver/net/) and
is used to redirect WebSocket communication from DevToolsClientImpl to the
DevToolsLogReader instead of getting the WebSocket events and responses from
Chrome.

Between these two classes, all of the communication between ChromeDriver and
Chrome is replicated, so when DevTools replay is enabled, no Chrome instance
even needs to be launched. Note that the DevTools replay component can be run
without running the client-side replay, but this doesn't make much sense as the
same client-side commands would have to be called in the same order as in the
logged session to replicate the same DevTools commands, and at that point you
may as well replay the client side as well as the DevTools side.

### Tests
#### Unit tests
The C++ unit tests for devtools_log_reader.cc are integrated into the
chromedriver_unittests target. Python unit tests for the client-replay are
standalone in the file client_replay_unittest.py but are integrated into the
Chromium CQ.

#### End-to-end tests
client_replay_tests.py runs the end-to-end tests for both the client side and
DevTools side (with the --devtools-replay=true flag) replay functions.

## Maintenance
There are a few places in this directory that need to be maintained based on
external code. On changes in command names, the `_COMMANDS` list in
client_replay.py will need to be updated (see
https://crbug.com/chromedriver/2511 ).

When there is significant changes to the format of logging (at spots marked with
comments in the ChromeDriver code), the log reading will need to be updated
accordingly, on both the DevTools side (devtools_log_reader.cc), and the client
side (`_Parser` class in client_replay.py).

If log formats change, it may be necessary to update the unit test data files
(in test_data/) to match the new logging format. These are small sample log
entries used only for unit tests; the end-to-end tests auto-generate the needed
logs.