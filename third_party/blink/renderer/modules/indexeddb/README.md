# IndexedDB

IndexedDB is a browser storage mechanism that can efficiently store and retrieve
large amounts of structured data with a subset of the
[ACID](https://en.wikipedia.org/wiki/ACID) guarantees that are generally
associated with relational databases. IndexedDB
[enjoys wide cross-browser adoption](https://caniuse.com/#feat=indexeddb).

The [IndexedDB specification](https://w3c.github.io/IndexedDB/) is
[maintained on GitHub](https://github.com/w3c/IndexedDB/). The specification's
[issue tracker](https://github.com/w3c/IndexedDB/issues/) is the recommended
forum for discussing new feature proposals and changes that would apply to all
browsers implementing IndexedDB.

Mozilla's [IndexedDB documentation](https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API)
is a solid introduction to IndexedDB for Web developers. Learning the IndexedDB
concepts is also a good first step in understanding Blink's IndexedDB
implementation.

## Documentation

Please add documents below as you write it.

* [Overview for Database People](/third_party/blink/renderer/modules/indexeddb/docs/idb_overview.md)
* [IndexedDB Data Path](/third_party/blink/renderer/modules/indexeddb/docs/idb_data_path.md)

## Design Docs

Please complete the list below with new or existing design docs.

* [Handling Large Values in IndexedDB](https://docs.google.com/document/d/1wmbLb91Se4OIp3Z0eKkAJEHG4YHgq75WUH2mqazrnik/)
* [IndexedDB Tombstone Sweeper](https://docs.google.com/document/d/1BWy0aT_hWrmc3umCxas6-7ofDmT8CSgK4sv1s4VwTeA/)
* [LevelDB Scopes: Special Transactions for IndexedDB](https://docs.google.com/document/d/16_igCI15Gfzb6UYqeuJTmJPrzEtawz6Y1tVOKNtYgiU/)
* [IndexedDB: Onion Soup](https://docs.google.com/document/d/12nwW3mLxVBximpIt9IS0h7hoaB5fcIAl4zHdhuOVKLg/)

## Obsoleted Design Docs

These documents are no longer current, but are still the best documentation we
have in their area.

* [Blob Storage in IndexedDB](https://docs.google.com/document/d/1Kdr4pcFt4QBDLLQn-fY4kZgw6ptmK23lthGZdQMVh2Y/)