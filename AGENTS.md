# MongoDB Extension with a Cooperative Runtime

- This repository owns a complete source distribution of the upstream PHP driver and its bundled libmongoc, libbson, libmongocrypt and required dependencies. Preserve upstream versions, attribution and license files. Do not copy these libraries into Swow.
- Write source and test comments in English, following the surrounding upstream style. Preserve upstream comments, error handling and protocol behavior.
- `php_mongodb_runtime.h` is the public, versioned C ABI. Consumers may redistribute this small header. Change its ABI version when changing its released layout or contracts, and update the consumer header to match. Do not make consumers depend on private phongo object layouts.
- The PHP driver identifies and protects the actual native client. Preserve persistence keys and shared-client identity; do not create a per-coroutine replacement client or connection pool. Destructors must not bypass ownership protection.
- Without an active provider, original waits and PHP behavior must remain intact. Zend, BSON callbacks, monitoring and driver protocol execution stay on the PHP thread. Only standalone native DNS calls may enter worker threads; their inputs and outputs must outlive cancellation safely.
- Runtime hooks apply to the bundled modified C driver. A stock system libmongoc has no such integration; do not silently advertise cooperative operation when linking it.
- Keep the official PHPT suite intact. Use `MONGODB_URI` and `MONGODB_DATABASE` for isolated test servers. Concurrency checks must observe real pending database commands after warming transports, rather than merely observing DNS suspension or client-side sleeps.
- Distribute expanded dependency sources through standard PIE source downloads and complete release assets, with `mongodb` as the native module name. Do not load this module alongside another binary providing the same module.

## Repository and Release Conventions

- Follow Swow's repository conventions: English comments and documentation, imperative English commit titles without Conventional Commit prefixes, and linear Git history without merge commits.
- `ci` is the development branch and `develop` is the release branch. Keep `ci` based on `develop`; move release changes forward only after the relevant CI results are available.
- `tools/versions.php` is the version source of truth. Generate `phongo_version.h` with `composer update-version`; do not hand-edit generated version constants. Record the upstream version separately from this distribution's version.
- New PHP tooling uses strict types and the published `swow/swow-php-cs-fixer-config`. Run `composer cs-fix` before committing. Keep imported source and PHPT formatting intact; do not reformat upstream files as part of tooling changes.
- Use the standard PHP extension build process and Swow's existing Windows builder. Do not introduce a custom Composer plugin or replace the PHP MongoDB API.
- GitHub Actions must build and execute tests on Linux, macOS and Windows with a real isolated MongoDB server. Preserve logs and report skipped/failed tests honestly. Existing upstream tests run without Swow, and the existing Swow concurrency tests run with the adapter enabled.
- Create PIE source assets with `composer package`. Release tags must match the generated extension version. Keep full dependency sources and license notices in the asset; exclude local plans, agent instructions, CI configuration and build products.
- Publishing tags, assets, Packagist updates or either repository's branches requires authorization for the actual remote operation. Never publish local plans or credentials.
