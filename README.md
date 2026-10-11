# Swow MongoDB Extension

[![Tests](https://github.com/swow/swow-mongodb-extension/actions/workflows/tests.yml/badge.svg)](https://github.com/swow/swow-mongodb-extension/actions/workflows/tests.yml)
[![Packagist](https://img.shields.io/packagist/v/swow/swow-mongodb-extension.svg)](https://packagist.org/packages/swow/swow-mongodb-extension)

The MongoDB PHP driver with cooperative I/O hooks for [Swow](https://github.com/swow/swow).
This is a Swow-maintained distribution, not an official MongoDB release. It retains the
`mongodb` module, official PHP API, native sockets/TLS, BSON processing, client persistence
and object ownership. Without an active runtime provider, the driver uses native waits.

## Installation

Use [PIE](https://github.com/php/pie) to install a matched release pair:

```sh
pie install swow/swow-extension swow/swow-mongodb-extension --enable-swow-mongodb
```

This installs two ordinary modules, `swow` and `mongodb`. It does not embed MongoDB into Swow.
Linux and macOS use the usual PHP extension build tools. Windows uses matching release DLLs.
No separate libmongoc, libbson or libmongocrypt installation is required.
This command requires published versions of both packages containing the runtime interface;
the development branch alone is not a stable release.

Replace an existing official `mongodb` binary when installing this distribution: both provide
the same module and must not be loaded together. Stock system libmongoc is not supported
because it does not contain the runtime interface.

Continue using the official high-level PHP library:

```sh
composer require mongodb/mongodb
```

## Concurrency

The hook lets other coroutines execute while MongoDB waits for I/O. Operations on the same
native client are protected and execute serially; independent native clients can overlap.
Managers with identical URI/options may share a persistent native client, as upstream.

Applications needing several concurrent operations should lease independent clients from
their existing framework pool. Pool members can use the official `disableClientPersistence`
driver option and retain their Client objects for reuse. This extension does not implement
a pool, replace PHP classes or change Session/Cursor ownership. A shared Session or Cursor
is still a stateful object, not an independent operation for each coroutine.

## Development and Releases

See [CONTRIBUTING.md](CONTRIBUTING.md). The GitHub Actions workflow builds on Linux, macOS and
Windows, runs the upstream PHPT suite without Swow, and runs the existing Swow MongoDB hook
tests against a real isolated server. Skipped hook tests do not count as successful verification.

The earlier full upstream suite with Swow loaded has a known object-number difference in
`tests/query/bug2457-001.phpt`, also reproduced with unmodified Swow. It is not reported as a pass.
The baseline and hook-specific suite do not prove complete compatibility for every callback
or shared application object.

Repository conventions follow Swow: `ci` is the development branch, `develop` is the release
branch, and commits use imperative English titles with linear history. `tools/versions.php`
defines the distribution version, upstream source and exact Swow integration revision.
`composer prepare-release` generates the extension version header. A matching `v<version>`
tag triggers PIE-compatible source and Windows asset builds.

The source package contains expanded dependencies and license notices. Packaging uses
committed files and excludes CI configuration, local plans, build products and agent instructions.
Integration issues belong in [this repository](https://github.com/swow/swow-mongodb-extension/issues).
Driver API documentation remains on [PHP.net](https://www.php.net/mongodb) and
[MongoDB's documentation](https://www.mongodb.com/docs/drivers/php-drivers/).

## Upstream and License

Based on MongoDB PHP Driver 2.5.4, upstream revision
`7fc68847e9c8972fdf8a2b1672d54d1a2f1af55d`. The imported source package SHA-256 is
`41eead0799cc66876b12d10b5efe86268bfc704c1be57a70d49e4bd4bf6c8ee1`.
Bundled libmongoc/libbson are 2.5.6 and libmongocrypt is 1.20.5.

The driver and Swow changes are distributed under Apache-2.0. Preserve [LICENSE](LICENSE),
[NOTICE](NOTICE), [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES) and individual dependency notices.
Third-party code retains its original license and copyright. See [CREDITS](CREDITS) for upstream contributors.
