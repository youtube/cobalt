# ProtoDataStore C++

A simple file-backed proto with an in-memory cache.

## Features

1. Caches proto in RAM after first read for performance.
1. Uses a checksum to verify integrity of data.
1. Leverages modern [Abseil](https://abseil.io/) error handling and memory management.

## Usage

``` c++
#include "protostore/file-storage.h"
#include "protostore/proto-data-store.h"

FileStorage storage;
ProtoDataStore<TestProto> pds(storage, testfile);

MyProto myproto;
myproto.set_value("hi");
absl::Status result = pds.Write(myproto);
if (!result.ok()) // error handling
```

## Compared to Jetpack DataStore

This library is a C++ implementation from the same origin as [Jetpack DataStore](https://developer.android.com/topic/libraries/architecture/datastore). Unlike that library, it does not support concurrent writes and the on-disk format
is incompatible.
