# MongoDB Extension with a Cooperative Runtime

- This repository owns a complete source distribution of the upstream PHP driver and its bundled libmongoc, libbson, libmongocrypt and required dependencies. Preserve upstream versions, attribution and license files. Do not copy these libraries into Swow.
- Write source and test comments in English, following the surrounding upstream style. Preserve upstream comments, error handling and protocol behavior.
- `php_mongodb_runtime.h` is the public, versioned C ABI. Consumers may redistribute this small header. Change its ABI version when changing its released layout or contracts, and update the consumer header to match. Do not make consumers depend on private phongo object layouts.
- The PHP driver identifies and protects the actual native client. Preserve persistence keys and shared-client identity; do not create a per-coroutine replacement client or connection pool. Destructors must not bypass ownership protection.
- Without an active provider, original waits and PHP behavior must remain intact. Zend, BSON callbacks, monitoring and driver protocol execution stay on the PHP thread. Only standalone native DNS calls may enter worker threads; their inputs and outputs must outlive cancellation safely.
- Runtime hooks apply to the bundled modified C driver. A stock system libmongoc has no such integration; do not silently advertise cooperative operation when linking it.
- Keep the official PHPT suite intact. Use `MONGODB_URI` and `MONGODB_DATABASE` for isolated test servers. Concurrency checks must observe real pending database commands after warming transports, rather than merely observing DNS suspension or client-side sleeps.
- Distribute expanded dependency sources through the existing PIE pre-packaged-source convention, with `mongodb` as the native module name. Do not load this module alongside another binary providing the same module.
