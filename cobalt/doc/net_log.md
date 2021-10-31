# Cobalt NetLog

Chromium has a very useful network diagnostic tool called the NetLog and Cobalt
is hooked up to use it. It's the main tool to track network traffic and debug
network code.

### Activate the NetLog

The following command line switch will activate the NetLog and store net log
record to the specified location.
`./cobalt --net_log=/PATH/TO/YOUR_NETLOG_NAME.json`
The output json file will be stored at the file location you choose.


### Read the NetLog records

The produced json file is not human-friendly, use the
[NetLog Viewer](https://netlog-viewer.appspot.com/#import)

Cobalt's net_log can not enable some features in the web viewer, but all the
network traffic is recorded in the event tab.


### Add NetLog entries

To Add NetLog entry, get the NetLog instance owned by NetworkModule to where you
want to add entries and start/end your entry according to the NetLog interface.

A NetLog object is created at each NetworkModule initialization and is passed
into Chromium net through URLRequestContext.
